#include "kernel/keyboard.hpp"
#include "kernel/console.hpp"
#include "kernel/mouse.hpp"

namespace {
bool shift_down = false;
bool control_down = false;
bool extended_scan = false;
constexpr uint32_t history_size = 8;
constexpr uint32_t history_line_size = 128;
char history[history_size][history_line_size]{};
uint32_t history_count = 0;
uint32_t history_next = 0;
char clipboard[128]{};
uint32_t clipboard_length = 0;

uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

char translate(uint8_t scan) {
    if (scan == 0x0E) return '\b';
    if (scan == 0x1C) return '\n';
    if (scan == 0x0F) return '\t';
    if (scan == 0x39) return ' ';
    if (scan >= 0x02 && scan <= 0x0D) return shift_down ? "!@#$%^&*()_+"[scan - 0x02] : "1234567890-="[scan - 0x02];
    if (scan >= 0x10 && scan <= 0x19) { char key = "qwertyuiop"[scan - 0x10]; return shift_down ? key - 'a' + 'A' : key; }
    if (scan == 0x1A) return shift_down ? '{' : '[';
    if (scan == 0x1B) return shift_down ? '}' : ']';
    if (scan >= 0x1E && scan <= 0x26) { char key = "asdfghjkl"[scan - 0x1E]; return shift_down ? key - 'a' + 'A' : key; }
    if (scan == 0x27) return shift_down ? ':' : ';';
    if (scan == 0x28) return shift_down ? '"' : '\'';
    if (scan == 0x29) return shift_down ? '~' : '`';
    if (scan == 0x2B) return shift_down ? '|' : '\\';
    if (scan >= 0x2C && scan <= 0x35) return shift_down ? "ZXCVBNM<>?"[scan - 0x2C] : "zxcvbnm,./"[scan - 0x2C];
    return 0;
}
}

