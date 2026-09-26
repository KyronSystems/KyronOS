#include "kernel/graphics.hpp"

namespace {
constexpr uint32_t columns = 80;
constexpr uint32_t rows = 25;
constexpr uint32_t font_width = 8;
constexpr uint32_t font_height = 16;
constexpr uint32_t background_width_limit = 960;
constexpr uint32_t background_height_limit = 540;
constexpr uint32_t pointer_width = 12;
constexpr uint32_t pointer_height = 16;

struct Framebuffer {
    volatile uint32_t* address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t red_position;
    uint8_t red_size;
    uint8_t green_position;
    uint8_t green_size;
    uint8_t blue_position;
    uint8_t blue_size;
    bool active;
};

Framebuffer framebuffer{};
uint32_t background_width = 0;
uint32_t background_height = 0;
uint32_t background[background_width_limit * background_height_limit];
uint32_t softened_background[background_width_limit * background_height_limit];
char characters[rows][columns]{};
uint8_t attributes[rows][columns]{};
uint32_t window_x = 0;
uint32_t window_y = 0;
uint32_t window_width = 0;
uint32_t window_height = 0;
uint32_t title_height = 0;
uint32_t content_x = 0;
uint32_t content_y = 0;
uint32_t cell_width = 1;
uint32_t cell_height = 1;
uint32_t glyph_scale = 1;
uint32_t cursor_row = 0;
uint32_t cursor_column = 0;
bool cursor_visible = false;
int32_t pointer_x = 0;
int32_t pointer_y = 0;
uint32_t pointer_saved[pointer_width * pointer_height]{};
bool pointer_visible = false;
uint8_t vga_font[256][font_height]{};
bool vga_font_loaded = false;

const uint8_t uppercase[36][font_height] = {
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14},
    {7,2,2,2,18,18,12}, {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4}, {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
    {14,17,17,17,17,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14}
};

const uint8_t lowercase[26][font_height] = {
    {0,0,14,1,15,17,15}, {16,16,30,17,17,17,30}, {0,0,14,16,16,17,14},
    {1,1,15,17,17,17,15}, {0,0,14,17,31,16,14}, {6,9,8,28,8,8,8},
    {0,0,15,17,15,1,14}, {16,16,30,17,17,17,17}, {4,0,12,4,4,4,14},
    {2,0,6,2,2,18,12}, {16,16,18,20,24,20,18}, {12,4,4,4,4,4,14},
    {0,0,26,21,21,21,21}, {0,0,30,17,17,17,17}, {0,0,14,17,17,17,14},
    {0,0,30,17,30,16,16}, {0,0,15,17,15,1,1}, {0,0,22,25,16,16,16},
    {0,0,15,16,14,1,30}, {8,8,28,8,8,9,6}, {0,0,17,17,17,19,13},
    {0,0,17,17,17,10,4}, {0,0,17,17,21,21,10}, {0,0,17,10,4,10,17},
    {0,0,17,17,15,1,14}, {0,0,31,2,4,8,31}
};

uint32_t scale_channel(uint8_t value, uint8_t size, uint8_t position) {
    if (size == 0 || size > 8 || position + size > 32) return 0;
    uint32_t maximum = (1u << size) - 1u;
    return ((static_cast<uint32_t>(value) * maximum + 127u) / 255u) << position;
}

uint32_t pack_rgb(uint8_t red, uint8_t green, uint8_t blue) {
    return scale_channel(red, framebuffer.red_size, framebuffer.red_position)
        | scale_channel(green, framebuffer.green_size, framebuffer.green_position)
        | scale_channel(blue, framebuffer.blue_size, framebuffer.blue_position);
}

uint32_t rgb_from_packed(uint32_t value) {
    uint32_t red_mask = (1u << framebuffer.red_size) - 1u;
    uint32_t green_mask = (1u << framebuffer.green_size) - 1u;
    uint32_t blue_mask = (1u << framebuffer.blue_size) - 1u;
    uint32_t red = (value >> framebuffer.red_position) & red_mask;
    uint32_t green = (value >> framebuffer.green_position) & green_mask;
    uint32_t blue = (value >> framebuffer.blue_position) & blue_mask;
    if (framebuffer.red_size != 8) red = red * 255 / red_mask;
    if (framebuffer.green_size != 8) green = green * 255 / green_mask;
    if (framebuffer.blue_size != 8) blue = blue * 255 / blue_mask;
    return (red << 16) | (green << 8) | blue;
}

void put_pixel(uint32_t x, uint32_t y, uint32_t rgb) {
    if (x >= framebuffer.width || y >= framebuffer.height) return;
    framebuffer.address[(y * framebuffer.pitch) / 4 + x] = pack_rgb(
        static_cast<uint8_t>(rgb >> 16), static_cast<uint8_t>(rgb >> 8), static_cast<uint8_t>(rgb));
}

uint32_t sample_background(const uint32_t* pixels, uint32_t x, uint32_t y) {
    uint32_t source_x = x * background_width / framebuffer.width;
    uint32_t source_y = y * background_height / framebuffer.height;
    if (source_x >= background_width) source_x = background_width - 1;
    if (source_y >= background_height) source_y = background_height - 1;
    return pixels[source_y * background_width + source_x];
}

uint32_t mix(uint32_t source, uint32_t tint, uint32_t alpha) {
    uint32_t inverse = 255 - alpha;
    uint32_t red = (((source >> 16) & 255) * inverse + ((tint >> 16) & 255) * alpha) / 255;
    uint32_t green = (((source >> 8) & 255) * inverse + ((tint >> 8) & 255) * alpha) / 255;
    uint32_t blue = ((source & 255) * inverse + (tint & 255) * alpha) / 255;
    return (red << 16) | (green << 8) | blue;
}

uint32_t background_at(uint32_t x, uint32_t y, bool blur) {
    return sample_background(blur ? softened_background : background, x, y);
}

uint32_t panel_color(uint32_t x, uint32_t y) {
    uint32_t left = window_x + 3;
    uint32_t right = window_x + window_width - 3;
    uint32_t top = window_y + 3;
    uint32_t bottom = window_y + window_height - 3;
    if (x < window_x || x >= window_x + window_width || y < window_y || y >= window_y + window_height)
        return background_at(x, y, false);
    if (x < left || x >= right || y < top || y >= bottom) {
        uint32_t t = (x - window_x) * 255 / window_width;
        return ((40 + t * 215 / 255) << 16) | ((130 - t * 55 / 255) << 8) | (245 - t * 5 / 255);
    }
    if (y < window_y + title_height) return mix(background_at(x, y, false), 0x252A3A, 225);
    return mix(background_at(x, y, true), 0x111725, 178);
}

uint8_t color_channel(uint8_t attribute, uint8_t channel) {
    static const uint8_t palette[16][3] = {
        {12,14,20}, {20,36,92}, {25,150,225}, {45,185,185},
        {220,55,65}, {205,65,190}, {235,110,210}, {222,228,238},
        {94,100,115}, {80,145,245}, {80,205,235}, {100,220,180},
        {255,100,105}, {230,110,220}, {255,180,230}, {255,255,255}
    };
    return palette[attribute & 15][channel];
}

uint8_t glyph_bits(char character, uint32_t row) {
    if (character >= 'A' && character <= 'Z') return uppercase[character - 'A'][row];
    if (character >= 'a' && character <= 'z') return lowercase[character - 'a'][row];
    if (character >= '0' && character <= '9') return uppercase[26 + character - '0'][row];
    switch (character) {
    case '.': return row >= 5 ? 6 : 0;
    case ',': return row == 5 ? 6 : row == 6 ? 4 : 0;
    case ':': return row == 2 || row == 5 ? 4 : 0;
    case ';': return row == 2 || row == 5 ? 4 : row == 6 ? 8 : 0;
    case '-': return row == 3 ? 14 : 0;
    case '_': return row == 6 ? 31 : 0;
    case '=': return row == 2 || row == 4 ? 14 : 0;
    case '+': return row == 3 ? 14 : row == 2 || row == 4 ? 4 : 0;
    case '/': return static_cast<uint8_t>(1u << (row < 5 ? 4 - row : 0));
    case '\\': return static_cast<uint8_t>(1u << (row < 5 ? row : 4));
    case '!': return row < 5 || row == 6 ? 4 : 0;
    case '?': return row == 0 ? 14 : row == 1 ? 17 : row == 2 ? 2 : row == 3 || row == 6 ? 4 : 0;
    case '#': return row == 2 || row == 4 ? 31 : row == 3 ? 10 : 0;
    case '*': return row == 2 || row == 4 ? 21 : row == 3 ? 14 : 0;
    case '(': return row == 0 || row == 6 ? 2 : row == 1 || row == 5 ? 4 : 8;
    case ')': return row == 0 || row == 6 ? 8 : row == 1 || row == 5 ? 4 : 2;
    case '[': return row == 0 || row == 6 ? 14 : 8;
    case ']': return row == 0 || row == 6 ? 14 : 2;
    case '<': return row == 2 || row == 4 ? 4 : row == 3 ? 8 : 0;
    case '>': return row == 2 || row == 4 ? 4 : row == 3 ? 2 : 0;
    case '"': return row < 2 ? 10 : 0;
    case '\'': return row < 2 ? 4 : 0;
    case '|': return 4;
    case ' ': return 0;
    default: return row == 0 || row == 6 ? 31 : 17;
    }
}

void load_font_module(const uint8_t* module, uint32_t size) {
    if (size < 4 || module[0] != 0x36 || module[1] != 0x04 || module[3] == 0) return;
    uint32_t glyph_height = module[3];
    if (glyph_height < font_height || size < 4 + 256 * glyph_height) return;
    const uint8_t* glyph_data = module + 4;
    for (uint32_t character = 0; character < 256; ++character)
        for (uint32_t row = 0; row < font_height; ++row)
            vga_font[character][row] = glyph_data[character * glyph_height + row];
    vga_font_loaded = true;
}

void draw_glyph(uint32_t x, uint32_t y, char character, uint8_t attribute) {
    uint8_t foreground = attribute & 15;
    uint8_t red = color_channel(foreground, 0);
    uint8_t green = color_channel(foreground, 1);
    uint8_t blue = color_channel(foreground, 2);
    for (uint32_t row = 0; row < font_height; ++row) {
        uint8_t bits = vga_font_loaded ? vga_font[static_cast<uint8_t>(character)][row]
            : static_cast<uint8_t>(glyph_bits(character, row * 7 / font_height) << 1);
        for (uint32_t column = 0; column < font_width; ++column) {
            if ((bits & (1u << (font_width - column - 1))) == 0) continue;
            for (uint32_t dy = 0; dy < glyph_scale; ++dy)
                for (uint32_t dx = 0; dx < glyph_scale; ++dx)
                    put_pixel(x + column * glyph_scale + dx, y + row * glyph_scale + dy,
                        (static_cast<uint32_t>(red) << 16) | (static_cast<uint32_t>(green) << 8) | blue);
        }
    }
}

void draw_cell_contents(uint32_t row, uint32_t column) {
    if (!framebuffer.active || row >= rows || column >= columns) return;
    uint32_t x = content_x + column * cell_width;
    uint32_t y = content_y + row * cell_height;
    uint8_t background_index = attributes[row][column] >> 4;
    for (uint32_t py = y; py < y + cell_height; ++py)
        for (uint32_t px = x; px < x + cell_width; ++px) {
            uint32_t color = panel_color(px, py);
            if (background_index) {
                uint32_t tint = (static_cast<uint32_t>(color_channel(background_index, 0)) << 16)
                    | (static_cast<uint32_t>(color_channel(background_index, 1)) << 8)
                    | color_channel(background_index, 2);
                color = mix(color, tint, 190);
            }
            put_pixel(px, py, color);
        }
    uint32_t glyph_w = font_width * glyph_scale;
    uint32_t glyph_h = font_height * glyph_scale;
    draw_glyph(x + (cell_width - glyph_w) / 2, y + (cell_height - glyph_h) / 2,
        characters[row][column], attributes[row][column]);
    if (cursor_visible && cursor_row == row && cursor_column == column) {
        for (uint32_t py = y + 2; py + 2 < y + cell_height; ++py)
            put_pixel(x + 1, py, 0xF4F4FF);
    }
}

bool pointer_overlaps(uint32_t x, uint32_t y, uint32_t width_value, uint32_t height_value) {
    if (!pointer_visible) return false;
    int32_t right = pointer_x + static_cast<int32_t>(pointer_width);
    int32_t bottom = pointer_y + static_cast<int32_t>(pointer_height);
    return static_cast<int32_t>(x) < right && static_cast<int32_t>(x + width_value) > pointer_x
        && static_cast<int32_t>(y) < bottom && static_cast<int32_t>(y + height_value) > pointer_y;
}

void draw_title() {
    const char title[] = "KyronOS Terminal";
    uint32_t x = window_x + 22;
    uint32_t y = window_y + (title_height - font_height * glyph_scale) / 2;
    for (uint32_t index = 0; title[index]; ++index) {
        draw_glyph(x, y, title[index], 0x0F);
        x += (font_width + 1) * glyph_scale;
    }
}

void draw_frame() {
    for (uint32_t y = 0; y < framebuffer.height; ++y) {
        for (uint32_t x = 0; x < framebuffer.width; ++x)
            put_pixel(x, y, panel_color(x, y));
    }
    draw_title();
    for (uint32_t row = 0; row < rows; ++row)
        for (uint32_t column = 0; column < columns; ++column)
            draw_cell_contents(row, column);
}

bool pointer_pixel(uint32_t x, uint32_t y) {
    static const uint16_t shape[pointer_height] = {
        0x8000,0xC000,0xE000,0xF000,0xF800,0xFC00,0xFE00,0xFF00,
        0xFF80,0xFFC0,0xFFE0,0xFF00,0xF800,0xD800,0x8C00,0x0400
    };
    return x < 16 && y < pointer_height && (shape[y] & (1u << (15 - x))) != 0;
}

void blur_background() {
    for (uint32_t y = 0; y < background_height; ++y) {
        uint32_t top = y < 2 ? 0 : y - 2;
        uint32_t bottom = y + 2 < background_height ? y + 2 : background_height - 1;
        for (uint32_t x = 0; x < background_width; ++x) {
            uint32_t left = x < 2 ? 0 : x - 2;
            uint32_t right = x + 2 < background_width ? x + 2 : background_width - 1;
            uint32_t red = 0, green = 0, blue = 0, samples = 0;
            for (uint32_t sample_y = top; sample_y <= bottom; ++sample_y) {
                for (uint32_t sample_x = left; sample_x <= right; ++sample_x) {
                    uint32_t sample = background[sample_y * background_width + sample_x];
                    red += (sample >> 16) & 255;
                    green += (sample >> 8) & 255;
                    blue += sample & 255;
                    ++samples;
                }
            }
            softened_background[y * background_width + x] = ((red / samples) << 16)
                | ((green / samples) << 8) | (blue / samples);
        }
    }
}
}

