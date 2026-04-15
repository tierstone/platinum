#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

enum {
    paging_user_trampoline_base = 0x0000000040000000ull,
    paging_user_image_base = 0x0000000040001000ull,
    paging_user_stack_top = 0x0000000040200000ull
};

void paging_initialize(void);
uintptr_t paging_kernel_address_space(void);
uintptr_t paging_create_user_address_space(void);
void paging_map_user_page(uintptr_t address_space, uintptr_t virtual_address, uintptr_t physical_address);
void paging_activate(uintptr_t address_space);
int paging_address_space_valid(uintptr_t address_space);
int paging_mapping_matches(uintptr_t address_space, uintptr_t virtual_address, uintptr_t physical_address, int user_accessible);
int paging_mapping_is_supervisor_only(uintptr_t address_space, uintptr_t virtual_address);

#endif
