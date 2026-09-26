#pragma once
#include "fs/block_device.hpp"
#include <stdint.h>

namespace kyron::drivers {
enum class DiskKind : uint8_t {
    nvme,
    sata,
    scsi,
    ide,
    eemc,
    usb,
    sd,
    unknown
};

struct DiskDescriptor {
    DiskKind kind;
    uint32_t controller;
    uint32_t partition;
    char name[24];
    kyron::fs::BlockDevice* device;
};

DiskKind disk_kind(const char* name);
bool parse_disk_name(const char* name, DiskDescriptor& descriptor);

class StorageController {
public:
    virtual ~StorageController() = default;
    virtual DiskKind kind() const = 0;
    virtual uint32_t enumerate(DiskDescriptor* descriptors, uint32_t capacity) = 0;
};

class StorageRegistry {
public:
    static constexpr uint32_t max_controllers = 16;
    bool add(StorageController& controller);
    uint32_t enumerate(DiskDescriptor* descriptors, uint32_t capacity);

private:
    StorageController* controllers[max_controllers]{};
    uint32_t count = 0;
};
}
