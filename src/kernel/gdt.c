#include "kernel/gdt.h"
#include "kernel/core.h"

#include <stdint.h>
#include <stddef.h>

enum {
    gdt_null_index = 0,
    gdt_kernel_code_index = 1,
    gdt_kernel_data_index = 2,
    gdt_user_data_index = 3,
    gdt_user_code_index = 4,
    gdt_tss_index = 5,
    gdt_entry_count = 7
};

struct tss64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t io_map_base;
};

static uint64_t gdt[gdt_entry_count];
static uint8_t gdtr[10];
static struct tss64 tss;

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

static uint64_t gdt_make_code_data_descriptor(uint32_t access, uint32_t flags) {
    uint64_t value = 0u;

    value |= 0x0000FFFFull;
    value |= ((uint64_t)(access & 0xFFu)) << 40;
    value |= ((uint64_t)(flags & 0x0Fu)) << 52;
    value |= 0x000F000000000000ull;

    return value;
}

static void gdt_set_tss_descriptor(uintptr_t base, uint32_t limit) {
    uint64_t low = 0u;
    uint64_t high = 0u;

    low |= (uint64_t)(limit & 0xFFFFu);
    low |= ((uint64_t)(base & 0xFFFFFFu)) << 16;
    low |= ((uint64_t)0x89u) << 40;
    low |= ((uint64_t)((limit >> 16) & 0x0Fu)) << 48;
    low |= ((uint64_t)((base >> 24) & 0xFFu)) << 56;

    high |= (uint64_t)((base >> 32) & 0xFFFFFFFFu);

    gdt[gdt_tss_index] = low;
    gdt[gdt_tss_index + 1] = high;
}

void gdt_initialize(void) {
    size_t index;
    uintptr_t tss_base;
    uint32_t tss_limit;

    for (index = 0u; index < gdt_entry_count; ++index) {
        gdt[index] = 0u;
    }

    tss.reserved0 = 0u;
    tss.rsp0 = 0u;
    tss.rsp1 = 0u;
    tss.rsp2 = 0u;
    tss.reserved1 = 0u;
    tss.ist1 = 0u;
    tss.ist2 = 0u;
    tss.ist3 = 0u;
    tss.ist4 = 0u;
    tss.ist5 = 0u;
    tss.ist6 = 0u;
    tss.ist7 = 0u;
    tss.reserved2 = 0u;
    tss.reserved3 = 0u;
    tss.io_map_base = (uint16_t)sizeof(tss);

    gdt[gdt_null_index] = 0x0000000000000000ull;
    gdt[gdt_kernel_code_index] = gdt_make_code_data_descriptor(0x9Au, 0x0Au);
    gdt[gdt_kernel_data_index] = gdt_make_code_data_descriptor(0x92u, 0x0Au);
    gdt[gdt_user_data_index] = gdt_make_code_data_descriptor(0xF2u, 0x0Au);
    gdt[gdt_user_code_index] = gdt_make_code_data_descriptor(0xFAu, 0x0Au);

    tss_base = (uintptr_t)&tss;
    tss_limit = (uint32_t)(sizeof(tss) - 1u);
    gdt_set_tss_descriptor(tss_base, tss_limit);

    write_le16(&gdtr[0], (uint16_t)(sizeof(gdt) - 1u));
    write_le64(&gdtr[2], (uint64_t)(uintptr_t)&gdt[0]);

    arch_load_gdt(&gdtr[0], 0x10u);
    arch_load_tr((uint16_t)(gdt_tss_index * 8u));
}

void gdt_set_tss_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}

uintptr_t gdt_reserved_begin(void) {
    uintptr_t a = (uintptr_t)&gdt[0];
    uintptr_t b = (uintptr_t)&gdtr[0];
    uintptr_t c = (uintptr_t)&tss;

    uintptr_t min = a;

    if (b < min) {
        min = b;
    }

    if (c < min) {
        min = c;
    }

    return min;
}

uintptr_t gdt_reserved_end(void) {
    uintptr_t a = (uintptr_t)(&gdt[gdt_entry_count]);
    uintptr_t b = (uintptr_t)(&gdtr[10]);
    uintptr_t c = (uintptr_t)(&tss + 1);

    uintptr_t max = a;

    if (b > max) {
        max = b;
    }

    if (c > max) {
        max = c;
    }

    return max;
}
