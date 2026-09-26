#include "drivers/ata_pio.hpp"

namespace {
void outb(uint16_t port, uint8_t value) { asm volatile("outb %0, %1" : : "a"(value), "Nd"(port)); }
uint8_t inb(uint16_t port) { uint8_t value; asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port)); return value; }
void insw(uint16_t port, void* buffer, uint32_t words) { asm volatile("rep insw" : "+D"(buffer), "+c"(words) : "d"(port) : "memory"); }
void outsw(uint16_t port, const void* buffer, uint32_t words) { asm volatile("rep outsw" : "+S"(buffer), "+c"(words) : "d"(port)); }
void pause_io() { asm volatile("inb %%dx, %%al" : : "dN"(0x80)); }
}

namespace kyron::drivers {
AtaPioDisk::AtaPioDisk(uint16_t base, uint16_t control, uint8_t selected_device)
    : io_base(base), control_base(control), device(selected_device) {}

void AtaPioDisk::select(uint32_t lba) const {
    outb(static_cast<uint16_t>(io_base + 6), static_cast<uint8_t>(0xE0 | (device << 4) | ((lba >> 24) & 0x0F)));
    pause_io();
}

bool AtaPioDisk::wait_not_busy() const {
    for (uint32_t count = 0; count < 100000; ++count) if ((inb(static_cast<uint16_t>(io_base + 7)) & 0x80) == 0) return true;
    return false;
}

bool AtaPioDisk::wait_data_request() const {
    for (uint32_t count = 0; count < 100000; ++count) {
        uint8_t status = inb(static_cast<uint16_t>(io_base + 7));
        if (status & 0x01) return false;
        if (status & 0x08) return true;
    }
    return false;
}

bool AtaPioDisk::probe() {
    select(0);
    outb(static_cast<uint16_t>(io_base + 2), 0);
    outb(static_cast<uint16_t>(io_base + 3), 0);
    outb(static_cast<uint16_t>(io_base + 4), 0);
    outb(static_cast<uint16_t>(io_base + 5), 0);
    outb(static_cast<uint16_t>(io_base + 7), 0xEC);
    if (inb(static_cast<uint16_t>(io_base + 7)) == 0 || !wait_not_busy()) return false;
    if (!wait_data_request()) return false;
    uint16_t identify[256]{};
    for (uint32_t index = 0; index < 256; ++index) {
        uint16_t word;
        asm volatile("inw %1, %0" : "=a"(word) : "Nd"(io_base));
        identify[index] = word;
    }
    sectors = static_cast<uint32_t>(identify[60]) | (static_cast<uint32_t>(identify[61]) << 16);
    present = sectors >= 8;
    return present;
}

bool AtaPioDisk::read(uint64_t block, void* buffer) {
    if (!present || !buffer || block >= block_count() || block > 0x1FFFFFF) return false;
    uint32_t lba = static_cast<uint32_t>(block * 8);
    select(lba);
    outb(static_cast<uint16_t>(io_base + 2), 8);
    outb(static_cast<uint16_t>(io_base + 3), static_cast<uint8_t>(lba));
    outb(static_cast<uint16_t>(io_base + 4), static_cast<uint8_t>(lba >> 8));
    outb(static_cast<uint16_t>(io_base + 5), static_cast<uint8_t>(lba >> 16));
    outb(static_cast<uint16_t>(io_base + 7), 0x20);
    for (uint32_t sector = 0; sector < 8; ++sector) {
        if (!wait_not_busy() || !wait_data_request()) return false;
        insw(io_base, static_cast<uint8_t*>(buffer) + sector * 512, 256);
    }
    return true;
}

bool AtaPioDisk::write(uint64_t block, const void* buffer) {
    if (!present || !buffer || block >= block_count() || block > 0x1FFFFFF) return false;
    uint32_t lba = static_cast<uint32_t>(block * 8);
    select(lba);
    outb(static_cast<uint16_t>(io_base + 2), 8);
    outb(static_cast<uint16_t>(io_base + 3), static_cast<uint8_t>(lba));
    outb(static_cast<uint16_t>(io_base + 4), static_cast<uint8_t>(lba >> 8));
    outb(static_cast<uint16_t>(io_base + 5), static_cast<uint8_t>(lba >> 16));
    outb(static_cast<uint16_t>(io_base + 7), 0x30);
    for (uint32_t sector = 0; sector < 8; ++sector) {
        if (!wait_not_busy() || !wait_data_request()) return false;
        outsw(io_base, static_cast<const uint8_t*>(buffer) + sector * 512, 256);
    }
    outb(static_cast<uint16_t>(io_base + 7), 0xE7);
    return wait_not_busy();
}

uint64_t AtaPioDisk::block_count() const { return sectors / 8; }

AtaPioController::AtaPioController(uint16_t io_base, uint16_t control_base, uint32_t number)
    : disks{AtaPioDisk(io_base, control_base, 0), AtaPioDisk(io_base, control_base, 1)}, controller_number(number) {}

DiskKind AtaPioController::kind() const { return DiskKind::ide; }

uint32_t AtaPioController::enumerate(DiskDescriptor* descriptors, uint32_t capacity) {
    if (!descriptors || capacity < 2) return 0;
    uint32_t found = 0;
    for (uint32_t index = 0; index < 2; ++index) {
        if (!disks[index].probe()) continue;
        DiskDescriptor& descriptor = descriptors[found++];
        descriptor.kind = DiskKind::ide;
        descriptor.controller = controller_number;
        descriptor.partition = 0;
        descriptor.device = &disks[index];
        descriptor.name[0] = 'i'; descriptor.name[1] = 'd'; descriptor.name[2] = 'e';
        descriptor.name[3] = static_cast<char>('0' + controller_number);
        descriptor.name[4] = '-'; descriptor.name[5] = static_cast<char>('0' + index); descriptor.name[6] = 0;
    }
    return found;
}
}
