#include "drivers/usb_keyboard.hpp"

namespace {
char usage_to_key(uint8_t usage, bool shifted) {
    if (usage >= 0x04 && usage <= 0x1D) { char key = static_cast<char>('a' + usage - 0x04); return shifted ? static_cast<char>(key - 'a' + 'A') : key; }
    if (usage >= 0x1E && usage <= 0x27) return shifted ? "!@#$%^&*()"[usage - 0x1E] : "1234567890"[usage - 0x1E];
    if (usage == 0x28) return '\n';
    if (usage == 0x2C) return ' ';
    if (usage == 0x2D) return shifted ? '_' : '-';
    if (usage == 0x2E) return shifted ? '+' : '=';
    if (usage == 0x2F) return shifted ? '{' : '[';
    if (usage == 0x30) return shifted ? '}' : ']';
    if (usage == 0x33) return shifted ? ':' : ';';
    if (usage == 0x34) return shifted ? '"' : '\'';
    if (usage == 0x35) return shifted ? '~' : '`';
    if (usage == 0x36) return shifted ? '<' : ',';
    if (usage == 0x37) return shifted ? '>' : '.';
    if (usage == 0x38) return shifted ? '?' : '/';
    return 0;
}
}

namespace usb::keyboard {
bool decode_boot_report(const uint8_t* report, uint32_t length, Sink& sink) {
    if (!report || length < 8) return false;
    bool shifted = (report[0] & 0x22) != 0;
    for (uint32_t index = 2; index < 8; ++index) {
        char key = usage_to_key(report[index], shifted);
        if (key) sink.key(key);
    }
    return true;
}
}

namespace usb::mouse {
bool decode_boot_report(const uint8_t* report, uint32_t length, Sink& sink) {
    if (!report || length < 3) return false;
    int32_t delta_x = static_cast<int8_t>(report[1]);
    int32_t delta_y = static_cast<int8_t>(report[2]);
    int8_t wheel = length >= 4 ? static_cast<int8_t>(report[3]) : 0;
    sink.movement(delta_x, delta_y, report[0], wheel);
    return true;
}
}
