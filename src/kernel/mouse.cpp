#include "kernel/mouse.hpp"
#include "kernel/console.hpp"
#include "drivers/usb_keyboard.hpp"

namespace {
uint8_t packet[4]{};
uint8_t packet_index = 0;
uint8_t packet_size = 3;
uint8_t device_id = 0;

uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

bool wait_input_empty() {
    for (uint32_t timeout = 0; timeout < 100000; ++timeout)
        if ((inb(0x64) & 2) == 0) return true;
    return false;
}

bool send_mouse_command(uint8_t command) {
    if (!wait_input_empty()) return false;
    outb(0x64, 0xD4);
    if (!wait_input_empty()) return false;
    outb(0x60, command);
    for (uint32_t timeout = 0; timeout < 100000; ++timeout) {
        uint8_t status = inb(0x64);
        if ((status & 1) == 0) continue;
        uint8_t response = inb(0x60);
        if (status & 0x20) return response == 0xFA;
    }
    return false;
}

bool read_mouse_byte(uint8_t& value) {
    for (uint32_t timeout = 0; timeout < 100000; ++timeout) {
        uint8_t status = inb(0x64);
        if ((status & 1) == 0) continue;
        uint8_t data = inb(0x60);
        if (status & 0x20) { value = data; return true; }
    }
    return false;
}

class ConsoleMouseSink final : public usb::mouse::Sink {
public:
    void movement(int32_t delta_x, int32_t delta_y, uint8_t buttons, int8_t) override {
        console::mouse_event(delta_x, delta_y, buttons);
    }
};

}

namespace mouse {
bool initialize() {
    if (!wait_input_empty()) return false;
    outb(0x64, 0xA8);
    packet_index = 0;
    if (!send_mouse_command(0xF6)) return false;
    if (send_mouse_command(0xF3) && send_mouse_command(200) &&
        send_mouse_command(0xF3) && send_mouse_command(100) &&
        send_mouse_command(0xF3) && send_mouse_command(80) &&
        send_mouse_command(0xF2) && read_mouse_byte(device_id)) {
        if (device_id == 3 || device_id == 4) packet_size = 4;
    }
    if (!send_mouse_command(0xF4)) return false;
    return true;
}

void consume_data(uint8_t value) {
    if (packet_index == 0 && (value & 0x08) == 0) return;
    packet[packet_index++] = value;
    if (packet_index != 3 && (packet_size != 4 || packet_index != 4)) return;
    packet_index = 0;
    if (packet[0] & 0xC0) return;
    int32_t delta_x = packet[1];
    int32_t delta_y = packet[2];
    if (packet[0] & 0x10) delta_x -= 256;
    if (packet[0] & 0x20) delta_y -= 256;
    uint8_t buttons = packet[0] & 0x07;
    if (packet_size == 4 && device_id == 4) buttons |= static_cast<uint8_t>((packet[3] & 0x30) >> 1);
    console::mouse_event(delta_x, delta_y, buttons);
}

bool consume_usb_report(const uint8_t* report, uint32_t length) {
    ConsoleMouseSink sink;
    return usb::mouse::decode_boot_report(report, length, sink);
}
}