namespace graphics {
bool initialize(uint32_t multiboot_info) {
    if (!multiboot_info) return false;
    const uint8_t* info = reinterpret_cast<const uint8_t*>(multiboot_info);
    uint32_t total_size = *reinterpret_cast<const uint32_t*>(info);
    if (total_size < 16) return false;
    const uint8_t* tag = info + 8;
    const uint8_t* end = info + total_size;
    while (tag + 8 <= end) {
        uint32_t type = *reinterpret_cast<const uint32_t*>(tag);
        uint32_t size = *reinterpret_cast<const uint32_t*>(tag + 4);
        if (size < 8 || tag + size > end) break;
        if (type == 8 && size >= 36) {
            uint64_t address = *reinterpret_cast<const uint64_t*>(tag + 8);
            uint32_t pitch = *reinterpret_cast<const uint32_t*>(tag + 16);
            uint32_t width_value = *reinterpret_cast<const uint32_t*>(tag + 20);
            uint32_t height_value = *reinterpret_cast<const uint32_t*>(tag + 24);
            if (address <= 0xFFFFFFFFu && width_value >= 640 && height_value >= 480
                && width_value <= 4096 && height_value <= 2160 && pitch >= width_value * 4
                && tag[28] == 32 && tag[29] == 1
                && tag[31] && tag[31] <= 8 && tag[33] && tag[33] <= 8 && tag[35] && tag[35] <= 8
                && tag[30] + tag[31] <= 32 && tag[32] + tag[33] <= 32 && tag[34] + tag[35] <= 32) {
                framebuffer.address = reinterpret_cast<volatile uint32_t*>(static_cast<uint32_t>(address));
                framebuffer.width = width_value;
                framebuffer.height = height_value;
                framebuffer.pitch = pitch;
                framebuffer.red_position = tag[30];
                framebuffer.red_size = tag[31];
                framebuffer.green_position = tag[32];
                framebuffer.green_size = tag[33];
                framebuffer.blue_position = tag[34];
                framebuffer.blue_size = tag[35];
                framebuffer.active = true;
                const uint8_t* module_tag = info + 8;
                while (module_tag + 8 <= end) {
                    uint32_t module_type = *reinterpret_cast<const uint32_t*>(module_tag);
                    uint32_t module_size = *reinterpret_cast<const uint32_t*>(module_tag + 4);
                    if (module_size < 8 || module_tag + module_size > end) break;
                    if (module_type == 3 && module_size >= 16) {
                        uint32_t module_start = *reinterpret_cast<const uint32_t*>(module_tag + 8);
                        uint32_t module_end = *reinterpret_cast<const uint32_t*>(module_tag + 12);
                        if (module_end > module_start)
                            load_font_module(reinterpret_cast<const uint8_t*>(module_start), module_end - module_start);
                    }
                    if (module_type == 0) break;
                    module_tag += (module_size + 7) & ~7u;
                }
                if (width_value * background_height_limit >= height_value * background_width_limit) {
                    background_width = background_width_limit;
                    background_height = height_value * background_width / width_value;
                } else {
                    background_height = background_height_limit;
                    background_width = width_value * background_height / height_value;
                }
                if (background_width == 0) background_width = 1;
                if (background_height == 0) background_height = 1;
                for (uint32_t y = 0; y < background_height; ++y) {
                    uint32_t source_y = y * height_value / background_height;
                    for (uint32_t x = 0; x < background_width; ++x) {
                        uint32_t source_x = x * width_value / background_width;
                        uint32_t packed = framebuffer.address[(source_y * pitch) / 4 + source_x];
                        background[y * background_width + x] = rgb_from_packed(packed);
                    }
                }
                blur_background();
                window_x = width_value / 40;
                window_y = height_value / 40;
                window_width = width_value - 2 * window_x;
                window_height = height_value - 2 * window_y;
                uint32_t inner_width = window_width - 48;
                uint32_t inner_height = window_height - 92;
                cell_width = inner_width / columns;
                cell_height = inner_height / rows;
                if (cell_width < 6 || cell_height < 9) { framebuffer.active = false; return false; }
                glyph_scale = cell_width / (font_width + 1);
                uint32_t vertical_scale = cell_height / (font_height + 2);
                if (glyph_scale > vertical_scale) glyph_scale = vertical_scale;
                if (glyph_scale > 3) glyph_scale = 3;
                if (glyph_scale == 0) glyph_scale = 1;
                title_height = font_height * glyph_scale + 16;
                if (title_height < 42) title_height = 42;
                content_x = window_x + 24 + (inner_width - columns * cell_width) / 2;
                content_y = window_y + title_height + 20;
                pointer_x = static_cast<int32_t>(width_value / 2);
                pointer_y = static_cast<int32_t>(height_value / 2);
                for (uint32_t row = 0; row < rows; ++row)
                    for (uint32_t column = 0; column < columns; ++column) {
                        characters[row][column] = ' ';
                        attributes[row][column] = 0x07;
                    }
                return true;
            }
        }
        if (type == 0) break;
        tag += (size + 7) & ~7u;
    }
    return false;
}

void clear() {
    if (!framebuffer.active) return;
    hide_mouse();
    cursor_visible = false;
    for (uint32_t row = 0; row < rows; ++row)
        for (uint32_t column = 0; column < columns; ++column) {
            characters[row][column] = ' ';
            attributes[row][column] = 0x07;
        }
    draw_frame();
    show_mouse();
}

void draw_cell(uint32_t row, uint32_t column, char character, uint8_t attribute) {
    if (!framebuffer.active || row >= rows || column >= columns) return;
    uint32_t x = content_x + column * cell_width;
    uint32_t y = content_y + row * cell_height;
    bool pointer_overlap = pointer_overlaps(x, y, cell_width, cell_height);
    if (pointer_overlap) hide_mouse();
    characters[row][column] = character;
    attributes[row][column] = attribute;
    draw_cell_contents(row, column);
    if (pointer_overlap) show_mouse();
}

void scroll() {
    if (!framebuffer.active) return;
    hide_mouse();
    for (uint32_t row = 1; row < rows; ++row)
        for (uint32_t column = 0; column < columns; ++column) {
            characters[row - 1][column] = characters[row][column];
            attributes[row - 1][column] = attributes[row][column];
        }
    for (uint32_t column = 0; column < columns; ++column) {
        characters[rows - 1][column] = ' ';
        attributes[rows - 1][column] = 0x07;
    }
    for (uint32_t row = 0; row < rows; ++row)
        for (uint32_t column = 0; column < columns; ++column)
            draw_cell_contents(row, column);
    show_mouse();
}

void set_cursor(uint32_t row, uint32_t column, bool visible) {
    if (!framebuffer.active) return;
    uint32_t old_row = cursor_row;
    uint32_t old_column = cursor_column;
    bool pointer_overlap = pointer_overlaps(content_x + old_column * cell_width,
        content_y + old_row * cell_height, cell_width, cell_height);
    uint32_t new_row = row < rows ? row : rows - 1;
    uint32_t new_column = column < columns ? column : columns - 1;
    if (pointer_overlaps(content_x + new_column * cell_width, content_y + new_row * cell_height,
        cell_width, cell_height)) pointer_overlap = true;
    if (pointer_overlap) hide_mouse();
    draw_cell_contents(old_row, old_column);
    cursor_row = new_row;
    cursor_column = new_column;
    cursor_visible = visible;
    draw_cell_contents(cursor_row, cursor_column);
    if (pointer_overlap) show_mouse();
}

void hide_mouse() {
    if (!framebuffer.active || !pointer_visible) return;
    for (uint32_t y = 0; y < pointer_height; ++y)
        for (uint32_t x = 0; x < pointer_width; ++x) {
            if (!pointer_pixel(x, y)) continue;
            int32_t screen_x = pointer_x + static_cast<int32_t>(x);
            int32_t screen_y = pointer_y + static_cast<int32_t>(y);
            if (screen_x >= 0 && screen_y >= 0 && static_cast<uint32_t>(screen_x) < framebuffer.width
                && static_cast<uint32_t>(screen_y) < framebuffer.height)
                framebuffer.address[(static_cast<uint32_t>(screen_y) * framebuffer.pitch) / 4 + static_cast<uint32_t>(screen_x)]
                    = pointer_saved[y * pointer_width + x];
        }
    pointer_visible = false;
}

void show_mouse() {
    if (!framebuffer.active || pointer_visible) return;
    for (uint32_t y = 0; y < pointer_height; ++y)
        for (uint32_t x = 0; x < pointer_width; ++x) {
            if (!pointer_pixel(x, y)) continue;
            int32_t screen_x = pointer_x + static_cast<int32_t>(x);
            int32_t screen_y = pointer_y + static_cast<int32_t>(y);
            if (screen_x < 0 || screen_y < 0 || static_cast<uint32_t>(screen_x) >= framebuffer.width
                || static_cast<uint32_t>(screen_y) >= framebuffer.height) continue;
            uint32_t index = (static_cast<uint32_t>(screen_y) * framebuffer.pitch) / 4 + static_cast<uint32_t>(screen_x);
            pointer_saved[y * pointer_width + x] = framebuffer.address[index];
            bool edge = x == 0 || y == 0 || !pointer_pixel(x - (x > 0), y) || !pointer_pixel(x + 1, y)
                || !pointer_pixel(x, y - (y > 0)) || !pointer_pixel(x, y + 1);
            put_pixel(static_cast<uint32_t>(screen_x), static_cast<uint32_t>(screen_y), edge ? 0x080A12 : 0xF7F8FF);
        }
    pointer_visible = true;
}

void set_mouse(uint32_t x, uint32_t y) {
    if (!framebuffer.active) return;
    hide_mouse();
    pointer_x = static_cast<int32_t>(x < framebuffer.width ? x : framebuffer.width - 1);
    pointer_y = static_cast<int32_t>(y < framebuffer.height ? y : framebuffer.height - 1);
    show_mouse();
}

uint32_t width() { return framebuffer.width; }
uint32_t height() { return framebuffer.height; }
void mouse_cell(uint32_t x, uint32_t y, uint32_t& column, uint32_t& row) {
    if (!framebuffer.active) { column = x / 9; row = y / 16; return; }
    column = x <= content_x ? 0 : (x - content_x) / cell_width;
    row = y <= content_y ? 0 : (y - content_y) / cell_height;
    if (column >= columns) column = columns - 1;
    if (row >= rows) row = rows - 1;
}
}