#include "kernel/console.hpp"
#include "kernel/mouse.hpp"
#include "kernel/shell.hpp"
#include "drivers/pci.hpp"
#include "drivers/ahci.hpp"
#include "drivers/ata_pio.hpp"
#include "fs/filesystem.hpp"
#include <stdint.h>

namespace {
constexpr uint64_t ksfs_start_block = 16384;
struct PciSummary { uint32_t devices; uint32_t storage; };
struct UpdateModule { const void* image; uint32_t size; };

bool find_module(uint32_t multiboot_info, const char* expected, UpdateModule& result) {
    if (!multiboot_info) return false;
    uint32_t expected_length = 0;
    while (expected[expected_length]) ++expected_length;
    const auto* info = reinterpret_cast<const uint8_t*>(multiboot_info);
    uint32_t total_size = *reinterpret_cast<const uint32_t*>(info);
    if (total_size < 16) return false;
    const uint8_t* tag = info + 8;
    const uint8_t* end = info + total_size;
    while (tag + 8 <= end) {
        uint32_t type = *reinterpret_cast<const uint32_t*>(tag);
        uint32_t size = *reinterpret_cast<const uint32_t*>(tag + 4);
        if (size < 8 || tag + size > end) break;
        if (type == 3 && size >= 17) {
            const char* command = reinterpret_cast<const char*>(tag + 16);
            uint32_t index = 0;
            while (index < expected_length && index + 1 < size - 16 && command[index] == expected[index]) ++index;
            if (index == expected_length && command[index] == 0) {
                uint32_t start = *reinterpret_cast<const uint32_t*>(tag + 8);
                uint32_t finish = *reinterpret_cast<const uint32_t*>(tag + 12);
                if (finish > start) {
                    result.image = reinterpret_cast<const void*>(start);
                    result.size = finish - start;
                    return true;
                }
            }
        }
        if (type == 0) break;
        tag += (size + 7) & ~7u;
    }
    return false;
}

void inspect_pci(const kyron::drivers::PciDevice& device, void* context) {
    auto& summary = *static_cast<PciSummary*>(context);
    ++summary.devices;
    if (device.class_code == 0x01) ++summary.storage;
}
}

extern "C" void kmain(uint32_t magic, uint32_t multiboot_info) {
    if (magic == 0x36D76289) {
        console::initialize(multiboot_info);
    }
    console::clear();
    console::write_line("+----------------------------------------------------------+", 0x09);
    console::write_line("|                      KYRONOS                            |", 0x0D);
    console::write_line("|                 KSFS SYSTEM                             |", 0x0B);
    console::write_line("+----------------------------------------------------------+", 0x09);
    console::write_line("");
    if (magic != 0x36D76289) {
        console::write_line("Boot error: invalid Multiboot2 magic.", 0x0C);
        return;
    }
    mouse::initialize();
    console::write_line("Initializing CPU ........ OK", 0x0B);
    console::write_line("Initializing console ... OK", 0x0B);
    PciSummary pci{};
    kyron::drivers::pci_enumerate(inspect_pci, &pci);
    console::write_line(pci.devices == 0 ? "PCI: no devices discovered" : "PCI: device scan complete", 0x0B);
    console::write_line(pci.storage == 0 ? "Storage: no controller driver available" : "Storage: controller candidates found", 0x0D);
    kyron::drivers::AtaPioController primary_ide(0x1F0, 0x3F6, 0);
    kyron::drivers::AtaPioController secondary_ide(0x170, 0x376, 1);
    kyron::drivers::DiskDescriptor ide_disks[4]{};
    uint32_t ide_count = primary_ide.enumerate(ide_disks, 4);
    ide_count += secondary_ide.enumerate(ide_disks + ide_count, 4 - ide_count);
    console::write_line(ide_count == 0 ? "IDE: no PIO disks detected" : "IDE: PIO disk detected", 0x0B);
    kyron::drivers::AhciController sata;
    kyron::drivers::DiskDescriptor sata_disks[32]{};
    uint32_t sata_count = sata.enumerate(sata_disks, 32);
    console::write_line(sata_count == 0 ? "SATA: no AHCI disks detected" : "SATA: AHCI disk detected", 0x0B);
    kyron::drivers::DiskDescriptor raw_disks[36]{};
    uint32_t raw_disk_count = 0;
    for (uint32_t index = 0; index < ide_count; ++index) raw_disks[raw_disk_count++] = ide_disks[index];
    for (uint32_t index = 0; index < sata_count; ++index) raw_disks[raw_disk_count++] = sata_disks[index];
    kyron::drivers::DiskDescriptor disks[36]{};
    kyron::drivers::DiskDescriptor install_disks[36]{};
    kyron::fs::BlockDeviceSlice slices[36]{};
    uint32_t disk_count = 0;
    for (uint32_t index = 0; index < raw_disk_count; ++index) {
        kyron::fs::BlockDevice* raw = raw_disks[index].device;
        if (!raw || raw->block_count() <= ksfs_start_block + 8) continue;
        if (!slices[disk_count].configure(*raw, ksfs_start_block, raw->block_count() - ksfs_start_block)) continue;
        disks[disk_count] = raw_disks[index];
        disks[disk_count].device = &slices[disk_count];
        install_disks[disk_count] = raw_disks[index];
        ++disk_count;
    }
    console::write_line("USB: controller probing deferred", 0x0D);
    console::write_line("CD/DVD: provided by boot media", 0x0D);
    UpdateModule update{};
    UpdateModule install_media{};
    bool update_available = find_module(multiboot_info, "kyron-update", update);
    bool install_media_available = find_module(multiboot_info, "kyron-install-image", install_media);
    console::write_line(update_available ? "CD update payload: available" : "CD update payload: not loaded", 0x0B);
    console::write_line(install_media_available ? "Installer boot image: available" : "Installer boot image: not loaded", 0x0B);
    if (disk_count != 0) {
        for (uint32_t index = 0; index < disk_count; ++index) {
            kyron::fs::FileSystem probe(*disks[index].device);
            if (probe.mount()) {
                if (index != 0) {
                    kyron::drivers::DiskDescriptor swap = disks[0];
                    disks[0] = disks[index];
                    disks[index] = swap;
                    swap = install_disks[0];
                    install_disks[0] = install_disks[index];
                    install_disks[index] = swap;
                }
                break;
            }
        }
        kyron::fs::FileSystem filesystem(*disks[0].device);
        console::write_line(filesystem.mount() ? "KSFS: persistent volume mounted" : "KSFS: disk is unformatted; installer available", 0x0B);
        shell::run(install_disks, disks, disk_count, &filesystem, update.image, update.size,
               install_media.image, install_media.size);
    }
    console::write_line("KSFS: no disk found; starting live RAM session", 0x0D);
    shell::run(nullptr, nullptr, 0, nullptr, update.image, update.size, install_media.image, install_media.size);
}
