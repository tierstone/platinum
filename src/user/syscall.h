#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include "common/syscall.h"

#include <stdint.h>

static inline uint64_t user_syscall1(uint64_t number, uint64_t arg0)
{
    uint64_t result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(number), "D"(arg0)
        : "memory"
    );

    return result;
}

static inline void user_sys_putc(char ch)
{
    (void)user_syscall1((uint64_t)SYS_PUTC, (uint64_t)(uint8_t)ch);
}

static inline void user_sys_yield(void)
{
    (void)user_syscall1((uint64_t)SYS_YIELD, 0u);
}

static inline uint64_t user_sys_get_ticks(void)
{
    return user_syscall1((uint64_t)SYS_GET_TICKS, 0u);
}

static inline void user_sys_exit(void)
{
    (void)user_syscall1((uint64_t)SYS_EXIT, 0u);
}

#endif
