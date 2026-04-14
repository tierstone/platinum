#include "kernel/sched.h"
#include "drivers/serial.h"
#include "kernel/palloc.h"

#include <stddef.h>
#include <stdint.h>

typedef struct task {
    uintptr_t rsp;
} task_t;

static uint64_t sched_ticks;
static task_t task0;
static task_t task1;
static task_t *current;
static int sched_started;

static void task_delay(void)
{
    volatile uint64_t i;

    for (i = 0u; i < 1000000ull; ++i) {
    }
}

static void task_idle(void)
{
    for (;;) {
        serial_write("I", 1u);
        task_delay();
    }
}

static void task_test(void)
{
    for (;;) {
        serial_write("T", 1u);
        task_delay();
    }
}

static uintptr_t task_build_stack(void (*entry)(void))
{
    uintptr_t page = palloc_alloc();
    uint64_t *sp;

    if (page == 0u) {
        return 0u;
    }

    sp = (uint64_t *)(page + 4096u);

    *--sp = 0x0010ull;
    *--sp = (uint64_t)(uintptr_t)(page + 4096u - 8u);
    *--sp = 0x0000000000000202ull;
    *--sp = 0x0008ull;
    *--sp = (uint64_t)(uintptr_t)entry;

    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;

    return (uintptr_t)sp;
}

void sched_initialize(void) {
    sched_ticks = 0u;
    task0.rsp = task_build_stack(task_idle);
    task1.rsp = task_build_stack(task_test);
    current = NULL;
    sched_started = 0;
}

uintptr_t sched_tick(uintptr_t current_rsp)
{
    ++sched_ticks;

    if (!sched_started) {
        sched_started = 1;
        current = &task0;
        return current->rsp;
    }

    current->rsp = current_rsp;

    if (current == &task0) {
        current = &task1;
    } else {
        current = &task0;
    }

    return current->rsp;
}
