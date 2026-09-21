#include "fs/filesystem.hpp"

namespace {
using namespace kyron::fs;

void copy_bytes(void* destination, const void* source, uint32_t count) {
    auto* output = static_cast<uint8_t*>(destination);
    const auto* input = static_cast<const uint8_t*>(source);
    for (uint32_t index = 0; index < count; ++index) output[index] = input[index];
}

void fill_bytes(void* destination, uint8_t value, uint32_t count) {
    auto* output = static_cast<uint8_t*>(destination);
    for (uint32_t index = 0; index < count; ++index) output[index] = value;
}

bool same_text(const char* left, const char* right) {
    uint32_t index = 0;
    while (left[index] || right[index]) {
        if (left[index] != right[index]) return false;
        ++index;
    }
    return true;
}

void copy_name(char* destination, const char* source, uint32_t capacity) {
    uint32_t index = 0;
    while (index + 1 < capacity && source[index]) { destination[index] = source[index]; ++index; }
    destination[index] = 0;
}

bool valid_name(const char* name) {
    if (!name || !*name) return false;
    uint32_t length = 0;
    while (name[length]) {
        if (name[length] == '/' || length >= KSFS_NAME_SIZE - 1) return false;
        ++length;
    }
    return true;
}

bool read_inode(const FileSystem& filesystem, uint32_t inode_number, Inode& inode) {
    const Superblock& metadata = filesystem.superblock();
    if (inode_number == 0 || inode_number > metadata.inode_count) return false;
    uint8_t block[KSFS_BLOCK_SIZE]{};
    uint32_t offset = (inode_number - 1) * KSFS_INODE_SIZE;
    if (!filesystem.block_device().read(metadata.inode_table_block + offset / KSFS_BLOCK_SIZE, block)) return false;
    copy_bytes(&inode, block + offset % KSFS_BLOCK_SIZE, sizeof(inode));
    return true;
}
}

