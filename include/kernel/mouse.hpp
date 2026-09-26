#pragma once
#include <stdint.h>

namespace mouse {
bool initialize();
void consume_data(uint8_t value);
}