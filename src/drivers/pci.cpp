#include "drivers/pci.hpp"

namespace {
void outl(uint16_t port, uint32_t value) {
    asm volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

uint32_t inl(uint16_t port) {
    uint32_t value;
    asm volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

uint32_t config_read(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    uint32_t address = 0x80000000u |
        (static_cast<uint32_t>(bus) << 16) |
        (static_cast<uint32_t>(slot) << 11) |
        (static_cast<uint32_t>(function) << 8) |
        (offset & 0xFC);
    outl(0xCF8, address);
    return inl(0xCFC);
}
}

namespace kyron::drivers {
uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset) {
    return config_read(bus, slot, function, offset);
}

uint32_t pci_enumerate(PciVisitor visitor, void* context) {
    if (!visitor) return 0;
    uint32_t count = 0;
    for (uint32_t bus = 0; bus < 256; ++bus) {
        for (uint32_t slot = 0; slot < 32; ++slot) {
            uint32_t header = config_read(static_cast<uint8_t>(bus), static_cast<uint8_t>(slot), 0, 0);
            if ((header & 0xFFFFu) == 0xFFFFu) continue;
            uint32_t functions = ((config_read(static_cast<uint8_t>(bus), static_cast<uint8_t>(slot), 0, 0x0C) >> 16) & 0x80u) ? 8 : 1;
            for (uint32_t function = 0; function < functions; ++function) {
                uint32_t identity = config_read(static_cast<uint8_t>(bus), static_cast<uint8_t>(slot), static_cast<uint8_t>(function), 0);
                if ((identity & 0xFFFFu) == 0xFFFFu) continue;
                PciDevice device{};
                device.bus = static_cast<uint8_t>(bus);
                device.slot = static_cast<uint8_t>(slot);
                device.function = static_cast<uint8_t>(function);
                device.vendor = static_cast<uint16_t>(identity & 0xFFFFu);
                device.device = static_cast<uint16_t>(identity >> 16);
                uint32_t class_data = config_read(device.bus, device.slot, device.function, 0x08);
                device.class_code = static_cast<uint8_t>(class_data >> 24);
                device.subclass = static_cast<uint8_t>(class_data >> 16);
                device.prog_if = static_cast<uint8_t>(class_data >> 8);
                for (uint32_t bar = 0; bar < 6; ++bar) device.bars[bar] = config_read(device.bus, device.slot, device.function, static_cast<uint8_t>(0x10 + bar * 4));
                visitor(device, context);
                ++count;
            }
        }
    }
    return count;
}
}
