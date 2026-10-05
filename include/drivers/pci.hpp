#pragma once
#include <stdint.h>

namespace kyron::drivers {
struct PciDevice {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint16_t vendor;
    uint16_t device;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint32_t bars[6];
};

using PciVisitor = void (*)(const PciDevice& device, void* context);

uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset);
void pci_config_write(uint8_t bus, uint8_t slot, uint8_t function, uint8_t offset, uint32_t value);
uint32_t pci_enumerate(PciVisitor visitor, void* context);
}
