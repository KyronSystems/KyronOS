#include "kernel/console.hpp"
#include "graphics/graphics.hpp"

namespace {
volatile uint16_t* const vga = reinterpret_cast<volatile uint16_t*>(0xB8000);
uint8_t row = 0;
uint8_t column = 0;
uint8_t input_row = 0;
uint8_t input_column = 0;
uint32_t input_length = 0;
char input_text[80]{};
bool input_active = false;
bool input_selection_active = false;
bool mouse_dragging = false;
uint32_t selection_anchor = 0;
uint32_t selection_cursor = 0;
uint32_t mouse_x = 320;
uint32_t mouse_y = 200;
uint8_t mouse_buttons = 0;
uint32_t pointer_cell = 0;
uint16_t pointer_under = 0;
bool pointer_visible = false;
console::MouseClickHandler mouse_click_handler = nullptr;

uint32_t mouse_column() { return mouse_x / 9; }
uint32_t mouse_row() { return mouse_y / 16; }

void hide_mouse_pointer() {
    if (!pointer_visible) return;
    vga[pointer_cell] = pointer_under;
    pointer_visible = false;
}

void show_mouse_pointer() {
    uint32_t column = mouse_column();
    uint32_t row = mouse_row();
    if (column >= 80) column = 79;
    if (row >= 25) row = 24;
    pointer_cell = row * 80 + column;
    pointer_under = vga[pointer_cell];
    uint8_t attribute = static_cast<uint8_t>(pointer_under >> 8);
    uint8_t inverted = static_cast<uint8_t>((attribute << 4) | (attribute >> 4));
    vga[pointer_cell] = static_cast<uint16_t>(pointer_under & 0x00FF) | (static_cast<uint16_t>(inverted) << 8);
    pointer_visible = true;
}

void redraw_input() {
    for (uint32_t index = 0; index < input_length; ++index) {
        bool selected = input_selection_active
            && index >= (selection_anchor < selection_cursor ? selection_anchor : selection_cursor)
            && index < (selection_anchor < selection_cursor ? selection_cursor : selection_anchor);
        uint8_t attribute = selected ? 0x70 : 0x07;
        vga[input_row * 80 + input_column + index] = static_cast<uint8_t>(input_text[index])
            | (static_cast<uint16_t>(attribute) << 8);
    }
}

void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

void update_cursor() {
    uint16_t position = static_cast<uint16_t>(row * 80 + column);
    outb(0x3D4, 0x0F);
    outb(0x3D5, static_cast<uint8_t>(position));
    outb(0x3D4, 0x0E);
    outb(0x3D5, static_cast<uint8_t>(position >> 8));
    graphics::set_cursor(row, column, true);
}

void scroll() {
    for (uint8_t screen_row = 1; screen_row < 25; ++screen_row)
        for (uint8_t screen_column = 0; screen_column < 80; ++screen_column)
            vga[(screen_row - 1) * 80 + screen_column] = vga[screen_row * 80 + screen_column];
    for (uint8_t screen_column = 0; screen_column < 80; ++screen_column)
        vga[24 * 80 + screen_column] = 0x0700 | ' ';
    row = 24;
    column = 0;
    graphics::scroll();
}

void put(char character, uint8_t color) {
    hide_mouse_pointer();
    if (character == '\n') { ++row; column = 0; if (row >= 25) scroll(); update_cursor(); show_mouse_pointer(); return; }
    if (column >= 80) { ++row; column = 0; if (row >= 25) scroll(); }
    vga[row * 80 + column] = static_cast<uint16_t>(character) | (static_cast<uint16_t>(color) << 8);
    graphics::draw_cell(row, column, character, color);
    ++column;
    update_cursor();
    show_mouse_pointer();
}
}

