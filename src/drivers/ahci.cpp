#include "drivers/ahci.hpp"
#include "drivers/pci.hpp"
#include <stddef.h>
#include <stdint.h>

extern "C" void* memcpy(void* destination, const void* source, size_t count);
extern "C" void* memset(void* destination, int value, size_t count);

namespace {
constexpr uint32_t hba_ghc_ae = 1u << 31;
constexpr uint32_t port_cmd_st = 1u;
constexpr uint32_t port_cmd_fre = 1u << 4;
constexpr uint32_t port_cmd_fr = 1u << 14;
constexpr uint32_t port_cmd_cr = 1u << 15;
constexpr uint32_t port_is_task_file_error = 1u << 30;
constexpr uint32_t port_tfd_busy = 1u << 7;
constexpr uint32_t port_tfd_data_request = 1u << 3;
constexpr uint32_t sata_signature_ata = 0x00000101u;
constexpr uint32_t transfer_bytes = 4096;
constexpr uint32_t sectors_per_block = transfer_bytes / 512;

struct HbaPort {
    volatile uint32_t clb;
    volatile uint32_t clbu;
    volatile uint32_t fb;
    volatile uint32_t fbu;
    volatile uint32_t is;
    volatile uint32_t ie;
    volatile uint32_t cmd;
    uint32_t reserved0;
    volatile uint32_t tfd;
    volatile uint32_t sig;
    volatile uint32_t ssts;
    volatile uint32_t sctl;
    volatile uint32_t serr;
    volatile uint32_t sact;
    volatile uint32_t ci;
    volatile uint32_t sntf;
    volatile uint32_t fbs;
    uint32_t reserved1[11];
    uint32_t vendor[4];
};

struct HbaMemory {
    volatile uint32_t cap;
    volatile uint32_t ghc;
    volatile uint32_t is;
    volatile uint32_t pi;
    volatile uint32_t vs;
    volatile uint32_t ccc_ctl;
    volatile uint32_t ccc_pts;
    volatile uint32_t em_loc;
    volatile uint32_t em_ctl;
    volatile uint32_t cap2;
    volatile uint32_t bohc;
    uint8_t reserved[0xA0 - 0x2C];
    uint8_t vendor[0x100 - 0xA0];
    HbaPort ports[32];
};

struct CommandHeader {
    uint16_t flags;
    uint16_t prdt_length;
    uint32_t transferred;
    uint32_t table_address;
    uint32_t table_address_high;
    uint32_t reserved[4];
} __attribute__((packed));

struct PrdtEntry {
    uint32_t address;
    uint32_t address_high;
    uint32_t reserved;
    uint32_t byte_count_and_flags;
} __attribute__((packed));

struct CommandTable {
    uint8_t fis[64];
    uint8_t atapi_command[16];
    uint8_t reserved[48];
    PrdtEntry prdt;
    uint8_t padding[112];
} __attribute__((packed, aligned(128)));

struct alignas(1024) PortMemory {
    CommandHeader headers[32];
    alignas(256) uint8_t received_fis[256];
    CommandTable command_table;
};

static_assert(sizeof(HbaPort) == 0x80, "AHCI port register layout must be 128 bytes");
static_assert(sizeof(CommandHeader) == 32, "AHCI command header must be 32 bytes");
static_assert(sizeof(CommandTable) == 256, "AHCI command table must be 256 bytes");

PortMemory port_memory[32]{};
alignas(4096) uint8_t dma_buffer[transfer_bytes]{};
HbaMemory* hba = nullptr;

uint32_t pointer_address(const void* pointer) {
    return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pointer));
}

bool wait_command_ready(volatile uint32_t* port) {
    for (uint32_t attempt = 0; attempt < 1000000; ++attempt) {
        if ((port[8] & (port_tfd_busy | port_tfd_data_request)) == 0) return true;
    }
    return false;
}

