#pragma once
#include <stdint.h>

namespace graphics {
bool initialize(uint32_t multiboot_info);
void draw_pixel(uint32_t x, uint32_t y, uint32_t rgb);
void clear();
void draw_cell(uint32_t row, uint32_t column, char character, uint8_t attribute);
void scroll();
void set_cursor(uint32_t row, uint32_t column, bool visible);
void hide_mouse();
void show_mouse();
void set_mouse(uint32_t x, uint32_t y);
uint32_t width();
uint32_t height();
void mouse_cell(uint32_t x, uint32_t y, uint32_t& column, uint32_t& row);
}