#include "kernel/console.hpp"
#include "kernel/mouse.hpp"
#include "kernel/shell.hpp"
#include "drivers/pci.hpp"
#include "drivers/ata_pio.hpp"
#include <stdint.h>

namespace {
struct PciSummary { uint32_t devices; uint32_t storage; };
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
    console::write_line("KSFS: RAM-only shell storage", 0x0D);
    console::write_line("USB: controller probing deferred", 0x0D);
    console::write_line("CD/DVD: provided by boot media", 0x0D);
    console::write_line("Mounting KSFS ........... pending storage driver", 0x0D);
    console::write_line("");
    shell::run();
}
