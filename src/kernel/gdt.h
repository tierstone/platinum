#ifndef GDT_H
#define GDT_H

#include <stdint.h>

void gdt_initialize(void);
void gdt_set_tss_rsp0(uint64_t rsp0);
uintptr_t gdt_reserved_begin(void);
uintptr_t gdt_reserved_end(void);

#endif