namespace kyron::fs {
FileSystem::FileSystem(BlockDevice& storage) : device(storage) {}

bool FileSystem::format(uint32_t inode_count) {
    if (!kyron::fs::format(device, inode_count) || !mount()) return false;
    uint32_t root_data_block = 0;
    uint8_t bitmap[KSFS_BLOCK_SIZE]{};
    if (!device.read(metadata.bitmap_block, bitmap)) return false;
    for (uint32_t block = 2 + metadata.inode_table_blocks; block < metadata.total_blocks; ++block) {
        if ((bitmap[block / 8] & (1u << (block % 8))) == 0) {
            bitmap[block / 8] |= static_cast<uint8_t>(1u << (block % 8));
            root_data_block = block;
            break;
        }
    }
    if (root_data_block == 0 || !device.write(metadata.bitmap_block, bitmap)) return false;
    Inode root{};
    root.mode = KSFS_DIRECTORY_MODE;
    root.parent = KSFS_ROOT_INODE;
    root.blocks[0] = root_data_block;
    root.data_blocks = 1;
    uint8_t inode_block[KSFS_BLOCK_SIZE]{};
    copy_bytes(inode_block, &root, sizeof(root));
    if (!device.write(metadata.inode_table_block, inode_block)) return false;
    metadata.free_blocks -= 1;
    metadata.checksum = checksum(metadata);
    fill_bytes(inode_block, 0, sizeof(inode_block));
    copy_bytes(inode_block, &metadata, sizeof(metadata));
    return device.write(0, inode_block);
}

bool FileSystem::mount() {
    is_mounted = kyron::fs::mount(device, metadata);
    return is_mounted;
}

bool FileSystem::mounted() const { return is_mounted; }
const Superblock& FileSystem::superblock() const { return metadata; }
BlockDevice& FileSystem::block_device() const { return device; }
bool FileSystem::read_inode(uint32_t inode, Inode& value) const { return ::read_inode(*this, inode, value); }

bool FileSystem::find(uint32_t parent, const char* name, uint32_t& inode) const {
    Inode directory{};
    if (!is_mounted || !valid_name(name) || !::read_inode(*this, parent, directory) || directory.mode != KSFS_DIRECTORY_MODE) return false;
    uint8_t block[KSFS_BLOCK_SIZE]{};
    if (!device.read(directory.blocks[0], block)) return false;
    const auto* entries = reinterpret_cast<const DirectoryEntry*>(block);
    for (uint32_t index = 0; index < KSFS_BLOCK_SIZE / sizeof(DirectoryEntry); ++index) {
        if (entries[index].inode != 0 && same_text(entries[index].name, name)) { inode = entries[index].inode; return true; }
    }
    return false;
}

bool FileSystem::resolve_path(uint32_t starting_inode, const char* path, uint32_t& inode) const {
    if (!is_mounted || !path || !*path) return false;
    uint32_t current = starting_inode == 0 ? KSFS_ROOT_INODE : starting_inode;
    if (path[0] == '/') {
        current = KSFS_ROOT_INODE;
        ++path;
    }
    while (*path) {
        while (*path == '/') ++path;
        if (!*path) break;
        char component[KSFS_NAME_SIZE]{};
        uint32_t length = 0;
        while (path[length] && path[length] != '/' && length + 1 < sizeof(component)) {
            component[length] = path[length];
            ++length;
        }
        if (length == 0) continue;
        component[length] = 0;
        if (same_text(component, ".")) {
            path += length;
            continue;
        }
        if (same_text(component, "..")) {
            Inode parent{};
            if (!::read_inode(*this, current, parent)) return false;
            current = parent.parent;
            path += length;
            continue;
        }
        uint32_t next = 0;
        if (!find(current, component, next)) return false;
        current = next;
        path += length;
    }
    inode = current;
    return true;
}

bool FileSystem::create_directory(uint32_t parent, const char* name, uint32_t& inode) {
    return create_file(parent, name, nullptr, 0, inode);
}

bool FileSystem::create_file(uint32_t parent, const char* name, const char* data, uint32_t size, uint32_t& inode) {
    uint32_t required_blocks = data ? (size + KSFS_BLOCK_SIZE - 1) / KSFS_BLOCK_SIZE : 1;
    if (!is_mounted || !valid_name(name) || required_blocks == 0 || required_blocks > 12 || find(parent, name, inode)) return false;
    Inode parent_inode{};
    if (!::read_inode(*this, parent, parent_inode) || parent_inode.mode != KSFS_DIRECTORY_MODE) return false;
    uint8_t bitmap[KSFS_BLOCK_SIZE]{};
    if (!device.read(metadata.bitmap_block, bitmap)) return false;
    uint32_t data_blocks[12]{};
    uint32_t found_blocks = 0;
    for (uint32_t block = 2 + metadata.inode_table_blocks; block < metadata.total_blocks; ++block) {
        if ((bitmap[block / 8] & (1u << (block % 8))) == 0 && found_blocks < required_blocks) data_blocks[found_blocks++] = block;
    }
    uint32_t free_inode = 0;
    Inode candidate{};
    for (uint32_t index = 2; index <= metadata.inode_count; ++index) {
        if (!::read_inode(*this, index, candidate) || candidate.mode == 0) { free_inode = index; break; }
    }
    if (found_blocks != required_blocks || free_inode == 0) return false;
    uint8_t directory_block[KSFS_BLOCK_SIZE]{};
    if (!device.read(parent_inode.blocks[0], directory_block)) return false;
    auto* entries = reinterpret_cast<DirectoryEntry*>(directory_block);
    uint32_t entry_index = KSFS_BLOCK_SIZE / sizeof(DirectoryEntry);
    for (uint32_t index = 0; index < entry_index; ++index) if (entries[index].inode == 0) { entry_index = index; break; }
    if (entry_index == KSFS_BLOCK_SIZE / sizeof(DirectoryEntry)) return false;
    Inode new_inode{};
    new_inode.mode = data ? KSFS_FILE_MODE : KSFS_DIRECTORY_MODE;
    new_inode.size = data ? size : 0;
    new_inode.parent = parent;
    new_inode.data_blocks = required_blocks;
    for (uint32_t index = 0; index < required_blocks; ++index) new_inode.blocks[index] = data_blocks[index];
    entries[entry_index].inode = free_inode;
    entries[entry_index].mode = new_inode.mode;
    copy_name(entries[entry_index].name, name, KSFS_NAME_SIZE);
    if (!device.write(parent_inode.blocks[0], directory_block)) return false;
    for (uint32_t index = 0; index < required_blocks; ++index) {
        fill_bytes(directory_block, 0, sizeof(directory_block));
        if (data && size > index * KSFS_BLOCK_SIZE) {
            uint32_t offset = index * KSFS_BLOCK_SIZE;
            uint32_t remaining = size - offset;
            if (remaining > KSFS_BLOCK_SIZE) remaining = KSFS_BLOCK_SIZE;
            copy_bytes(directory_block, data + offset, remaining);
        }
        if (!device.write(data_blocks[index], directory_block)) return false;
    }
    fill_bytes(directory_block, 0, sizeof(directory_block));
    copy_bytes(directory_block, &new_inode, sizeof(new_inode));
    uint32_t inode_block_number = metadata.inode_table_block + (free_inode - 1) * KSFS_INODE_SIZE / KSFS_BLOCK_SIZE;
    if (!device.read(inode_block_number, directory_block)) return false;
    copy_bytes(directory_block + ((free_inode - 1) * KSFS_INODE_SIZE) % KSFS_BLOCK_SIZE, &new_inode, sizeof(new_inode));
    if (!device.write(inode_block_number, directory_block)) return false;
    for (uint32_t index = 0; index < required_blocks; ++index) bitmap[data_blocks[index] / 8] |= static_cast<uint8_t>(1u << (data_blocks[index] % 8));
    if (!device.write(metadata.bitmap_block, bitmap)) return false;
    metadata.free_blocks -= required_blocks;
    metadata.checksum = checksum(metadata);
    uint8_t superblock[KSFS_BLOCK_SIZE]{};
    copy_bytes(superblock, &metadata, sizeof(metadata));
    if (!device.write(0, superblock)) return false;
    inode = free_inode;
    return true;
}

bool FileSystem::remove(uint32_t parent, const char* name) {
    uint32_t inode_number = 0;
    if (!is_mounted || !valid_name(name) || !find(parent, name, inode_number) || inode_number == KSFS_ROOT_INODE) return false;
    Inode parent_inode{};
    Inode inode{};
    if (!::read_inode(*this, parent, parent_inode) || !::read_inode(*this, inode_number, inode)) return false;
    if (inode.mode == KSFS_DIRECTORY_MODE) {
        uint8_t child_block[KSFS_BLOCK_SIZE]{};
        if (!device.read(inode.blocks[0], child_block)) return false;
        const auto* children = reinterpret_cast<const DirectoryEntry*>(child_block);
        for (uint32_t index = 0; index < KSFS_BLOCK_SIZE / sizeof(DirectoryEntry); ++index)
            if (children[index].inode != 0) return false;
    }
    uint8_t directory_block[KSFS_BLOCK_SIZE]{};
    if (!device.read(parent_inode.blocks[0], directory_block)) return false;
    auto* entries = reinterpret_cast<DirectoryEntry*>(directory_block);
    for (uint32_t index = 0; index < KSFS_BLOCK_SIZE / sizeof(DirectoryEntry); ++index) {
        if (entries[index].inode == inode_number) {
            fill_bytes(&entries[index], 0, sizeof(DirectoryEntry));
            if (!device.write(parent_inode.blocks[0], directory_block)) return false;
            uint8_t bitmap[KSFS_BLOCK_SIZE]{};
            if (!device.read(metadata.bitmap_block, bitmap)) return false;
            for (uint32_t block = 0; block < inode.data_blocks; ++block)
                bitmap[inode.blocks[block] / 8] &= static_cast<uint8_t>(~(1u << (inode.blocks[block] % 8)));
            if (!device.write(metadata.bitmap_block, bitmap)) return false;
            Inode empty{};
            uint32_t inode_block_number = metadata.inode_table_block + (inode_number - 1) * KSFS_INODE_SIZE / KSFS_BLOCK_SIZE;
            if (!device.read(inode_block_number, directory_block)) return false;
            copy_bytes(directory_block + ((inode_number - 1) * KSFS_INODE_SIZE) % KSFS_BLOCK_SIZE, &empty, sizeof(empty));
            if (!device.write(inode_block_number, directory_block)) return false;
            metadata.free_blocks += inode.data_blocks;
            metadata.checksum = checksum(metadata);
            fill_bytes(directory_block, 0, sizeof(directory_block));
            copy_bytes(directory_block, &metadata, sizeof(metadata));
            return device.write(0, directory_block);
        }
    }
    return false;
}

bool FileSystem::read_file(uint32_t inode_number, void* buffer, uint32_t capacity, uint32_t& size) const {
    Inode inode{};
    if (!is_mounted || !buffer || !::read_inode(*this, inode_number, inode) || inode.mode != KSFS_FILE_MODE || capacity < inode.size) return false;
    uint8_t block[KSFS_BLOCK_SIZE]{};
    for (uint32_t index = 0; index < inode.data_blocks; ++index) {
        if (!device.read(inode.blocks[index], block)) return false;
        uint32_t offset = index * KSFS_BLOCK_SIZE;
        uint32_t length = inode.size - offset;
        if (length > KSFS_BLOCK_SIZE) length = KSFS_BLOCK_SIZE;
        copy_bytes(static_cast<uint8_t*>(buffer) + offset, block, length);
    }
    size = inode.size;
    return true;
}

bool FileSystem::list_directory(uint32_t inode_number, DirectoryVisitor visitor, void* context) const {
    Inode inode{};
    if (!is_mounted || !visitor || !::read_inode(*this, inode_number, inode) || inode.mode != KSFS_DIRECTORY_MODE) return false;
    uint8_t block[KSFS_BLOCK_SIZE]{};
    if (!device.read(inode.blocks[0], block)) return false;
    const auto* entries = reinterpret_cast<const DirectoryEntry*>(block);
    for (uint32_t index = 0; index < KSFS_BLOCK_SIZE / sizeof(DirectoryEntry); ++index)
        if (entries[index].inode != 0 && !visitor(entries[index], context)) return false;
    return true;
}
}
