#ifndef CORE_H
#define CORE_H

#include <stdint.h>

void kernel_main(void *image_handle, void *system_table);
void arch_load_gdt(const void *descriptor, uint16_t data_selector);
void arch_halt_forever(void);

#endif
