#ifndef CORE_H
#define CORE_H

#include <stdint.h>

void kernel_main(void *image_handle, void *system_table);
void kernel_trap(uint32_t vector);
void kernel_trap_error(uint32_t vector, uint64_t error_code);
uintptr_t kernel_timer_tick(uintptr_t current_rsp);
uintptr_t kernel_syscall_entry(uintptr_t current_rsp);

void arch_load_gdt(const void *descriptor, uint16_t data_selector);
void arch_load_idt(const void *descriptor);
void arch_load_cr3(uint64_t value);
uint64_t arch_read_cr2(void);
void arch_enable_interrupts(void);
void arch_load_tr(uint16_t selector);
void arch_task_enter(uint64_t rsp);
void arch_enter_user(uint64_t rip, uint64_t rsp);
void arch_halt_forever(void);

void arch_isr_invalid_opcode(void);
void arch_isr_general_protection(void);
void arch_isr_page_fault(void);
void arch_irq0_entry(void);
void arch_syscall_entry(void);
void arch_user_init_entry(void);
void arch_user_program_entry(void);

#endif
