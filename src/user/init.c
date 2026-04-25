#include "user/syscall.h"

#include <stdint.h>

void user_init_main(void)
{
#ifdef USER_TEST_DUP_FULL
    int64_t duplicates[16];
    int64_t fd;
    int index;
    char ok[11];

    for (index = 0; index < 16; ++index) {
        duplicates[index] = -1;
    }

    for (index = 0; index < 13; ++index) {
        fd = user_sys_dup(1);
        if (fd < 0) {
            user_sys_putc('!');
            user_sys_exit();
        }
        duplicates[index] = fd;
    }

    if (user_sys_dup(1) != -1) {
        user_sys_putc('!');
        user_sys_exit();
    }

    for (index = 0; index < 13; ++index) {
        if (user_sys_close((int)duplicates[index]) != 0) {
            user_sys_putc('!');
            user_sys_exit();
        }
    }

    ok[0] = 'd';
    ok[1] = 'u';
    ok[2] = 'p';
    ok[3] = 'f';
    ok[4] = 'u';
    ok[5] = 'l';
    ok[6] = 'l';
    ok[7] = ' ';
    ok[8] = 'o';
    ok[9] = 'k';
    ok[10] = '\n';
    if (user_sys_write(1, ok, 11u) != 11) {
        user_sys_putc('!');
        user_sys_exit();
    }

    user_sys_exit();
#elif defined(USER_TEST_BAD_POINTERS)
    char ok[7];

    if (user_sys_read(0, (void *)(uintptr_t)0u, 1u) != -1 ||
        user_sys_write(1, (const void *)(uintptr_t)0u, 1u) != -1 ||
        user_sys_read(0, (void *)(uintptr_t)0x50000000ull, 1u) != -1 ||
        user_sys_write(1, (const void *)(uintptr_t)0x401fffffull, 2u) != -1 ||
        user_sys_open((const char *)(uintptr_t)0x50000000ull) != -1 ||
        user_sys_exec((const char *)(uintptr_t)0x50000000ull) != -1) {
        user_sys_putc('!');
        user_sys_exit();
    }

    ok[0] = 'p';
    ok[1] = 't';
    ok[2] = 'r';
    ok[3] = ' ';
    ok[4] = 'o';
    ok[5] = 'k';
    ok[6] = '\n';
    if (user_sys_write(1, ok, 7u) != 7) {
        user_sys_putc('!');
        user_sys_exit();
    }

    user_sys_exit();
#elif defined(USER_TEST_EXEC_LOOP)
    char path[11];

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
#elif defined(USER_TEST_EXEC_BAD_LOOP)
    char path[11];
    char ok[12];

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

    while (user_sys_get_ticks() < 4u) {
        if (user_sys_exec(path) != -1) {
            user_sys_putc('!');
            user_sys_exit();
        }
    }

    ok[0] = 'e';
    ok[1] = 'x';
    ok[2] = 'e';
    ok[3] = 'c';
    ok[4] = 'b';
    ok[5] = 'a';
    ok[6] = 'd';
    ok[7] = ' ';
    ok[8] = 'o';
    ok[9] = 'k';
    ok[10] = '\n';
    ok[11] = '\0';
    if (user_sys_write(1, ok, 11u) != 11) {
        user_sys_putc('!');
        user_sys_exit();
    }

    user_sys_exit();
#elif defined(USER_TEST_EXEC_TRANSFER_FAIL)
    char path[11];
    char ok[12];

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

    if (user_sys_exec(path) != -1) {
        user_sys_putc('!');
        user_sys_exit();
    }

    ok[0] = 'e';
    ok[1] = 'x';
    ok[2] = 'e';
    ok[3] = 'c';
    ok[4] = 'x';
    ok[5] = 'f';
    ok[6] = 'e';
    ok[7] = 'r';
    ok[8] = ' ';
    ok[9] = 'o';
    ok[10] = 'k';
    ok[11] = '\n';
    ok[12] = '\0';
    if (user_sys_write(1, ok, 12u) != 12) {
        user_sys_putc('!');
        user_sys_exit();
    }

    user_sys_exit();
#elif defined(USER_TEST_EXEC_REGISTRY)
    char pulse[11];
    char missing[13];

    missing[0] = '/';
    missing[1] = 'b';
    missing[2] = 'i';
    missing[3] = 'n';
    missing[4] = '/';
    missing[5] = 'm';
    missing[6] = 'i';
    missing[7] = 's';
    missing[8] = 's';
    missing[9] = 'i';
    missing[10] = 'n';
    missing[11] = 'g';
    missing[12] = '\0';

    pulse[0] = '/';
    pulse[1] = 'b';
    pulse[2] = 'i';
    pulse[3] = 'n';
    pulse[4] = '/';
    pulse[5] = 'p';
    pulse[6] = 'u';
    pulse[7] = 'l';
    pulse[8] = 's';
    pulse[9] = 'e';
    pulse[10] = '\0';

    if (user_sys_exec(missing) != -1) {
        user_sys_putc('!');
        user_sys_exit();
    }

    if (user_sys_exec(pulse) == -1) {
        user_sys_putc('!');
        user_sys_exit();
    }

    user_sys_putc('!');
    user_sys_exit();
#elif defined(USER_TEST_EXEC_PATHS)
    char trailing[12];
    char doubled[12];
    char ok[13];

    trailing[0] = '/';
    trailing[1] = 'b';
    trailing[2] = 'i';
    trailing[3] = 'n';
    trailing[4] = '/';
    trailing[5] = 'p';
    trailing[6] = 'u';
    trailing[7] = 'l';
    trailing[8] = 's';
    trailing[9] = 'e';
    trailing[10] = '/';
    trailing[11] = '\0';

    doubled[0] = '/';
    doubled[1] = 'b';
    doubled[2] = 'i';
    doubled[3] = 'n';
    doubled[4] = '/';
    doubled[5] = '/';
    doubled[6] = 'p';
    doubled[7] = 'u';
    doubled[8] = 'l';
    doubled[9] = 's';
    doubled[10] = 'e';
    doubled[11] = '\0';

    if (user_sys_exec(trailing) != -1 || user_sys_exec(doubled) != -1) {
        user_sys_putc('!');
        user_sys_exit();
    }

    ok[0] = 'e';
    ok[1] = 'x';
    ok[2] = 'e';
    ok[3] = 'c';
    ok[4] = 'p';
    ok[5] = 'a';
    ok[6] = 't';
    ok[7] = 'h';
    ok[8] = ' ';
    ok[9] = 'o';
    ok[10] = 'k';
    ok[11] = '\n';
    ok[12] = '\0';
    if (user_sys_write(1, ok, 12u) != 12) {
        user_sys_putc('!');
        user_sys_exit();
    }

    user_sys_exit();
#elif defined(USER_TEST_EXEC_DIR_ROOT)
    char root[2];
    char ok[13];

    root[0] = '/';
    root[1] = '\0';

    if (user_sys_exec(root) != -1) {
        user_sys_putc('!');
        user_sys_exit();
    }

    ok[0] = 'e';
    ok[1] = 'x';
    ok[2] = 'e';
    ok[3] = 'c';
    ok[4] = 'r';
    ok[5] = 'o';
    ok[6] = 'o';
    ok[7] = 't';
    ok[8] = ' ';
    ok[9] = 'o';
    ok[10] = 'k';
    ok[11] = '\n';
    ok[12] = '\0';
    if (user_sys_write(1, ok, 12u) != 12) {
        user_sys_putc('!');
        user_sys_exit();
    }

    user_sys_exit();
#elif defined(USER_TEST_EXEC_NOENT)
    char path[10];
    char ok[14];

    path[0] = '/';
    path[1] = 'b';
    path[2] = 'i';
    path[3] = 'n';
    path[4] = '/';
    path[5] = 'n';
    path[6] = 'o';
    path[7] = 'p';
    path[8] = 'e';
    path[9] = '\0';

    if (user_sys_exec(path) != -1) {
        user_sys_putc('!');
        user_sys_exit();
    }

    ok[0] = 'e';
    ok[1] = 'x';
    ok[2] = 'e';
    ok[3] = 'c';
    ok[4] = 'n';
    ok[5] = 'o';
    ok[6] = 'e';
    ok[7] = 'n';
    ok[8] = 't';
    ok[9] = ' ';
    ok[10] = 'o';
    ok[11] = 'k';
    ok[12] = '\n';
    ok[13] = '\0';
    if (user_sys_write(1, ok, 13u) != 13) {
        user_sys_putc('!');
        user_sys_exit();
    }

    user_sys_exit();
#elif defined(USER_TEST_EXEC_NONEXEC)
    char path[12];
    char ok[16];

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

    if (user_sys_exec(path) != -1) {
        user_sys_putc('!');
        user_sys_exit();
    }

    ok[0] = 'e';
    ok[1] = 'x';
    ok[2] = 'e';
    ok[3] = 'c';
    ok[4] = 'n';
    ok[5] = 'o';
    ok[6] = 'n';
    ok[7] = 'e';
    ok[8] = 'x';
    ok[9] = 'e';
    ok[10] = 'c';
    ok[11] = ' ';
    ok[12] = 'o';
    ok[13] = 'k';
    ok[14] = '\n';
    ok[15] = '\0';
    if (user_sys_write(1, ok, 15u) != 15) {
        user_sys_putc('!');
        user_sys_exit();
    }

    user_sys_exit();
#elif defined(USER_TEST_EXEC_BAD_ELF2)
    char path[13];
    char ok[16];

    path[0] = '/';
    path[1] = 'b';
    path[2] = 'i';
    path[3] = 'n';
    path[4] = '/';
    path[5] = 'b';
    path[6] = 'a';
    path[7] = 'd';
    path[8] = 'e';
    path[9] = 'l';
    path[10] = 'f';
    path[11] = '2';
    path[12] = '\0';

    if (user_sys_exec(path) != -1) {
        user_sys_putc('!');
        user_sys_exit();
    }

    ok[0] = 'e';
    ok[1] = 'x';
    ok[2] = 'e';
    ok[3] = 'c';
    ok[4] = 'b';
    ok[5] = 'a';
    ok[6] = 'd';
    ok[7] = 'e';
    ok[8] = 'l';
    ok[9] = 'f';
    ok[10] = '2';
    ok[11] = ' ';
    ok[12] = 'o';
    ok[13] = 'k';
    ok[14] = '\n';
    ok[15] = '\0';
    if (user_sys_write(1, ok, 15u) != 15) {
        user_sys_putc('!');
        user_sys_exit();
    }

    user_sys_exit();
#elif defined(USER_TEST_EXEC_BAD_PATHS)
    char relative[10];
    char empty[1];
    char doubleslash[3];
    char tripleslash[4];
    char trailing[6];
    char ok[17];

    relative[0] = 'b';
    relative[1] = 'i';
    relative[2] = 'n';
    relative[3] = '/';
    relative[4] = 'p';
    relative[5] = 'u';
    relative[6] = 'l';
    relative[7] = 's';
    relative[8] = 'e';
    relative[9] = '\0';

    empty[0] = '\0';

    doubleslash[0] = '/';
    doubleslash[1] = '/';
    doubleslash[2] = '\0';

    tripleslash[0] = '/';
    tripleslash[1] = '/';
    tripleslash[2] = '/';
    tripleslash[3] = '\0';

    trailing[0] = '/';
    trailing[1] = 'b';
    trailing[2] = 'i';
    trailing[3] = 'n';
    trailing[4] = '/';
    trailing[5] = '\0';

    if (user_sys_exec(relative) != -1 ||
        user_sys_exec(empty) != -1 ||
        user_sys_exec(doubleslash) != -1 ||
        user_sys_exec(tripleslash) != -1 ||
        user_sys_exec(trailing) != -1) {
        user_sys_putc('!');
        user_sys_exit();
    }

    ok[0] = 'e';
    ok[1] = 'x';
    ok[2] = 'e';
    ok[3] = 'c';
    ok[4] = 'b';
    ok[5] = 'a';
    ok[6] = 'd';
    ok[7] = 'p';
    ok[8] = 'a';
    ok[9] = 't';
    ok[10] = 'h';
    ok[11] = 's';
    ok[12] = ' ';
    ok[13] = 'o';
    ok[14] = 'k';
    ok[15] = '\n';
    ok[16] = '\0';
    if (user_sys_write(1, ok, 16u) != 16) {
        user_sys_putc('!');
        user_sys_exit();
    }

    user_sys_exit();
#elif defined(USER_TEST_EXEC_SECOND)
    char path[12];

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
    path[10] = '2';
    path[11] = '\0';

    if (user_sys_exec(path) == -1) {
        user_sys_putc('!');
        user_sys_exit();
    }

    user_sys_exit();
#elif defined(USER_TEST_FD_WRITE)
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
