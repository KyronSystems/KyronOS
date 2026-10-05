#include "fs/installer.hpp"
#include "fs/ksfs.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

class FileDevice final : public kyron::fs::BlockDevice {
public:
    FileDevice(const char* path, uint64_t blocks, uint64_t first_block)
        : handle(std::fopen(path, first_block == 0 ? "w+b" : "r+b")), start(first_block), count(blocks) {
        if (!handle || blocks == 0) return;
        uint64_t end_offset = (start + blocks) * kyron::fs::KSFS_BLOCK_SIZE;
        if (std::fseek(handle, static_cast<long>(end_offset - 1), SEEK_SET) != 0) return;
        uint8_t zero = 0;
        if (std::fwrite(&zero, 1, 1, handle) != 1) return;
        std::fflush(handle);
        valid = true;
    }

    ~FileDevice() override { if (handle) std::fclose(handle); }
    bool read(uint64_t block, void* buffer) override {
        if (!valid || block >= count || std::fseek(handle, static_cast<long>((start + block) * kyron::fs::KSFS_BLOCK_SIZE), SEEK_SET) != 0) return false;
        return std::fread(buffer, kyron::fs::KSFS_BLOCK_SIZE, 1, handle) == 1;
    }
    bool write(uint64_t block, const void* buffer) override {
        if (!valid || block >= count || std::fseek(handle, static_cast<long>((start + block) * kyron::fs::KSFS_BLOCK_SIZE), SEEK_SET) != 0) return false;
        if (std::fwrite(buffer, kyron::fs::KSFS_BLOCK_SIZE, 1, handle) != 1) return false;
        std::fflush(handle);
        return true;
    }
    uint64_t block_count() const override { return count; }
    bool ready() const { return valid; }

private:
    std::FILE* handle = nullptr;
    uint64_t start = 0;
    uint64_t count = 0;
    bool valid = false;
};

int main(int argc, char** argv) {
    if (argc < 2 || argc > 4) {
        std::fprintf(stderr, "usage: ksfs-mkfs <image> [blocks] [first-block]\n");
        return 2;
    }
    uint64_t blocks = argc == 3 ? std::strtoull(argv[2], nullptr, 10) : 1024;
    if (argc >= 4) blocks = std::strtoull(argv[2], nullptr, 10);
    uint64_t first_block = argc >= 4 ? std::strtoull(argv[3], nullptr, 10) : 0;
    if (blocks < 16) {
        std::fprintf(stderr, "image must contain at least 16 blocks\n");
        return 2;
    }
    FileDevice device(argv[1], blocks, first_block);
    if (!device.ready() || !kyron::fs::Installer::install(device)) {
        std::fprintf(stderr, "unable to install KSFS image: %s\n", argv[1]);
        return 1;
    }
    std::printf("installed KyronOS filesystem: %s (%llu blocks, offset %llu)\n", argv[1],
                static_cast<unsigned long long>(blocks), static_cast<unsigned long long>(first_block));
    return 0;
}
