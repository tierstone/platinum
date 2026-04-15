#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

enum {
    paging_user_trampoline_base = 0x0000000040000000ull,
    paging_user_image_base = 0x0000000040001000ull,
    paging_user_stack_top = 0x0000000040200000ull
};

void paging_initialize(void);
void paging_map_user_page(uintptr_t virtual_address, uintptr_t physical_address);

#endif
