#ifndef GDT_H
#define GDT_H

#include <stdint.h>

void gdt_initialize(void);
uintptr_t gdt_reserved_begin(void);
uintptr_t gdt_reserved_end(void);

#endif
