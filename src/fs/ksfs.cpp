#include "fs/ksfs.hpp"

namespace {
void copy_bytes(void* destination, const void* source, size_t count) {
    auto* output = static_cast<uint8_t*>(destination);
    const auto* input = static_cast<const uint8_t*>(source);
    for (size_t index = 0; index < count; ++index) output[index] = input[index];
}
}

namespace kyron::fs {
uint32_t checksum(const Superblock& block) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(&block);
    uint32_t value = 2166136261u;
    for (size_t index = 0; index < sizeof(Superblock) - sizeof(uint32_t); ++index) {
        value ^= bytes[index];
        value *= 16777619u;
    }
    return value;
}

bool format(BlockDevice& device, uint32_t inode_count) {
    if (device.block_count() < 8 || inode_count == 0) return false;
    uint32_t inode_table_blocks = (inode_count * 64 + KSFS_BLOCK_SIZE - 1) / KSFS_BLOCK_SIZE;
    uint32_t metadata_blocks = 2 + inode_table_blocks;
    if (device.block_count() <= metadata_blocks) return false;
    Superblock block{};
    block.magic = KSFS_MAGIC;
    block.version = KSFS_VERSION;
    block.block_size = KSFS_BLOCK_SIZE;
    block.total_blocks = static_cast<uint32_t>(device.block_count());
    block.inode_table_block = 2;
    block.inode_count = inode_count;
    block.root_inode = KSFS_ROOT_INODE;
    block.bitmap_block = 1;
    block.inode_table_blocks = inode_table_blocks;
    block.free_blocks = block.total_blocks - metadata_blocks;
    block.checksum = checksum(block);
    uint8_t buffer[KSFS_BLOCK_SIZE]{};
    copy_bytes(buffer, &block, sizeof(block));
    if (!device.write(0, buffer)) return false;
    uint8_t bitmap[KSFS_BLOCK_SIZE]{};
    for (uint32_t index = 0; index < metadata_blocks; ++index) bitmap[index / 8] |= static_cast<uint8_t>(1u << (index % 8));
    if (!device.write(block.bitmap_block, bitmap)) return false;
    uint8_t empty[KSFS_BLOCK_SIZE]{};
    for (uint32_t index = 0; index < inode_table_blocks; ++index) if (!device.write(block.inode_table_block + index, empty)) return false;
    return true;
}

bool mount(BlockDevice& device, Superblock& block) {
    uint8_t buffer[KSFS_BLOCK_SIZE]{};
    if (!device.read(0, buffer)) return false;
    copy_bytes(&block, buffer, sizeof(block));
    return block.magic == KSFS_MAGIC && block.version == KSFS_VERSION &&
           block.block_size == KSFS_BLOCK_SIZE && block.root_inode == KSFS_ROOT_INODE &&
            block.inode_table_blocks != 0 &&
           block.checksum == checksum(block);
}
}
