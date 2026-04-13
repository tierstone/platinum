#include "serial.h"

#include <stdint.h>
#include <stddef.h>

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
    io_write8(serial_com1 + 1, 0x00);
    io_write8(serial_com1 + 3, 0x80);
    io_write8(serial_com1 + 0, 0x03);
    io_write8(serial_com1 + 1, 0x00);
    io_write8(serial_com1 + 3, 0x03);
    io_write8(serial_com1 + 2, 0xC7);
    io_write8(serial_com1 + 4, 0x0B);
}

void serial_write(const char *buffer, size_t length) {
    for (size_t i = 0; i < length; i++) {
        while ((io_read8(serial_com1 + 5) & 0x20) == 0);
        io_write8(serial_com1, (uint8_t)buffer[i]);
    }
}
