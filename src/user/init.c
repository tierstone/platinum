#include "user/syscall.h"

#include <stdint.h>

void user_init_main(void)
{
    uint64_t ticks;
    uint64_t previous;
    uint32_t budget;

    user_sys_putc('U');
    previous = user_sys_get_ticks();
    budget = 16u;

    for (;;) {
        ticks = user_sys_get_ticks();
        if (ticks == previous) {
            continue;
        }

        previous = ticks;
        if (--budget == 0u) {
            budget = 16u;
            user_sys_putc('u');
        }

        user_sys_yield();
    }
}