namespace keyboard {
uint16_t read_key() {
    uint8_t scan = 0;
    while (true) {
        uint8_t status = inb(0x64);
        if ((status & 1) == 0) continue;
        uint8_t value = inb(0x60);
        if (status & 0x20) { mouse::consume_data(value); continue; }
        scan = value;
        break;
    }
    if (scan == 0xE0) { extended_scan = true; return 0; }
    bool extended = extended_scan;
    extended_scan = false;
    bool released = (scan & 0x80) != 0;
    scan &= 0x7F;
    if (extended) {
        if (released) return 0;
        if (scan == 0x4B) return key_left;
        if (scan == 0x4D) return key_right;
        if (scan == 0x48) return key_up;
        if (scan == 0x50) return key_down;
        if (scan == 0x53) return key_delete;
        return 0;
    }
    if (scan == 0x1D) { control_down = !released; return 0; }
    if (released) {
        if (scan == 0x2A || scan == 0x36) shift_down = false;
        return 0;
    }
    if (scan == 0x2A || scan == 0x36) { shift_down = true; return 0; }
    char character = translate(scan);
    if (control_down && (character == 'c' || character == 'C')) return key_ctrl_c;
    if (control_down && (character == 'v' || character == 'V')) return key_ctrl_v;
    if (control_down && shift_down && (character == 'x' || character == 'X')) return key_ctrl_shift_x;
    return static_cast<uint8_t>(character);
}

void read_line(char* buffer, uint32_t capacity, Completion completion) {
    if (capacity == 0) return;
    uint32_t available = console::input_begin();
    uint32_t line_capacity = capacity;
    if (line_capacity > available) line_capacity = available;
    if (line_capacity == 0) line_capacity = 1;
    uint32_t length = 0;
    uint32_t cursor = 0;
    uint32_t history_offset = 0;
    char saved_line[history_line_size]{};
    buffer[0] = 0;
    while (true) {
        uint16_t key = read_key();
        if (key == 0) continue;
        uint32_t mouse_cursor = cursor;
        uint32_t selection_start = 0;
        uint32_t selection_end = 0;
        bool selected = console::input_mouse_state(mouse_cursor, selection_start, selection_end);
        if (mouse_cursor <= length) cursor = mouse_cursor;
        if (key == key_ctrl_c) {
            if (selected) {
                clipboard_length = selection_end - selection_start;
                if (clipboard_length >= sizeof(clipboard)) clipboard_length = sizeof(clipboard) - 1;
                for (uint32_t index = 0; index < clipboard_length; ++index) clipboard[index] = buffer[selection_start + index];
                clipboard[clipboard_length] = 0;
            } else {
                clipboard_length = length < sizeof(clipboard) - 1 ? length : sizeof(clipboard) - 1;
                for (uint32_t index = 0; index < clipboard_length; ++index) clipboard[index] = buffer[index];
                clipboard[clipboard_length] = 0;
            }
            continue;
        }
        if (selected && key != key_ctrl_v) {
            if (key == key_left) { cursor = selection_start; key = 0; }
            else if (key == key_right) { cursor = selection_end; key = 0; }
            else {
                for (uint32_t index = selection_end; index <= length; ++index)
                    buffer[selection_start + index - selection_end] = buffer[index];
                length -= selection_end - selection_start;
                cursor = selection_start;
            }
        }
        if (key == '\n') {
            buffer[length] = '\0';
            if (length > 0 && length < history_line_size) {
                uint32_t previous = (history_next + history_size - 1) % history_size;
                bool duplicate = history_count > 0;
                for (uint32_t index = 0; duplicate && index < length; ++index)
                    if (history[previous][index] != buffer[index]) duplicate = false;
                if (duplicate && history[previous][length] != 0) duplicate = false;
                if (!duplicate) {
                    for (uint32_t index = 0; index <= length; ++index) history[history_next][index] = buffer[index];
                    history_next = (history_next + 1) % history_size;
                    if (history_count < history_size) ++history_count;
                }
            }
            console::input_end();
            console::write_line("");
            return;
        }
        if (key == key_left) { if (cursor > 0) --cursor; }
        else if (key == key_right) { if (cursor < length) ++cursor; }
        else if (key == key_up || key == key_down) {
            if (history_offset == 0) {
                uint32_t save_length = length < history_line_size - 1 ? length : history_line_size - 1;
                for (uint32_t index = 0; index < save_length; ++index) saved_line[index] = buffer[index];
                saved_line[save_length] = 0;
            }
            if (key == key_up && history_offset < history_count) ++history_offset;
            if (key == key_down && history_offset > 0) --history_offset;
            if (history_offset == 0) {
                length = 0;
                while (saved_line[length] && length + 1 < line_capacity) { buffer[length] = saved_line[length]; ++length; }
            } else {
                uint32_t slot = (history_next + history_size - history_offset) % history_size;
                length = 0;
                while (history[slot][length] && length + 1 < line_capacity) { buffer[length] = history[slot][length]; ++length; }
            }
            cursor = length;
        }
        else if (key == '\b') {
            if (cursor > 0) {
                for (uint32_t index = cursor - 1; index < length; ++index) buffer[index] = buffer[index + 1];
                --cursor;
                --length;
            }
        }
        else if (key == key_delete) {
            if (cursor < length) {
                for (uint32_t index = cursor; index < length; ++index) buffer[index] = buffer[index + 1];
                --length;
            }
        }
        else if (key == '\t') {
            if (completion && cursor == length) {
                length = completion(buffer, length, line_capacity);
                if (length >= line_capacity) length = line_capacity - 1;
                cursor = length;
            }
        }
        else if (key == key_ctrl_v) {
            if (selected) {
                for (uint32_t index = selection_end; index <= length; ++index)
                    buffer[selection_start + index - selection_end] = buffer[index];
                length -= selection_end - selection_start;
                cursor = selection_start;
            }
            uint32_t paste_length = clipboard_length;
            if (paste_length > line_capacity - 1 - length) paste_length = line_capacity - 1 - length;
            for (uint32_t index = length; paste_length > 0 && index > cursor; --index)
                buffer[index + paste_length - 1] = buffer[index - 1];
            for (uint32_t index = 0; index < paste_length; ++index) buffer[cursor + index] = clipboard[index];
            length += paste_length;
            cursor += paste_length;
        }
        else if (key < 0x100 && key != '\n' && key != '\t' && key != 0 && length + 1 < line_capacity) {
            for (uint32_t index = length; index > cursor; --index) buffer[index] = buffer[index - 1];
            buffer[cursor++] = static_cast<char>(key);
            ++length;
        }
        buffer[length] = 0;
        console::input_update(buffer, length, cursor);
    }
}
}
