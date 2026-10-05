#include "drivers/usb_keyboard.hpp"
#include <cassert>
#include <stdint.h>

class MouseSink final : public usb::mouse::Sink {
public:
    void movement(int32_t x, int32_t y, uint8_t value, int8_t wheel_value) override {
        delta_x = x;
        delta_y = y;
        buttons = value;
        wheel = wheel_value;
    }
    int32_t delta_x = 0;
    int32_t delta_y = 0;
    uint8_t buttons = 0;
    int8_t wheel = 0;
};

int main() {
    const uint8_t report[4] = {0x15, 0xFE, 0x03, 0xFF};
    MouseSink sink;
    assert(usb::mouse::decode_boot_report(report, sizeof(report), sink));
    assert(sink.buttons == 0x15);
    assert(sink.delta_x == -2);
    assert(sink.delta_y == 3);
    assert(sink.wheel == -1);
    assert(!usb::mouse::decode_boot_report(report, 2, sink));
    return 0;
}
