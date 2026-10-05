#include "fs/iso9660.hpp"
#include <array>
#include <cassert>
#include <cstring>
#include <stdint.h>

class MemoryDevice final : public kyron::fs::BlockDevice {
    std::array<std::array<uint8_t, 4096>, 160> blocks{};
public:
    bool read(uint64_t block, void* buffer) override {
        if (block >= blocks.size()) return false;
        std::memcpy(buffer, blocks[block].data(), 4096);
        return true;
    }
    bool write(uint64_t block, const void* buffer) override {
        if (block >= blocks.size()) return false;
        std::memcpy(blocks[block].data(), buffer, 4096);
        return true;
    }
    uint64_t block_count() const override { return blocks.size(); }
};

static void set_u32(uint8_t* bytes, uint32_t value) {
    bytes[0] = static_cast<uint8_t>(value);
    bytes[1] = static_cast<uint8_t>(value >> 8);
    bytes[2] = static_cast<uint8_t>(value >> 16);
    bytes[3] = static_cast<uint8_t>(value >> 24);
}

static uint8_t* iso_sector(MemoryDevice& device, uint32_t sector, std::array<uint8_t, 4096>& block) {
    assert(device.read(sector / 2, block.data()));
    return block.data() + (sector % 2) * 2048;
}

static uint32_t directory_record(uint8_t* out, uint32_t extent, uint32_t size, uint8_t flags,
                                 const char* name, uint8_t name_length) {
    uint32_t record_size = 33 + name_length + ((name_length % 2) == 0 ? 1 : 0);
    std::memset(out, 0, record_size);
    out[0] = static_cast<uint8_t>(record_size);
    set_u32(out + 2, extent);
    set_u32(out + 10, size);
    out[25] = flags;
    out[28] = 1;
    out[31] = 1;
    out[32] = name_length;
    std::memcpy(out + 33, name, name_length);
    return record_size;
}

int main() {
    MemoryDevice boot_device;
    std::array<uint8_t, 4096> protected_block{};
    protected_block.fill(0x3C);
    assert(boot_device.write(20, protected_block.data()));
    std::array<uint8_t, 40960> boot_image{};
    boot_image[510] = 0x55;
    boot_image[511] = 0xAA;
    uint8_t* boot_pvd = boot_image.data() + 16 * 2048;
    boot_pvd[0] = 1;
    std::memcpy(boot_pvd + 1, "CD001", 5);
    boot_pvd[6] = 1;
    assert(kyron::fs::install_hybrid_boot_image(boot_device, boot_image.data(), boot_image.size()));
    std::array<uint8_t, 4096> copied_boot{};
    assert(boot_device.read(0, copied_boot.data()));
    assert(copied_boot[510] == 0x55 && copied_boot[511] == 0xAA);
    std::array<uint8_t, 4096> still_protected{};
    assert(boot_device.read(20, still_protected.data()));
    assert(still_protected == protected_block);
    boot_image[510] = 0;
    assert(!kyron::fs::install_hybrid_boot_image(boot_device, boot_image.data(), boot_image.size()));

    MemoryDevice device;
    std::array<uint8_t, 4096> block{};
    uint8_t* pvd = iso_sector(device, 16, block);
    pvd[0] = 1;
    std::memcpy(pvd + 1, "CD001", 5);
    pvd[6] = 1;
    directory_record(pvd + 156, 20, 2048, 2, "\0", 1);
    assert(device.write(8, block.data()));

    uint8_t* root = iso_sector(device, 20, block);
    uint32_t root_entry_size = directory_record(root, 20, 2048, 2, "\0", 1);
    root_entry_size += directory_record(root + root_entry_size, 20, 2048, 2, "\1", 1);
    root_entry_size += directory_record(root + root_entry_size, 21, 2048, 2, "BOOT", 4);
    assert(device.write(10, block.data()));

    uint8_t* boot = iso_sector(device, 21, block);
    uint32_t boot_entry_size = directory_record(boot, 21, 2048, 2, "\0", 1);
    boot_entry_size += directory_record(boot + boot_entry_size, 20, 2048, 2, "\1", 1);
    const char kernel_name[] = "KYRONOS.KERNEL;1";
    directory_record(boot + boot_entry_size, 22, 512 * 1024, 0, kernel_name, sizeof(kernel_name) - 1);
    assert(device.write(10, block.data()));

    std::array<uint8_t, 4096> update{};
    update.fill(0xA7);
    update[0] = 0x7F;
    update[1] = 'E';
    update[2] = 'L';
    update[3] = 'F';
    assert(kyron::fs::update_embedded_kernel(device, update.data(), update.size()));
    std::array<uint8_t, 4096> updated_block{};
    assert(device.read(11, updated_block.data()));
    assert(updated_block[0] == 0x7F && updated_block[1] == 'E' && updated_block[2] == 'L' && updated_block[3] == 'F');
    for (uint32_t index = 4; index < updated_block.size(); ++index) assert(updated_block[index] == 0xA7);
    assert(!kyron::fs::update_embedded_kernel(device, update.data(), 512 * 1024 + 1));
    return 0;
}
