#include "kernel/sched.h"
#include "kernel/palloc.h"

#include <stdint.h>

typedef enum task_state {
    TASK_UNUSED = 0,
    TASK_RUNNABLE = 1,
    TASK_RUNNING = 2
} task_state_t;

typedef struct task {
    uintptr_t rsp;
    task_state_t state;
} task_t;

static task_t task0;
static task_t task1;
static task_t *current;
static int sched_started;
static volatile uint64_t worker_counter;

static void task_idle(void)
{
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

static void task_worker(void)
{
    for (;;) {
        ++worker_counter;
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

void sched_initialize(void)
{
    worker_counter = 0u;

    task0.rsp = task_build_stack(task_idle);
    task0.state = TASK_RUNNABLE;

    task1.rsp = task_build_stack(task_worker);
    task1.state = TASK_RUNNABLE;

    current = 0;
    sched_started = 0;
}

uintptr_t sched_tick(uintptr_t current_rsp)
{
    if (!sched_started) {
        sched_started = 1;
        current = &task0;
        current->state = TASK_RUNNING;
        return current->rsp;
    }

    current->rsp = current_rsp;
    current->state = TASK_RUNNABLE;

    if (current == &task0) {
        current = &task1;
    } else {
        current = &task0;
    }

    current->state = TASK_RUNNING;
    return current->rsp;
}

uint64_t sched_debug_worker_counter(void)
{
    return worker_counter;
}
