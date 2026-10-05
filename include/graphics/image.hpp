#pragma once
#include <stdint.h>

namespace graphics {
struct RgbImage {
    uint32_t width;
    uint32_t height;
    uint32_t pixel_count;
};

bool decode_bmp(const void* data, uint32_t size, uint32_t* pixels, uint32_t pixel_capacity, RgbImage& image);
void show_image(const uint32_t* pixels, uint32_t width, uint32_t height);
}
