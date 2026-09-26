#include "drivers/storage.hpp"
#include <string.h>

namespace {
struct Prefix { const char* text; kyron::drivers::DiskKind kind; };
constexpr Prefix prefixes[] = {
    {"nvme", kyron::drivers::DiskKind::nvme},
    {"sata", kyron::drivers::DiskKind::sata},
    {"scsi", kyron::drivers::DiskKind::scsi},
    {"ide", kyron::drivers::DiskKind::ide},
    {"eemc", kyron::drivers::DiskKind::eemc},
    {"usb", kyron::drivers::DiskKind::usb},
    {"sd", kyron::drivers::DiskKind::sd}
};
}

namespace kyron::drivers {
DiskKind disk_kind(const char* name) {
    if (!name) return DiskKind::unknown;
    for (const Prefix& prefix : prefixes) if (strncmp(name, prefix.text, strlen(prefix.text)) == 0) return prefix.kind;
    return DiskKind::unknown;
}

bool parse_disk_name(const char* name, DiskDescriptor& descriptor) {
    if (!name) return false;
    DiskKind kind = disk_kind(name);
    if (kind == DiskKind::unknown) return false;
    uint32_t prefix_length = 0;
    for (const Prefix& prefix : prefixes) if (prefix.kind == kind) { prefix_length = static_cast<uint32_t>(strlen(prefix.text)); break; }
    uint32_t index = prefix_length;
    uint32_t controller = 0;
    if (name[index] < '0' || name[index] > '9') return false;
    while (name[index] >= '0' && name[index] <= '9') { controller = controller * 10 + name[index] - '0'; ++index; }
    if (name[index++] != '-') return false;
    uint32_t partition = 0;
    if (name[index] < '0' || name[index] > '9') return false;
    while (name[index] >= '0' && name[index] <= '9') { partition = partition * 10 + name[index] - '0'; ++index; }
    if (name[index] != 0) return false;
    descriptor.kind = kind;
    descriptor.controller = controller;
    descriptor.partition = partition;
    descriptor.device = nullptr;
    strncpy(descriptor.name, name, sizeof(descriptor.name) - 1);
    descriptor.name[sizeof(descriptor.name) - 1] = 0;
    return true;
}

bool StorageRegistry::add(StorageController& controller) {
    if (count == max_controllers) return false;
    controllers[count++] = &controller;
    return true;
}

uint32_t StorageRegistry::enumerate(DiskDescriptor* descriptors, uint32_t capacity) {
    uint32_t total = 0;
    for (uint32_t index = 0; index < count && total < capacity; ++index)
        total += controllers[index]->enumerate(descriptors + total, capacity - total);
    return total;
}
}
