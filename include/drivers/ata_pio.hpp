#pragma once
#include "drivers/storage.hpp"

namespace kyron::drivers {
class AtaPioDisk final : public kyron::fs::BlockDevice {
public:
    AtaPioDisk(uint16_t io_base, uint16_t control_base, uint8_t device);
    bool probe();
    bool read(uint64_t block, void* buffer) override;
    bool write(uint64_t block, const void* buffer) override;
    uint64_t block_count() const override;

private:
    bool wait_not_busy() const;
    bool wait_data_request() const;
    void select(uint32_t lba) const;
    uint16_t io_base;
    uint16_t control_base;
    uint8_t device;
    uint32_t sectors = 0;
    bool present = false;
};

class AtaPioController final : public StorageController {
public:
    AtaPioController(uint16_t io_base, uint16_t control_base, uint32_t controller_number);
    DiskKind kind() const override;
    uint32_t enumerate(DiskDescriptor* descriptors, uint32_t capacity) override;

private:
    AtaPioDisk disks[2];
    uint32_t controller_number;
};
}
