#include "graphics/image.hpp"
#include "graphics/graphics.hpp"
#include <stddef.h>

namespace {
uint16_t read_u16(const uint8_t* bytes) {
    return static_cast<uint16_t>(bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8));
}

uint32_t read_u32(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) |
        (static_cast<uint32_t>(bytes[1]) << 8) |
        (static_cast<uint32_t>(bytes[2]) << 16) |
        (static_cast<uint32_t>(bytes[3]) << 24);
}
}

namespace graphics {
bool decode_bmp(const void* data, uint32_t size, uint32_t* pixels, uint32_t pixel_capacity, RgbImage& image) {
    image = {};
    if (!data || !pixels || size < 54) return false;
    const auto* bytes = static_cast<const uint8_t*>(data);
    if (bytes[0] != 'B' || bytes[1] != 'M') return false;
    uint32_t pixel_offset = read_u32(bytes + 10);
    uint32_t dib_size = read_u32(bytes + 14);
    int32_t signed_width = static_cast<int32_t>(read_u32(bytes + 18));
    int32_t signed_height = static_cast<int32_t>(read_u32(bytes + 22));
    uint16_t planes = read_u16(bytes + 26);
    uint16_t bits_per_pixel = read_u16(bytes + 28);
    uint32_t compression = read_u32(bytes + 30);
    if (dib_size < 40 || dib_size > size - 14 || signed_width <= 0 || signed_height == 0 ||
        signed_height == static_cast<int32_t>(0x80000000u) || planes != 1 ||
        (bits_per_pixel != 24 && bits_per_pixel != 32) || compression != 0 || pixel_offset > size) return false;
    uint32_t width = static_cast<uint32_t>(signed_width);
    uint32_t height = static_cast<uint32_t>(signed_height < 0 ? -signed_height : signed_height);
    if (width > pixel_capacity || height > pixel_capacity / width) return false;
    uint32_t bytes_per_pixel = bits_per_pixel / 8;
    if (width > (0xFFFFFFFFu - 31u) / bits_per_pixel) return false;
    uint32_t row_stride = ((width * bits_per_pixel + 31u) / 32u) * 4u;
    if (height > (0xFFFFFFFFu - pixel_offset) / row_stride) return false;
    if (pixel_offset + row_stride * height > size) return false;
    for (uint32_t y = 0; y < height; ++y) {
        uint32_t source_y = signed_height > 0 ? height - 1 - y : y;
        const uint8_t* row = bytes + pixel_offset + source_y * row_stride;
        for (uint32_t x = 0; x < width; ++x) {
            const uint8_t* source = row + x * bytes_per_pixel;
            pixels[y * width + x] = (static_cast<uint32_t>(source[2]) << 16) |
                (static_cast<uint32_t>(source[1]) << 8) | source[0];
        }
    }
    image.width = width;
    image.height = height;
    image.pixel_count = width * height;
    return true;
}

void show_image(const uint32_t* pixels, uint32_t image_width, uint32_t image_height) {
    if (!pixels || image_width == 0 || image_height == 0 || width() == 0 || height() == 0) return;
    uint32_t screen_width = width();
    uint32_t screen_height = height();
    uint32_t available_height = screen_height > 100 ? screen_height - 100 : screen_height;
    uint32_t draw_width = screen_width;
    uint32_t draw_height = image_height * draw_width / image_width;
    if (draw_height > available_height) {
        draw_height = available_height;
        draw_width = image_width * draw_height / image_height;
    }
    if (draw_width == 0 || draw_height == 0) return;
    uint32_t left = (screen_width - draw_width) / 2;
    uint32_t top = (screen_height - draw_height) / 2;
    for (uint32_t y = 0; y < screen_height; ++y) {
        for (uint32_t x = 0; x < screen_width; ++x) {
            uint32_t image_x = x < left || x >= left + draw_width ? 0 : (x - left) * image_width / draw_width;
            uint32_t image_y = y < top || y >= top + draw_height ? 0 : (y - top) * image_height / draw_height;
            uint32_t color = pixels[image_y * image_width + image_x];
            if (x < left || x >= left + draw_width || y < top || y >= top + draw_height) color = 0x121A26;
            draw_pixel(x, y, color);
        }
    }
}
}
