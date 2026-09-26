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
}
