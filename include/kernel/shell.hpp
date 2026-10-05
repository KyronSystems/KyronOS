#pragma once
#include "drivers/storage.hpp"
#include "fs/filesystem.hpp"
#include <stdint.h>
namespace shell {
[[noreturn]] void run(kyron::drivers::DiskDescriptor* raw_disks,
					 kyron::drivers::DiskDescriptor* disks,
					 uint32_t disk_count,
					 kyron::fs::FileSystem* filesystem,
					 const void* update_image,
					 uint32_t update_image_size,
					 const void* install_image,
					 uint32_t install_image_size);
}
