#include "drivers/pic.h"

#include <stdint.h>

enum {
    pic1_command = 0x20u,
    pic1_data = 0x21u,
    pic2_command = 0xA0u,
    pic2_data = 0xA1u,
    icw1_init = 0x10u,
    icw1_icw4 = 0x01u,
    icw4_8086 = 0x01u
};

static void io_wait(void) {
    __asm__ __volatile__("outb %%al, $0x80" : : "a"(0u));
}

static void io_write8(uint16_t port, uint8_t value) {
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint8_t io_read8(uint16_t port) {
    uint8_t value;
    __asm__ __volatile__("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void pic_initialize(void) {
    uint8_t master_mask;
    uint8_t slave_mask;

    master_mask = io_read8(pic1_data);
    slave_mask = io_read8(pic2_data);

    io_write8(pic1_command, (uint8_t)(icw1_init | icw1_icw4));
    io_wait();
    io_write8(pic2_command, (uint8_t)(icw1_init | icw1_icw4));
    io_wait();

    io_write8(pic1_data, 0x20u);
    io_wait();
    io_write8(pic2_data, 0x28u);
    io_wait();

    io_write8(pic1_data, 0x04u);
    io_wait();
    io_write8(pic2_data, 0x02u);
    io_wait();

    io_write8(pic1_data, icw4_8086);
    io_wait();
    io_write8(pic2_data, icw4_8086);
    io_wait();

    master_mask = (uint8_t)((master_mask | 0xFFu) & (uint8_t)~0x01u & (uint8_t)~0x04u);
    slave_mask = 0xFFu;

    io_write8(pic1_data, master_mask);
    io_write8(pic2_data, slave_mask);
}

void pic_eoi(uint8_t irq) {
    if (irq >= 8u) {
        io_write8(pic2_command, 0x20u);
    }

    io_write8(pic1_command, 0x20u);
}
