#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

#include "common/syscall.h"

#include <stdint.h>

static inline uint64_t user_syscall0(uint64_t number)
{
    uint64_t result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(number)
        : "memory"
    );

    return result;
}

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

static inline uint64_t user_syscall2(uint64_t number, uint64_t arg0, uint64_t arg1)
{
    uint64_t result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(number), "D"(arg0), "S"(arg1)
        : "memory"
    );

    return result;
}

static inline uint64_t user_syscall3(uint64_t number, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    uint64_t result;

    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(number), "D"(arg0), "S"(arg1), "d"(arg2)
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
    (void)user_syscall0((uint64_t)SYS_YIELD);
}

static inline uint64_t user_sys_get_ticks(void)
{
    return user_syscall0((uint64_t)SYS_GET_TICKS);
}

static inline void user_sys_exit(void)
{
    (void)user_syscall0((uint64_t)SYS_EXIT);
}

static inline int64_t user_sys_read(int fd, void *buffer, uint64_t count)
{
    return (int64_t)user_syscall3((uint64_t)SYS_READ, (uint64_t)fd, (uint64_t)(uintptr_t)buffer, count);
}

static inline int64_t user_sys_write(int fd, const void *buffer, uint64_t count)
{
    return (int64_t)user_syscall3((uint64_t)SYS_WRITE, (uint64_t)fd, (uint64_t)(uintptr_t)buffer, count);
}

static inline int64_t user_sys_close(int fd)
{
    return (int64_t)user_syscall1((uint64_t)SYS_CLOSE, (uint64_t)fd);
}

static inline int64_t user_sys_dup(int fd)
{
    return (int64_t)user_syscall1((uint64_t)SYS_DUP, (uint64_t)fd);
}

static inline int64_t user_sys_open(const char *path)
{
    return (int64_t)user_syscall2((uint64_t)SYS_OPEN, (uint64_t)(uintptr_t)path, (uint64_t)SYS_OPEN_READ);
}

static inline int64_t user_sys_open_mode(const char *path, uint64_t flags)
{
    return (int64_t)user_syscall2((uint64_t)SYS_OPEN, (uint64_t)(uintptr_t)path, flags);
}

static inline int64_t user_sys_exec(const char *path)
{
    return (int64_t)user_syscall1((uint64_t)SYS_EXEC, (uint64_t)(uintptr_t)path);
}

#endif
