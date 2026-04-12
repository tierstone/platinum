#ifndef SERIAL_H
#define SERIAL_H
#include <stdint.h>
#include <stddef.h>
void serial_init(void);
void serial_write(const char *buf, size_t count);
#endif
