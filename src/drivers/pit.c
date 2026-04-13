#include "drivers/pit.h"

#include <stdint.h>

enum {
    pit_command_port = 0x43u,
    pit_channel0_port = 0x40u,
    pit_input_hz = 1193182u
};

static void io_write8(uint16_t port, uint8_t value) {
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

void pit_initialize(uint32_t frequency_hz) {
    uint32_t divisor32;
    uint16_t divisor;

    if (frequency_hz == 0u) {
        frequency_hz = 100u;
    }

    divisor32 = pit_input_hz / frequency_hz;
    if (divisor32 == 0u) {
        divisor32 = 1u;
    }

    if (divisor32 > 65535u) {
        divisor32 = 65535u;
    }

    divisor = (uint16_t)divisor32;

    io_write8(pit_command_port, 0x36u);
    io_write8(pit_channel0_port, (uint8_t)(divisor & 0xFFu));
    io_write8(pit_channel0_port, (uint8_t)((divisor >> 8) & 0xFFu));
}
