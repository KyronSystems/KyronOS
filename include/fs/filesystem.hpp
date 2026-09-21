#pragma once
#include "fs/ksfs.hpp"
#include <stdint.h>

namespace kyron::fs {
constexpr uint32_t KSFS_INODE_SIZE = 64;
constexpr uint32_t KSFS_NAME_SIZE = 48;
constexpr uint32_t KSFS_DIRECTORY_ENTRY_SIZE = 64;
constexpr uint32_t KSFS_DIRECTORY_MODE = 1;
constexpr uint32_t KSFS_FILE_MODE = 2;

struct Inode {
    uint32_t mode;
    uint32_t size;
    uint32_t parent;
    uint32_t data_blocks;
    uint32_t blocks[12];
};

struct DirectoryEntry {
    uint32_t inode;
    uint32_t mode;
    char name[KSFS_NAME_SIZE];
};

using DirectoryVisitor = bool (*)(const DirectoryEntry& entry, void* context);

class FileSystem {
public:
    explicit FileSystem(BlockDevice& device);
    bool format(uint32_t inode_count = 128);
    bool mount();
    bool mounted() const;
    const Superblock& superblock() const;
    bool read_inode(uint32_t inode, Inode& value) const;
    bool find(uint32_t parent, const char* name, uint32_t& inode) const;
    bool resolve_path(uint32_t starting_inode, const char* path, uint32_t& inode) const;
    bool create_directory(uint32_t parent, const char* name, uint32_t& inode);
    bool create_file(uint32_t parent, const char* name, const char* data, uint32_t size, uint32_t& inode);
    bool read_file(uint32_t inode, void* buffer, uint32_t capacity, uint32_t& size) const;
    bool remove(uint32_t parent, const char* name);
    bool list_directory(uint32_t inode, DirectoryVisitor visitor, void* context) const;
    BlockDevice& block_device() const;

private:
    BlockDevice& device;
    Superblock metadata{};
    bool is_mounted = false;
};
}
