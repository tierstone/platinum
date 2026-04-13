#ifndef PALLOC_H
#define PALLOC_H

#include <stdint.h>

void palloc_initialize(void);
uintptr_t palloc_alloc(void);
void palloc_free(uintptr_t page);
void palloc_self_test(void);
uintptr_t palloc_reserved_begin(void);
uintptr_t palloc_reserved_end(void);

#endif
