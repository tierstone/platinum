#ifndef IDT_H
#define IDT_H

#include <stdint.h>

void idt_initialize(void);
void idt_set_irq0_gate(void);
uintptr_t idt_reserved_begin(void);
uintptr_t idt_reserved_end(void);

#endif
