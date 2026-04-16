#ifndef COMMON_SYSCALL_H
#define COMMON_SYSCALL_H

#include <stdint.h>

enum {
    SYS_PUTC = 0,
    SYS_YIELD = 1,
    SYS_GET_TICKS = 2,
    SYS_EXIT = 3,
    SYS_READ = 4,
    SYS_WRITE = 5,
    SYS_CLOSE = 6,
    SYS_DUP = 7,
    SYS_OPEN = 8
};

enum {
    SYS_RESULT_OK = 0
};

#define SYS_RESULT_ERROR ((uint64_t)-1)

#endif
