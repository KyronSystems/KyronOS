#pragma once
#include <stdint.h>

namespace keyboard {
constexpr uint16_t key_left = 0x100;
constexpr uint16_t key_right = 0x101;
constexpr uint16_t key_up = 0x102;
constexpr uint16_t key_down = 0x103;
constexpr uint16_t key_delete = 0x104;
constexpr uint16_t key_ctrl_c = 0x105;
constexpr uint16_t key_ctrl_v = 0x106;
constexpr uint16_t key_ctrl_shift_x = 0x107;
uint16_t read_key();
using Completion = uint32_t (*)(char* buffer, uint32_t length, uint32_t capacity);
void read_line(char* buffer, uint32_t capacity, Completion completion = nullptr);
}
