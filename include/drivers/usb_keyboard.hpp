#pragma once
#include <stdint.h>

namespace usb::keyboard {
class Sink {
public:
    virtual ~Sink() = default;
    virtual void key(char character) = 0;
};

bool decode_boot_report(const uint8_t* report, uint32_t length, Sink& sink);
}

namespace usb::mouse {
class Sink {
public:
    virtual ~Sink() = default;
    virtual void movement(int32_t delta_x, int32_t delta_y, uint8_t buttons, int8_t wheel) = 0;
};

bool decode_boot_report(const uint8_t* report, uint32_t length, Sink& sink);
}
