#include "user/syscall.h"

#include <stdint.h>

void user_init_main(void)
{
#ifdef USER_TEST_FD_WRITE
    char out1;
    char out2;
    int64_t dup_fd;

    out1 = 'F';
    out2 = 'D';

    if (user_sys_write(1, &out1, 1u) != 1) {
        user_sys_putc('!');
        user_sys_exit();
    }

    dup_fd = user_sys_dup(1);
    if (dup_fd < 0) {
        user_sys_putc('!');
        user_sys_exit();
    }

    if (user_sys_write((int)dup_fd, &out2, 1u) != 1 || user_sys_close((int)dup_fd) != 0) {
        user_sys_putc('!');
        user_sys_exit();
    }

    user_sys_exit();
#elif defined(USER_TEST_BAD_SYSCALL)
    uint64_t result;

    result = user_syscall1((uint64_t)0xFFFFu, 0u);
    if (result == (uint64_t)-1) {
        user_sys_putc('V');
    } else {
        user_sys_putc('X');
    }
    user_sys_exit();
#elif defined(USER_TEST_YIELD_STRESS)
    uint32_t budget;

    user_sys_putc('Y');
    budget = 128u;

    for (;;) {
        user_sys_yield();
        if (--budget == 0u) {
            budget = 128u;
            user_sys_putc('y');
        }
    }
#else
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
#endif
}
