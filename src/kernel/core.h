#ifndef CORE_H
#define CORE_H

#include <stdint.h>

void kernel_main(void *image_handle, void *system_table);
void kernel_trap(uint32_t vector);
uintptr_t kernel_timer_tick(uintptr_t current_rsp);

void arch_load_gdt(const void *descriptor, uint16_t data_selector);
void arch_load_idt(const void *descriptor);
void arch_load_cr3(uint64_t value);
void arch_enable_interrupts(void);
void arch_task_enter(uint64_t rsp);
void arch_halt_forever(void);

void arch_isr_invalid_opcode(void);
void arch_irq0_entry(void);

#endif
