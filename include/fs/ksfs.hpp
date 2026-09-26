#pragma once
#include "fs/block_device.hpp"
#include <stdint.h>
#include <stddef.h>

namespace kyron::fs {
constexpr uint32_t KSFS_MAGIC = 0x4B534653;
constexpr uint32_t KSFS_VERSION = 1;
constexpr uint32_t KSFS_BLOCK_SIZE = 4096;
constexpr uint32_t KSFS_ROOT_INODE = 1;

struct Superblock {
    uint32_t magic;
    uint32_t version;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t inode_table_block;
    uint32_t inode_table_blocks;
    uint32_t inode_count;
    uint32_t root_inode;
    uint32_t bitmap_block;
    uint32_t checksum;
};

bool format(BlockDevice& device, uint32_t inode_count = 128);
bool mount(BlockDevice& device, Superblock& superblock);
uint32_t checksum(const Superblock& superblock);
}
