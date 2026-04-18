#include "user/syscall.h"

#include <stdint.h>

void user_init_main(void)
{
#ifdef USER_TEST_EXEC_REGISTRY
    char ok[11];

    ok[0] = 'e';
    ok[1] = 'x';
    ok[2] = 'e';
    ok[3] = 'c';
    ok[4] = 'r';
    ok[5] = 'e';
    ok[6] = 'g';
    ok[7] = ' ';
    ok[8] = 'o';
    ok[9] = 'k';
    ok[10] = '\n';
    if (user_sys_write(1, ok, 11u) != 11) {
        user_sys_putc('!');
        user_sys_exit();
    }

    user_sys_exit();
#else
    uint64_t ticks;
    uint64_t previous;
    uint32_t budget;

    user_sys_putc('E');
    previous = user_sys_get_ticks();
    budget = 8u;

    for (;;) {
        ticks = user_sys_get_ticks();
        if (ticks == previous) {
            continue;
        }

        previous = ticks;
        if (--budget == 0u) {
            budget = 8u;
            user_sys_putc('e');
        }

        user_sys_yield();
    }
#endif
}
