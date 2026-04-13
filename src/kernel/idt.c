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
    uintptr_t addr = (uintptr_t)handler;

    idt[vector].offset_low = (uint16_t)(addr & 0xFFFFu);
    idt[vector].selector = 0x08u;
    idt[vector].ist = 0u;
    idt[vector].type_attributes = 0x8Eu;
    idt[vector].offset_mid = (uint16_t)((addr >> 16) & 0xFFFFu);
    idt[vector].offset_high = (uint32_t)((addr >> 32) & 0xFFFFFFFFu);
    idt[vector].reserved = 0u;
}

void idt_initialize(void) {
    for (size_t i = 0; i < idt_entry_count; i++) {
        idt[i].offset_low = 0;
        idt[i].selector = 0;
        idt[i].ist = 0;
        idt[i].type_attributes = 0;
        idt[i].offset_mid = 0;
        idt[i].offset_high = 0;
        idt[i].reserved = 0;
    }

    idt_set_gate(6u, arch_isr_invalid_opcode);

    write_le16(&idtr[0], (uint16_t)(sizeof(idt) - 1u));
    write_le64(&idtr[2], (uint64_t)(uintptr_t)&idt[0]);

    arch_load_idt(&idtr[0]);
}
