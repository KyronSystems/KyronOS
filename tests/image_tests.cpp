#include "graphics/image.hpp"
#include <cassert>
#include <stdint.h>

namespace graphics {
uint32_t width() { return 0; }
uint32_t height() { return 0; }
void draw_pixel(uint32_t, uint32_t, uint32_t) {}
}

int main() {
    uint8_t bitmap[58] = {
        'B','M',58,0,0,0,0,0,0,0,54,0,0,0,40,0,0,0,
        1,0,0,0,1,0,0,0,1,0,24,0,0,0,0,0,4,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x56,0x34,0x12,0
    };
    uint32_t pixels[1]{};
    graphics::RgbImage image{};
    assert(graphics::decode_bmp(bitmap, sizeof(bitmap), pixels, 1, image));
    assert(image.width == 1 && image.height == 1 && image.pixel_count == 1);
    assert(pixels[0] == 0x123456);
    assert(!graphics::decode_bmp(bitmap, sizeof(bitmap) - 1, pixels, 1, image));
    assert(!graphics::decode_bmp(bitmap, sizeof(bitmap), pixels, 0, image));
    return 0;
}
