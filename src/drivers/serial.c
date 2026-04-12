#include "serial.h"
#include <stdint.h>
#include <stddef.h>
#define COM1 0x3F8
static void serial_out(uint16_t port, uint8_t val) {
    __asm__ __volatile__ ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static uint8_t serial_in(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__ ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
void serial_init(void) {
    serial_out(COM1 + 1, 0x00);
    serial_out(COM1 + 3, 0x80);
    serial_out(COM1 + 0, 0x03);
    serial_out(COM1 + 1, 0x00);
    serial_out(COM1 + 3, 0x03);
    serial_out(COM1 + 2, 0xC7);
    serial_out(COM1 + 4, 0x0B);
}
void serial_write(const char *buf, size_t count) {
    for (size_t i = 0; i < count; i++) {
        while ((serial_in(COM1 + 5) & 0x20) == 0);
        serial_out(COM1, buf[i]);
    }
}
