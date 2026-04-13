#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>

void kernel_main(void *image_handle, void *system_table);
void arch_halt_forever(void);

#endif