bool stop_port(volatile uint32_t* port) {
    port[6] &= ~port_cmd_st;
    port[6] &= ~port_cmd_fre;
    for (uint32_t attempt = 0; attempt < 1000000; ++attempt) {
        if ((port[6] & (port_cmd_cr | port_cmd_fr)) == 0) return true;
    }
    return false;
}

bool start_port(volatile uint32_t* port) {
    if (!stop_port(port)) return false;
    PortMemory& memory = port_memory[(reinterpret_cast<uintptr_t>(port) - reinterpret_cast<uintptr_t>(hba->ports)) / sizeof(HbaPort)];
    memset(&memory, 0, sizeof(memory));
    port[0] = pointer_address(memory.headers);
    port[1] = 0;
    port[2] = pointer_address(memory.received_fis);
    port[3] = 0;
    port[4] = 0xFFFFFFFFu;
    port[12] = 0xFFFFFFFFu;
    memory.headers[0].table_address = pointer_address(&memory.command_table);
    memory.headers[0].table_address_high = 0;
    port[6] |= port_cmd_fre;
    port[6] |= port_cmd_st;
    return true;
}

bool issue_command(volatile uint32_t* port, uint8_t command, uint64_t lba, uint16_t sectors,
                   bool write, uint32_t byte_count) {
    if (!wait_command_ready(port)) return false;
    uintptr_t port_offset = reinterpret_cast<uintptr_t>(port) - reinterpret_cast<uintptr_t>(hba->ports);
    uint32_t index = static_cast<uint32_t>(port_offset / sizeof(HbaPort));
    PortMemory& memory = port_memory[index];
    CommandHeader& header = memory.headers[0];
    CommandTable& table = memory.command_table;
    memset(&table, 0, sizeof(table));
    header.flags = static_cast<uint16_t>(5u | (write ? (1u << 6) : 0u));
    header.prdt_length = byte_count == 0 ? 0 : 1;
    header.transferred = 0;
    if (byte_count != 0) {
        table.prdt.address = pointer_address(dma_buffer);
        table.prdt.address_high = 0;
        table.prdt.byte_count_and_flags = (byte_count - 1u) & 0x003FFFFFu;
    }
    table.fis[0] = 0x27;
    table.fis[1] = 1u << 7;
    table.fis[2] = command;
    table.fis[4] = static_cast<uint8_t>(lba);
    table.fis[5] = static_cast<uint8_t>(lba >> 8);
    table.fis[6] = static_cast<uint8_t>(lba >> 16);
    table.fis[7] = 1u << 6;
    table.fis[8] = static_cast<uint8_t>(lba >> 24);
    table.fis[9] = static_cast<uint8_t>(lba >> 32);
    table.fis[10] = static_cast<uint8_t>(lba >> 40);
    table.fis[12] = static_cast<uint8_t>(sectors);
    table.fis[13] = static_cast<uint8_t>(sectors >> 8);
    port[4] = 0xFFFFFFFFu;
    port[12] = 0xFFFFFFFFu;
    if (!wait_command_ready(port)) return false;
    port[14] = 1u;
    for (uint32_t attempt = 0; attempt < 10000000; ++attempt) {
        if ((port[14] & 1u) == 0) {
            return (port[12] & port_is_task_file_error) == 0 && (port[8] & 1u) == 0;
        }
        if ((port[12] & port_is_task_file_error) != 0) return false;
    }
    return false;
}

bool identify_disk(volatile uint32_t* port, uint64_t& sectors) {
    if (!issue_command(port, 0xEC, 0, 1, false, 512)) return false;
    const uint16_t* identify = reinterpret_cast<const uint16_t*>(dma_buffer);
    sectors = static_cast<uint64_t>(identify[100]) |
              (static_cast<uint64_t>(identify[101]) << 16) |
              (static_cast<uint64_t>(identify[102]) << 32) |
              (static_cast<uint64_t>(identify[103]) << 48);
    return sectors >= sectors_per_block;
}

struct FindAhciContext { kyron::drivers::PciDevice device{}; bool found = false; };

