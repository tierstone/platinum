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

static uint64_t task_syscall(uint64_t number, uint64_t arg0)
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

static void task_delay(void)
{
    volatile uint64_t i;

    for (i = 0u; i < 1000000ull; ++i) {
    }
}

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

        if ((worker_counter % 256u) == 0u) {
            task_syscall(0u, (uint64_t)'S');
        }

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

static int task_create(task_t *task, void (*entry)(void))
{
    uintptr_t rsp;

    rsp = task_build_stack(entry);
    if (rsp == 0u) {
        task->rsp = 0u;
        task->state = TASK_UNUSED;
        return 0;
    }

    task->rsp = rsp;
    task->state = TASK_RUNNABLE;
    return 1;
}

void sched_initialize(void)
{
    worker_counter = 0u;
    current = 0;
    sched_started = 0;

    task0.rsp = 0u;
    task0.state = TASK_UNUSED;

    task1.rsp = 0u;
    task1.state = TASK_UNUSED;

    if (!task_create(&task0, task_idle)) {
        for (;;) {
            __asm__ __volatile__("cli; hlt");
        }
    }

    if (!task_create(&task1, task_worker)) {
        for (;;) {
            __asm__ __volatile__("cli; hlt");
        }
    }
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
