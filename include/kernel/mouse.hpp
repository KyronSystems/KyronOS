#pragma once
#include <stdint.h>

namespace mouse {
bool initialize();
void consume_data(uint8_t value);
bool consume_usb_report(const uint8_t* report, uint32_t length);
}