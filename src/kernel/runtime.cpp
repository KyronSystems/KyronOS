#include <stddef.h>

extern "C" void* memcpy(void* destination, const void* source, size_t count) {
    auto* output = static_cast<unsigned char*>(destination);
    const auto* input = static_cast<const unsigned char*>(source);
    for (size_t index = 0; index < count; ++index) output[index] = input[index];
    return destination;
}

extern "C" void* memset(void* destination, int value, size_t count) {
    auto* output = static_cast<unsigned char*>(destination);
    for (size_t index = 0; index < count; ++index) output[index] = static_cast<unsigned char>(value);
    return destination;
}

extern "C" size_t strlen(const char* text) {
    size_t length = 0;
    while (text[length]) ++length;
    return length;
}

extern "C" int strcmp(const char* left, const char* right) {
    while (*left && *left == *right) { ++left; ++right; }
    return static_cast<unsigned char>(*left) - static_cast<unsigned char>(*right);
}

extern "C" char* strncpy(char* destination, const char* source, size_t count) {
    size_t index = 0;
    for (; index < count && source[index]; ++index) destination[index] = source[index];
    for (; index < count; ++index) destination[index] = 0;
    return destination;
}

void operator delete(void* pointer) noexcept {
    (void)pointer;
}

void operator delete[](void* pointer) noexcept {
    (void)pointer;
}

void operator delete(void* pointer, size_t size) noexcept {
    (void)pointer;
    (void)size;
}

void operator delete[](void* pointer, size_t size) noexcept {
    (void)pointer;
    (void)size;
}
