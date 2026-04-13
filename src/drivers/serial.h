#ifndef SERIAL_H
#define SERIAL_H

#include <stddef.h>

void serial_init(void);
void serial_write(const char *buffer, size_t length);

#endif
