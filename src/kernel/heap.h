#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>

void heap_initialize(void);
void *kmalloc(size_t size);
void *kzalloc(size_t size);
void kfree(void *ptr);
void heap_self_test(void);

#endif
