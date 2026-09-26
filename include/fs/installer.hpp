#pragma once
#include "fs/block_device.hpp"

namespace kyron::fs {
class Installer {
public:
    static bool install(BlockDevice& device, const char* hostname = "kyron");
};
}
