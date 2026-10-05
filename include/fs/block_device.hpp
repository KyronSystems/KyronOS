#pragma once
#include <stdint.h>

namespace kyron::fs {
class BlockDevice {
public:
    virtual ~BlockDevice() = default;
    virtual bool read(uint64_t block, void* buffer) = 0;
    virtual bool write(uint64_t block, const void* buffer) = 0;
    virtual uint64_t block_count() const = 0;
};

class BlockDeviceSlice final : public BlockDevice {
public:
    bool configure(BlockDevice& parent, uint64_t first_block, uint64_t block_count) {
        if (first_block > parent.block_count() || block_count > parent.block_count() - first_block) return false;
        device = &parent;
        start = first_block;
        count = block_count;
        return true;
    }
    bool read(uint64_t block, void* buffer) override {
        return device && block < count && device->read(start + block, buffer);
    }
    bool write(uint64_t block, const void* buffer) override {
        return device && block < count && device->write(start + block, buffer);
    }
    uint64_t block_count() const override { return count; }

private:
    BlockDevice* device = nullptr;
    uint64_t start = 0;
    uint64_t count = 0;
};
}
