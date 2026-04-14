#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

void paging_initialize(void);
void paging_mark_user_accessible(uintptr_t address);

#endif
