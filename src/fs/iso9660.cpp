#include "fs/iso9660.hpp"
#include <stddef.h>
#include <stdint.h>

namespace {
constexpr uint32_t iso_sector_size = 2048;
constexpr uint32_t device_block_size = 4096;
constexpr uint32_t kernel_slot_size = 512 * 1024;
constexpr uint64_t boot_region_size = 64ull * 1024 * 1024;

uint32_t read_u32(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) |
        (static_cast<uint32_t>(bytes[1]) << 8) |
        (static_cast<uint32_t>(bytes[2]) << 16) |
        (static_cast<uint32_t>(bytes[3]) << 24);
}

void copy_bytes(void* destination, const void* source, uint32_t size) {
    auto* out = static_cast<uint8_t*>(destination);
    const auto* in = static_cast<const uint8_t*>(source);
    for (uint32_t index = 0; index < size; ++index) out[index] = in[index];
}

bool read_sector(kyron::fs::BlockDevice& device, uint32_t sector, uint8_t* output) {
    uint8_t block[device_block_size];
    uint64_t block_number = sector / 2;
    if (block_number >= device.block_count() || !device.read(block_number, block)) return false;
    copy_bytes(output, block + (sector % 2) * iso_sector_size, iso_sector_size);
    return true;
}

bool equal_identifier(const uint8_t* identifier, uint8_t length, const char* expected) {
    uint32_t expected_length = 0;
    while (expected[expected_length]) ++expected_length;
    if (length < expected_length) return false;
    for (uint32_t index = 0; index < expected_length; ++index) {
        uint8_t value = identifier[index];
        if (value >= 'a' && value <= 'z') value = static_cast<uint8_t>(value - 'a' + 'A');
        if (value != static_cast<uint8_t>(expected[index])) return false;
    }
    return length == expected_length || identifier[expected_length] == ';';
}

bool find_entry(kyron::fs::BlockDevice& device, uint32_t extent, uint32_t size, const char* name,
                uint32_t& result_extent, uint32_t& result_size, bool& is_directory) {
    uint8_t sector_data[iso_sector_size];
    uint32_t sector_count = (size + iso_sector_size - 1) / iso_sector_size;
    for (uint32_t sector_index = 0; sector_index < sector_count; ++sector_index) {
        if (!read_sector(device, extent + sector_index, sector_data)) return false;
        uint32_t position = 0;
        while (position < iso_sector_size) {
            uint8_t record_size = sector_data[position];
            if (record_size == 0) break;
            if (record_size < 34 || position + record_size > iso_sector_size) return false;
            const uint8_t* record = sector_data + position;
            uint8_t identifier_length = record[32];
            if (33u + identifier_length > record_size) return false;
            if (equal_identifier(record + 33, identifier_length, name)) {
                result_extent = read_u32(record + 2);
                result_size = read_u32(record + 10);
                is_directory = (record[25] & 2) != 0;
                return true;
            }
            position += record_size;
        }
    }
    return false;
}
}

namespace kyron::fs {
bool install_hybrid_boot_image(BlockDevice& device, const void* image, uint32_t image_size) {
    if (!image || image_size < 17 * iso_sector_size || image_size > boot_region_size ||
        image_size > device.block_count() * device_block_size) return false;
    const auto* source = static_cast<const uint8_t*>(image);
    const uint8_t* pvd = source + 16 * iso_sector_size;
    if (source[510] != 0x55 || source[511] != 0xAA || pvd[0] != 1 || pvd[6] != 1 ||
        pvd[1] != 'C' || pvd[2] != 'D' || pvd[3] != '0' || pvd[4] != '0' || pvd[5] != '1') return false;
    uint8_t block[device_block_size];
    uint32_t block_count = (image_size + device_block_size - 1) / device_block_size;
    for (uint32_t index = 0; index < block_count; ++index) {
        uint32_t offset = index * device_block_size;
        uint32_t chunk = image_size - offset;
        if (chunk > device_block_size) chunk = device_block_size;
        for (uint32_t byte = 0; byte < device_block_size; ++byte) block[byte] = byte < chunk ? source[offset + byte] : 0;
        if (!device.write(index, block)) return false;
    }
    return true;
}

bool update_embedded_kernel(BlockDevice& device, const void* kernel_image, uint32_t image_size) {
    if (!kernel_image || image_size == 0 || image_size > kernel_slot_size || device.block_count() < 10) return false;
    const auto* image = static_cast<const uint8_t*>(kernel_image);
    if (image_size < 4 || image[0] != 0x7F || image[1] != 'E' || image[2] != 'L' || image[3] != 'F') return false;
    uint8_t descriptor[iso_sector_size];
    if (!read_sector(device, 16, descriptor) || descriptor[0] != 1 || descriptor[6] != 1 ||
        descriptor[1] != 'C' || descriptor[2] != 'D' || descriptor[3] != '0' ||
        descriptor[4] != '0' || descriptor[5] != '1') return false;
    const uint8_t* root_record = descriptor + 156;
    if (root_record[0] < 34 || (root_record[25] & 2) == 0) return false;
    uint32_t root_extent = read_u32(root_record + 2);
    uint32_t root_size = read_u32(root_record + 10);
    uint32_t boot_extent = 0;
    uint32_t boot_size = 0;
    uint32_t kernel_extent = 0;
    uint32_t kernel_size = 0;
    bool is_directory = false;
    if (!find_entry(device, root_extent, root_size, "BOOT", boot_extent, boot_size, is_directory) || !is_directory ||
        !find_entry(device, boot_extent, boot_size, "KYRONOS.KERNEL", kernel_extent, kernel_size, is_directory) ||
        is_directory || kernel_size < kernel_slot_size || image_size > kernel_size) return false;

    uint8_t block[device_block_size];
    const auto* source = image;
    uint64_t byte_offset = static_cast<uint64_t>(kernel_extent) * iso_sector_size;
    uint32_t written = 0;
    while (written < kernel_size) {
        uint64_t block_number = (byte_offset + written) / device_block_size;
        uint32_t block_offset = static_cast<uint32_t>((byte_offset + written) % device_block_size);
        uint32_t chunk = device_block_size - block_offset;
        if (chunk > kernel_size - written) chunk = kernel_size - written;
        if (!device.read(block_number, block)) return false;
        for (uint32_t index = 0; index < chunk; ++index)
            block[block_offset + index] = written + index < image_size ? source[written + index] : 0;
        if (!device.write(block_number, block)) return false;
        written += chunk;
    }
    return true;
}
}
