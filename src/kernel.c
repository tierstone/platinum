#include "drivers/serial.h"
#include <stdint.h>

void kernel_main(void) {
    serial_init();
    serial_write("Hello from /p/OS\n", 17);
    
    while (1) {
        __asm__ __volatile__ ("hlt" ::: "memory");
    }
}
