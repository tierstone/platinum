#include "user/syscall.h"

#include <stdint.h>

void user_init_main(void)
{
#ifdef USER_TEST_EXEC_LOOP
    char path[11];
    char ok[12];

    if (user_sys_get_ticks() < 4u) {
        path[0] = '/';
        path[1] = 'b';
        path[2] = 'i';
        path[3] = 'n';
        path[4] = '/';
        path[5] = 'p';
        path[6] = 'u';
        path[7] = 'l';
        path[8] = 's';
        path[9] = 'e';
        path[10] = '\0';

        if (user_sys_exec(path) == -1) {
            user_sys_putc('!');
            user_sys_exit();
        }

        user_sys_putc('!');
        user_sys_exit();
    }

    ok[0] = 'e';
    ok[1] = 'x';
    ok[2] = 'e';
    ok[3] = 'c';
    ok[4] = 'l';
    ok[5] = 'o';
    ok[6] = 'o';
    ok[7] = 'p';
    ok[8] = ' ';
    ok[9] = 'o';
    ok[10] = 'k';
    ok[11] = '\n';
    if (user_sys_write(1, ok, 12u) != 12) {
        user_sys_putc('!');
        user_sys_exit();
    }

    user_sys_exit();
#else
    uint64_t ticks;
    uint64_t previous;
    uint32_t budget;

    user_sys_putc('P');
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
            user_sys_putc('p');
        }

        user_sys_yield();
    }
#endif
}
