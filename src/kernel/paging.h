#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

void paging_initialize(void);
uintptr_t paging_reserved_begin(void);
uintptr_t paging_reserved_end(void);

#endif