namespace console {
void initialize(uint32_t multiboot_info) { graphics::initialize(multiboot_info); }
void clear() {
    hide_mouse_pointer();
    for (uint16_t index = 0; index < 80 * 25; ++index) vga[index] = 0x0700 | ' ';
    row = 0; column = 0;
    input_length = 0;
    graphics::clear();
    update_cursor();
    show_mouse_pointer();
}
void write(const char* text, uint8_t color) { while (*text) put(*text++, color); }
void write_line(const char* text, uint8_t color) { write(text, color); put('\n', color); }
void write_char(char character, uint8_t color) { put(character, color); }
void backspace() {
    if (column == 0) return;
    hide_mouse_pointer();
    --column;
    vga[row * 80 + column] = 0x0700 | ' ';
    graphics::draw_cell(row, column, ' ', 0x07);
    update_cursor();
    show_mouse_pointer();
}
uint32_t input_begin() {
    hide_mouse_pointer();
    if (column >= 79) { ++row; column = 0; if (row >= 25) scroll(); }
    input_row = row;
    input_column = column;
    input_length = 0;
    input_active = true;
    input_selection_active = false;
    mouse_dragging = false;
    show_mouse_pointer();
    return 80 - input_column;
}
void input_update(const char* text, uint32_t length, uint32_t cursor) {
    hide_mouse_pointer();
    uint32_t available = 80 - input_column;
    if (length > available) length = available;
    if (cursor > length) cursor = length;
    input_selection_active = false;
    uint32_t cells = input_length > length ? input_length : length;
    for (uint32_t index = 0; index < cells; ++index) {
        char character = index < length ? text[index] : ' ';
        uint32_t cell = input_column + index;
        vga[input_row * 80 + cell] = 0x0700 | static_cast<uint8_t>(character);
        graphics::draw_cell(input_row, cell, character, 0x07);
        if (index < sizeof(input_text)) input_text[index] = character;
    }
    input_length = length;
    uint32_t position = input_row * 80 + input_column + cursor;
    if (position == static_cast<uint32_t>(input_row + 1) * 80) {
        row = input_row;
        column = 80;
    } else {
        row = static_cast<uint8_t>(position / 80);
        column = static_cast<uint8_t>(position % 80);
    }
    update_cursor();
    show_mouse_pointer();
}
void input_end() {
    uint32_t position = input_row * 80 + input_column + input_length;
    if (position == static_cast<uint32_t>(input_row + 1) * 80) {
        row = input_row;
        column = 80;
    } else {
        row = static_cast<uint8_t>(position / 80);
        column = static_cast<uint8_t>(position % 80);
    }
    input_active = false;
    input_selection_active = false;
    mouse_dragging = false;
    update_cursor();
}
void mouse_event(int32_t delta_x, int32_t delta_y, uint8_t buttons) {
    hide_mouse_pointer();
    int32_t speed_x = 2 + (delta_x < 0 ? -delta_x : delta_x) / 8;
    int32_t speed_y = 2 + (delta_y < 0 ? -delta_y : delta_y) / 8;
    if (speed_x > 5) speed_x = 5;
    if (speed_y > 5) speed_y = 5;
    uint32_t screen_width = graphics::width();
    uint32_t screen_height = graphics::height();
    int32_t horizontal_limit = screen_width ? static_cast<int32_t>(screen_width - 1) : 719;
    int32_t vertical_limit = screen_height ? static_cast<int32_t>(screen_height - 1) : 399;
    int32_t next_x = static_cast<int32_t>(mouse_x) + delta_x * speed_x;
    int32_t next_y = static_cast<int32_t>(mouse_y) - delta_y * speed_y;
    if (next_x < 0) next_x = 0;
    if (next_x > horizontal_limit) next_x = horizontal_limit;
    if (next_y < 0) next_y = 0;
    if (next_y > vertical_limit) next_y = vertical_limit;
    mouse_x = static_cast<uint32_t>(next_x);
    mouse_y = static_cast<uint32_t>(next_y);
    graphics::set_mouse(mouse_x, mouse_y);
    bool left_down = (buttons & 1) != 0;
    bool was_left_down = (mouse_buttons & 1) != 0;
    bool left_pressed = left_down && !was_left_down;
    uint32_t cell_column = mouse_column();
    uint32_t cell_row = mouse_row();
    if (input_active && left_down && cell_row == input_row && cell_column >= input_column) {
        uint32_t index = cell_column - input_column;
        if (index > input_length) index = input_length;
        if (!was_left_down || !mouse_dragging) {
            selection_anchor = index;
            mouse_dragging = true;
        }
        selection_cursor = index;
        input_selection_active = true;
        redraw_input();
    }
    if (!left_down && was_left_down) mouse_dragging = false;
    mouse_buttons = buttons;
    show_mouse_pointer();
    if (left_pressed && mouse_click_handler) mouse_click_handler(mouse_x, mouse_y);
}
void set_mouse_click_handler(MouseClickHandler handler) { mouse_click_handler = handler; }
bool input_mouse_state(uint32_t& cursor, uint32_t& selection_start, uint32_t& selection_end) {
    if (!input_active) return false;
    uint32_t cell_column = mouse_column();
    cursor = cell_column > input_column ? cell_column - input_column : 0;
    if (cursor > input_length) cursor = input_length;
    if (!input_selection_active || selection_anchor == selection_cursor) return false;
    selection_start = selection_anchor < selection_cursor ? selection_anchor : selection_cursor;
    selection_end = selection_anchor < selection_cursor ? selection_cursor : selection_anchor;
    return true;
}
void editor_draw(const char* title, const char* text, uint32_t length, uint32_t cursor) {
    hide_mouse_pointer();
    for (uint16_t index = 0; index < 80; ++index) {
        uint8_t color = index < 40 ? 0x09 : 0x0D;
        vga[index] = static_cast<uint16_t>(' ') | (static_cast<uint16_t>(color) << 8);
    }
    uint32_t title_length = 0;
    while (title[title_length] && title_length < 72) {
        vga[3 + title_length] = static_cast<uint8_t>(title[title_length]) | (0x0F << 8);
        ++title_length;
    }
    for (uint16_t index = 80; index < 24 * 80; ++index) vga[index] = 0x0700 | ' ';
    for (uint16_t index = 24 * 80; index < 25 * 80; ++index) {
        uint8_t color = index % 80 < 40 ? 0x09 : 0x0D;
        vga[index] = static_cast<uint16_t>(' ') | (static_cast<uint16_t>(color) << 8);
    }
    const char status[] = "Ctrl+Shift+X: Save and close";
    for (uint32_t index = 0; status[index]; ++index) vga[24 * 80 + 2 + index] = static_cast<uint8_t>(status[index]) | (0x0F << 8);
    uint32_t position = 0;
    for (uint32_t index = 0; index < length && position < 23 * 80; ++index) {
        if (text[index] == '\n') position = ((position / 80) + 1) * 80;
        else vga[80 + position++] = 0x0700 | static_cast<uint8_t>(text[index]);
    }
    if (cursor > length) cursor = length;
    position = 0;
    for (uint32_t index = 0; index < cursor && position < 23 * 80; ++index) {
        if (text[index] == '\n') position = ((position / 80) + 1) * 80;
        else ++position;
    }
    row = static_cast<uint8_t>(1 + position / 80);
    column = static_cast<uint8_t>(position % 80);
    update_cursor();
    for (uint32_t screen_row = 0; screen_row < 25; ++screen_row)
        for (uint32_t screen_column = 0; screen_column < 80; ++screen_column) {
            uint16_t cell = vga[screen_row * 80 + screen_column];
            graphics::draw_cell(screen_row, screen_column, static_cast<char>(cell & 0xFF), static_cast<uint8_t>(cell >> 8));
        }
    show_mouse_pointer();
}
}
