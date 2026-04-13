#ifndef PIC_H
#define PIC_H

#include <stdint.h>

void pic_initialize(void);
void pic_eoi(uint8_t irq);

#endif
