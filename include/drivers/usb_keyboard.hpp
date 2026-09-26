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
