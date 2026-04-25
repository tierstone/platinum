#include "user/syscall.h"

#include <stdint.h>

void user_init_main(void)
{
#ifdef USER_TEST_EXEC_SECOND
    char ok[10];

    ok[0] = 'p';
    ok[1] = 'u';
    ok[2] = 'l';
    ok[3] = 's';
    ok[4] = 'e';
    ok[5] = '2';
    ok[6] = ' ';
    ok[7] = 'o';
    ok[8] = 'k';
    ok[9] = '\n';
    if (user_sys_write(1, ok, 10u) != 10) {
        user_sys_putc('!');
        user_sys_exit();
    }

    user_sys_exit();
#else
    uint64_t ticks;
    uint64_t previous;
    uint32_t budget;

    user_sys_putc('2');
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
            user_sys_putc('2');
        }

        user_sys_yield();
    }
#endif
}
