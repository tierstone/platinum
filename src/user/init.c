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
#elif defined(USER_TEST_FD_READ)
    char ch;

    ch = 0;
    if (user_sys_read(0, &ch, 1u) != 1 || ch != '@' || user_sys_write(1, &ch, 1u) != 1) {
        user_sys_putc('!');
        user_sys_exit();
    }

    user_sys_exit();
#elif defined(USER_TEST_OPEN)
    char ch;
    int64_t fd;
    char path[12];
    char ok;

    path[0] = '/';
    path[1] = 'e';
    path[2] = 't';
    path[3] = 'c';
    path[4] = '/';
    path[5] = 'b';
    path[6] = 'a';
    path[7] = 'n';
    path[8] = 'n';
    path[9] = 'e';
    path[10] = 'r';
    path[11] = '\0';

    fd = user_sys_open(path);
    ok = 'O';
    ch = 0;
    if (fd < 0 || user_sys_read((int)fd, &ch, 1u) != 1 || ch != 'p' || user_sys_close((int)fd) != 0) {
        user_sys_putc('!');
        user_sys_exit();
    }

    if (user_sys_write(1, &ok, 1u) != 1 || user_sys_write(1, &ch, 1u) != 1) {
        user_sys_putc('!');
        user_sys_exit();
    }

    user_sys_exit();
#elif defined(USER_TEST_OPEN_FLAGS)
    char banner[12];
    char console[13];
    char devdir[5];
    char ch;
    int64_t fd;
    char ok;

    banner[0] = '/';
    banner[1] = 'e';
    banner[2] = 't';
    banner[3] = 'c';
    banner[4] = '/';
    banner[5] = 'b';
    banner[6] = 'a';
    banner[7] = 'n';
    banner[8] = 'n';
    banner[9] = 'e';
    banner[10] = 'r';
    banner[11] = '\0';

    console[0] = '/';
    console[1] = 'd';
    console[2] = 'e';
    console[3] = 'v';
    console[4] = '/';
    console[5] = 'c';
    console[6] = 'o';
    console[7] = 'n';
    console[8] = 's';
    console[9] = 'o';
    console[10] = 'l';
    console[11] = 'e';
    console[12] = '\0';

    devdir[0] = '/';
    devdir[1] = 'd';
    devdir[2] = 'e';
    devdir[3] = 'v';
    devdir[4] = '\0';

    if (user_sys_open_mode(banner, (uint64_t)SYS_OPEN_WRITE) != -1 ||
        user_sys_open_mode(console, (uint64_t)SYS_OPEN_READ) != -1 ||
        user_sys_open_mode(devdir, (uint64_t)SYS_OPEN_READ) != -1 ||
        user_sys_open_mode(banner, (uint64_t)(SYS_OPEN_READ | SYS_OPEN_WRITE)) != -1) {
        user_sys_putc('!');
        user_sys_exit();
    }

    fd = user_sys_open_mode(banner, (uint64_t)SYS_OPEN_READ);
    ch = 0;
    ok = 'A';
    if (fd < 0 || user_sys_read((int)fd, &ch, 1u) != 1 || ch != 'p' || user_sys_close((int)fd) != 0) {
        user_sys_putc('!');
        user_sys_exit();
    }

    if (user_sys_write(1, &ok, 1u) != 1) {
        user_sys_putc('!');
        user_sys_exit();
    }

    user_sys_exit();
#elif defined(USER_TEST_EXEC)
    char path[11];
    char bad[12];

    bad[0] = '/';
    bad[1] = 'e';
    bad[2] = 't';
    bad[3] = 'c';
    bad[4] = '/';
    bad[5] = 'b';
    bad[6] = 'a';
    bad[7] = 'n';
    bad[8] = 'n';
    bad[9] = 'e';
    bad[10] = 'r';
    bad[11] = '\0';
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

    if (user_sys_exec(bad) != -1) {
        user_sys_putc('!');
        user_sys_exit();
    }

    if (user_sys_exec(path) == -1) {
        user_sys_putc('!');
        user_sys_exit();
    }

    user_sys_putc('!');
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
