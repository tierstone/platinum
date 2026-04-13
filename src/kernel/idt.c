#include "kernel/idt.h"
#include "kernel/core.h"

#include <stdint.h>
#include <stddef.h>

enum {
    idt_entry_count = 256
};

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attributes;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
};

static struct idt_entry idt[idt_entry_count];
static uint8_t idtr[10];

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

static void idt_set_gate(uint8_t vector, void (*handler)(void)) {
    uintptr_t address = (uintptr_t)handler;

    idt[vector].offset_low = (uint16_t)(address & 0xFFFFu);
    idt[vector].selector = 0x08u;
    idt[vector].ist = 0u;
    idt[vector].type_attributes = 0x8Eu;
    idt[vector].offset_mid = (uint16_t)((address >> 16) & 0xFFFFu);
    idt[vector].offset_high = (uint32_t)((address >> 32) & 0xFFFFFFFFu);
    idt[vector].reserved = 0u;
}

void idt_initialize(void) {
    size_t index;

    for (index = 0u; index < idt_entry_count; ++index) {
        idt[index].offset_low = 0u;
        idt[index].selector = 0u;
        idt[index].ist = 0u;
        idt[index].type_attributes = 0u;
        idt[index].offset_mid = 0u;
        idt[index].offset_high = 0u;
        idt[index].reserved = 0u;
    }

    idt_set_gate(6u, arch_isr_invalid_opcode);

    write_le16(&idtr[0], (uint16_t)(sizeof(idt) - 1u));
    write_le64(&idtr[2], (uint64_t)(uintptr_t)&idt[0]);

    arch_load_idt(&idtr[0]);
}

uintptr_t idt_reserved_begin(void) {
    uintptr_t a = (uintptr_t)&idt[0];
    uintptr_t b = (uintptr_t)&idtr[0];

    return a < b ? a : b;
}

uintptr_t idt_reserved_end(void) {
    uintptr_t a = (uintptr_t)(&idt[idt_entry_count]);
    uintptr_t b = (uintptr_t)(&idtr[10]);

    return a > b ? a : b;
}
