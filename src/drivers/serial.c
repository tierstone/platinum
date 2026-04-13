#include "serial.h"

#include <stddef.h>
#include <stdint.h>

enum {
    serial_com1 = 0x3F8u
};

static void io_write8(uint16_t port, uint8_t value) {
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint8_t io_read8(uint16_t port) {
    uint8_t value;
    __asm__ __volatile__("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void serial_init(void) {
    io_write8((uint16_t)(serial_com1 + 1u), 0x00u);
    io_write8((uint16_t)(serial_com1 + 3u), 0x80u);
    io_write8((uint16_t)(serial_com1 + 0u), 0x03u);
    io_write8((uint16_t)(serial_com1 + 1u), 0x00u);
    io_write8((uint16_t)(serial_com1 + 3u), 0x03u);
    io_write8((uint16_t)(serial_com1 + 2u), 0xC7u);
    io_write8((uint16_t)(serial_com1 + 4u), 0x0Bu);
}

void serial_write(const char *buffer, size_t length) {
    size_t index;

    if (buffer == NULL) {
        return;
    }

    for (index = 0u; index < length; ++index) {
        while ((io_read8((uint16_t)(serial_com1 + 5u)) & 0x20u) == 0u) {
        }
        io_write8(serial_com1, (uint8_t)buffer[index]);
    }
}