void find_ahci(const kyron::drivers::PciDevice& device, void* context) {
    auto& result = *static_cast<FindAhciContext*>(context);
    if (!result.found && device.class_code == 0x01 && device.subclass == 0x06 && device.prog_if == 0x01) {
        result.device = device;
        result.found = true;
    }
}
}

namespace kyron::drivers {
bool AhciDisk::read(uint64_t block, void* buffer) {
    if (!port || !buffer || block >= block_count() || block > (0x0000FFFFFFFFFFFFull / sectors_per_block)) return false;
    uint64_t lba = block * sectors_per_block;
    if (!issue_command(port, 0x25, lba, sectors_per_block, false, transfer_bytes)) return false;
    memcpy(buffer, dma_buffer, transfer_bytes);
    return true;
}

bool AhciDisk::write(uint64_t block, const void* buffer) {
    if (!port || !buffer || block >= block_count() || block > (0x0000FFFFFFFFFFFFull / sectors_per_block)) return false;
    memcpy(dma_buffer, buffer, transfer_bytes);
    return issue_command(port, 0x35, block * sectors_per_block, sectors_per_block, true, transfer_bytes) &&
           issue_command(port, 0xEA, 0, 0, false, 0);
}

uint64_t AhciDisk::block_count() const { return sectors / sectors_per_block; }

DiskKind AhciController::kind() const { return DiskKind::sata; }

bool AhciController::initialize() {
    if (initialized) return disk_count != 0;
    initialized = true;
    FindAhciContext result{};
    pci_enumerate(find_ahci, &result);
    if (!result.found) return false;
    uint32_t bar = result.device.bars[5];
    if ((bar & 1u) != 0 || (bar & 0xFFFFFFF0u) == 0) return false;
    uint32_t command = pci_config_read(result.device.bus, result.device.slot, result.device.function, 0x04) & 0xFFFFu;
    command |= (1u << 1) | (1u << 2);
    pci_config_write(result.device.bus, result.device.slot, result.device.function, 0x04, command);
    hba = reinterpret_cast<HbaMemory*>(static_cast<uintptr_t>(bar & 0xFFFFFFF0u));
    hba->ghc |= hba_ghc_ae;
    uint32_t implemented_ports = hba->pi;
    for (uint32_t index = 0; index < 32 && disk_count < 32; ++index) {
        if ((implemented_ports & (1u << index)) == 0) continue;
        volatile uint32_t* port = reinterpret_cast<volatile uint32_t*>(&hba->ports[index]);
        uint32_t link = port[10];
        if ((link & 0x0Fu) != 3u || ((link >> 8) & 0x0Fu) != 1u || port[9] != sata_signature_ata) continue;
        if (!start_port(port)) continue;
        AhciDisk& disk = disks[disk_count];
        disk.port = port;
        disk.port_number = static_cast<uint8_t>(index);
        if (!identify_disk(port, disk.sectors)) {
            disk.port = nullptr;
            continue;
        }
        ++disk_count;
    }
    return disk_count != 0;
}

uint32_t AhciController::enumerate(DiskDescriptor* descriptors, uint32_t capacity) {
    if (!descriptors || capacity == 0 || !initialize()) return 0;
    uint32_t count = disk_count < capacity ? disk_count : capacity;
    for (uint32_t index = 0; index < count; ++index) {
        DiskDescriptor& descriptor = descriptors[index];
        descriptor.kind = DiskKind::sata;
        descriptor.controller = 0;
        descriptor.partition = 0;
        descriptor.device = &disks[index];
        descriptor.name[0] = 's'; descriptor.name[1] = 'a'; descriptor.name[2] = 't'; descriptor.name[3] = 'a';
        descriptor.name[4] = '0'; descriptor.name[5] = '-';
        descriptor.name[6] = static_cast<char>('0' + disks[index].port_number / 10);
        descriptor.name[7] = static_cast<char>('0' + disks[index].port_number % 10);
        descriptor.name[8] = 0;
    }
    return count;
}
}