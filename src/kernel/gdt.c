#include "kernel/gdt.h"
#include "kernel/core.h"

#include <stdint.h>
#include <stddef.h>

enum {
    gdt_null_index = 0,
    gdt_kernel_code_index = 1,
    gdt_kernel_data_index = 2,
    gdt_entry_count = 3
};

static uint64_t gdt[gdt_entry_count];
static uint8_t gdtr[10];

static void write_le16(uint8_t *buffer, uint16_t value) {
    buffer[0] = (uint8_t)(value & 0xFFu);
    buffer[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void write_le64(uint8_t *buffer, uint64_t value) {
    buffer[0] = (uint8_t)(value & 0xFFu);
    buffer[1] = (uint8_t)((value >> 8) & 0xFFu);
    buffer[2] = (uint8_t)((value >> 16) & 0xFFu);
    buffer[3] = (uint8_t)((value >> 24) & 0xFFu);
    buffer[4] = (uint8_t)((value >> 32) & 0xFFu);
    buffer[5] = (uint8_t)((value >> 40) & 0xFFu);
    buffer[6] = (uint8_t)((value >> 48) & 0xFFu);
    buffer[7] = (uint8_t)((value >> 56) & 0xFFu);
}

void gdt_initialize(void) {
    gdt[gdt_null_index] = 0x0000000000000000ull;
    gdt[gdt_kernel_code_index] = 0x00AF9A000000FFFFull;
    gdt[gdt_kernel_data_index] = 0x00AF92000000FFFFull;

    write_le16(&gdtr[0], (uint16_t)(sizeof(gdt) - 1u));
    write_le64(&gdtr[2], (uint64_t)(uintptr_t)&gdt[0]);

    arch_load_gdt(&gdtr[0], 0x10u);
}

uintptr_t gdt_reserved_begin(void) {
    uintptr_t a = (uintptr_t)&gdt[0];
    uintptr_t b = (uintptr_t)&gdtr[0];

    return a < b ? a : b;
}

uintptr_t gdt_reserved_end(void) {
    uintptr_t a = (uintptr_t)(&gdt[gdt_entry_count]);
    uintptr_t b = (uintptr_t)(&gdtr[10]);

    return a > b ? a : b;
}
