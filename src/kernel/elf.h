#ifndef ELF_H
#define ELF_H

#include "kernel/sched.h"

#include <stddef.h>
#include <stdint.h>

enum {
    elf_user_load_base = 0x02100000u,
    elf_user_load_size = 0x00100000u
};

int elf_load_user_image(const uint8_t *image, size_t image_size, struct user_task_bootstrap *bootstrap);

#endif
