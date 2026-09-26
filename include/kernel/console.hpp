#pragma once
#include <stdint.h>

namespace console {
void initialize(uint32_t multiboot_info);
void clear();
void write(const char* text, uint8_t color = 0x07);
void write_line(const char* text, uint8_t color = 0x07);
void write_char(char character, uint8_t color = 0x07);
void backspace();
uint32_t input_begin();
void input_update(const char* text, uint32_t length, uint32_t cursor);
void input_end();
void mouse_event(int32_t delta_x, int32_t delta_y, uint8_t buttons);
bool input_mouse_state(uint32_t& cursor, uint32_t& selection_start, uint32_t& selection_end);
void editor_draw(const char* title, const char* text, uint32_t length, uint32_t cursor);
}
