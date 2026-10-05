#pragma once
#include "drivers/storage.hpp"

namespace kyron::drivers {
class AhciDisk final : public kyron::fs::BlockDevice {
public:
    bool read(uint64_t block, void* buffer) override;
    bool write(uint64_t block, const void* buffer) override;
    uint64_t block_count() const override;

private:
    friend class AhciController;
    volatile uint32_t* port = nullptr;
    uint64_t sectors = 0;
    uint8_t port_number = 0;
};

class AhciController final : public StorageController {
public:
    DiskKind kind() const override;
    uint32_t enumerate(DiskDescriptor* descriptors, uint32_t capacity) override;

private:
    bool initialize();
    bool initialized = false;
    AhciDisk disks[32]{};
    uint32_t disk_count = 0;
};
}