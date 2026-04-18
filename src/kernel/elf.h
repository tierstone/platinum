#ifndef ELF_H
#define ELF_H

#include "kernel/sched.h"

#include <stddef.h>
#include <stdint.h>

enum {
    elf_user_load_base = 0x02100000u,
    elf_user_load_size = 0x00100000u
};

struct loaded_user_image {
    uintptr_t load_begin;
    uintptr_t load_end;
    uintptr_t load_physical_begin;
    uintptr_t load_physical_end;
    uintptr_t reusable_physical_begin;
    uintptr_t reusable_physical_end;
    void (*entry)(void);
    uintptr_t stack_page;
    uintptr_t stack_top;
};

int elf_load_user_image(
    const uint8_t *image,
    size_t image_size,
    const struct user_virtual_layout *layout,
    uintptr_t address_space,
    uintptr_t reusable_physical_begin,
    uintptr_t reusable_physical_end,
    struct loaded_user_image *loaded_image
);
void elf_release_user_image(struct loaded_user_image *loaded_image);

#endif
