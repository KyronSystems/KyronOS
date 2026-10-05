#pragma once
#include "fs/block_device.hpp"
#include <stdint.h>

namespace kyron::fs {
bool install_hybrid_boot_image(BlockDevice& device, const void* image, uint32_t image_size);
bool update_embedded_kernel(BlockDevice& device, const void* kernel_image, uint32_t image_size);
}
