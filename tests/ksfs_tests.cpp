#include "fs/ksfs.hpp"
#include "fs/filesystem.hpp"
#include "fs/installer.hpp"
#include <array>
#include <cassert>
#include <cstring>

class MemoryDevice final : public kyron::fs::BlockDevice {
    std::array<std::array<uint8_t, kyron::fs::KSFS_BLOCK_SIZE>, 64> blocks{};
public:
    bool read(uint64_t block, void* buffer) override {
        if (block >= blocks.size()) return false;
        std::memcpy(buffer, blocks[block].data(), kyron::fs::KSFS_BLOCK_SIZE); return true;
    }
    bool write(uint64_t block, const void* buffer) override {
        if (block >= blocks.size()) return false;
        std::memcpy(blocks[block].data(), buffer, kyron::fs::KSFS_BLOCK_SIZE); return true;
    }
    uint64_t block_count() const override { return blocks.size(); }
};

int main() {
    MemoryDevice device;
    assert(kyron::fs::format(device));
    kyron::fs::Superblock superblock{};
    assert(kyron::fs::mount(device, superblock));
    assert(superblock.magic == kyron::fs::KSFS_MAGIC);
    assert(superblock.inode_table_blocks == 2);
    assert(superblock.free_blocks == 60);

    kyron::fs::FileSystem filesystem(device);
    assert(filesystem.format(32));
    uint32_t documents = 0;
    assert(filesystem.create_directory(kyron::fs::KSFS_ROOT_INODE, "documents", documents));
    uint32_t projects = 0;
    assert(filesystem.create_directory(documents, "projects", projects));
    uint32_t note = 0;
    assert(filesystem.create_file(documents, "note.txt", "persistent", 10, note));
    uint32_t todo = 0;
    assert(filesystem.create_file(projects, "todo.txt", "planned", 7, todo));

    uint32_t resolved_documents = 0;
    assert(filesystem.resolve_path(kyron::fs::KSFS_ROOT_INODE, "/documents", resolved_documents));
    assert(resolved_documents == documents);
    uint32_t resolved_project_file = 0;
    assert(filesystem.resolve_path(documents, "projects/todo.txt", resolved_project_file));
    assert(resolved_project_file == todo);

    kyron::fs::FileSystem remounted(device);
    assert(remounted.mount());
    uint32_t found = 0;
    assert(remounted.find(documents, "note.txt", found));
    assert(found == note);
    char contents[16]{};
    uint32_t size = 0;
    assert(remounted.read_file(found, contents, sizeof(contents), size));
    assert(size == 10);
    assert(std::strcmp(contents, "persistent") == 0);
    char large_data[5000];
    std::memset(large_data, 'K', sizeof(large_data));
    uint32_t large_file = 0;
    assert(remounted.create_file(documents, "large.bin", large_data, sizeof(large_data), large_file));
    char large_contents[5000];
    assert(remounted.read_file(large_file, large_contents, sizeof(large_contents), size));
    assert(size == sizeof(large_contents));
    assert(std::memcmp(large_data, large_contents, sizeof(large_data)) == 0);
    assert(remounted.remove(documents, "large.bin"));
    assert(!remounted.find(documents, "large.bin", found));

    MemoryDevice installed_device;
    assert(kyron::fs::Installer::install(installed_device, "kyron-host"));
    kyron::fs::FileSystem installed(installed_device);
    assert(installed.mount());
    uint32_t etc = 0;
    assert(installed.find(kyron::fs::KSFS_ROOT_INODE, "etc", etc));
    uint32_t hostname = 0;
    assert(installed.find(etc, "hostname", hostname));
    char hostname_contents[32]{};
    assert(installed.read_file(hostname, hostname_contents, sizeof(hostname_contents), size));
    assert(std::strcmp(hostname_contents, "kyron-host") == 0);
    return 0;
}
