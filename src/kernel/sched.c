#include "kernel/sched.h"
#include "kernel/paging.h"
#include "kernel/palloc.h"

#include <stdint.h>

typedef enum task_state {
    TASK_UNUSED = 0,
    TASK_RUNNABLE = 1,
    TASK_RUNNING = 2,
    TASK_DONE = 3
} task_state_t;

typedef struct task {
    uintptr_t rsp;
    task_state_t state;
    task_kind_t kind;
    uint32_t id;
} task_t;

static task_t task0;
static task_t task1;
static task_t *current;
static int sched_started;
static int user_task_enabled;
static void (*user_task_entry)(void);
static volatile uint64_t worker_counter;

static task_t *sched_select_next(task_t *previous)
{
    task_t *other;

    if (previous == &task0) {
        other = &task1;
    } else {
        other = &task0;
    }

    if (other->state == TASK_RUNNABLE) {
        return other;
    }

    if (previous->state == TASK_RUNNABLE) {
        return previous;
    }

    if (task0.state == TASK_RUNNABLE) {
        return &task0;
    }

    if (task1.state == TASK_RUNNABLE) {
        return &task1;
    }

    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}

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
    uint64_t ticks;

    for (;;) {
        ++worker_counter;

        if ((worker_counter % 256u) == 0u) {
            task_syscall(0u, (uint64_t)'S');
        }

        if ((worker_counter % 64u) == 0u) {
            task_syscall(1u, 0u);
        }

        if ((worker_counter % 512u) == 0u) {
            ticks = task_syscall(2u, 0u);
            if (ticks != 0u) {
                task_syscall(0u, (uint64_t)'K');
            }
        }

        task_delay();
    }
}

static uintptr_t task_build_kernel_stack(void (*entry)(void))
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

static uintptr_t task_build_user_stack(void (*entry)(void))
{
    uintptr_t page = palloc_alloc();
    uint64_t *sp;

    if (page == 0u) {
        return 0u;
    }

    paging_mark_user_accessible((uintptr_t)(void *)entry);
    paging_mark_user_accessible(page);

    sp = (uint64_t *)(page + 4096u);

    *--sp = 0x001Bull;
    *--sp = (uint64_t)(page + 4096u);
    *--sp = 0x0000000000000202ull;
    *--sp = 0x0023ull;
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

static int task_create_kernel(task_t *task, uint32_t id, void (*entry)(void))
{
    uintptr_t rsp;

    rsp = task_build_kernel_stack(entry);
    if (rsp == 0u) {
        task->rsp = 0u;
        task->state = TASK_UNUSED;
        return 0;
    }

    task->rsp = rsp;
    task->state = TASK_RUNNABLE;
    task->kind = TASK_KERNEL;
    task->id = id;
    return 1;
}

static int task_create_user(task_t *task, uint32_t id, void (*entry)(void))
{
    uintptr_t rsp;

    rsp = task_build_user_stack(entry);
    if (rsp == 0u) {
        task->rsp = 0u;
        task->state = TASK_UNUSED;
        return 0;
    }

    task->rsp = rsp;
    task->state = TASK_RUNNABLE;
    task->kind = TASK_USER;
    task->id = id;
    return 1;
}

void sched_enable_user_task(void (*entry)(void))
{
    user_task_enabled = 1;
    user_task_entry = entry;
}

void sched_initialize(void)
{
    worker_counter = 0u;
    current = 0;
    sched_started = 0;

    task0.rsp = 0u;
    task0.state = TASK_UNUSED;
    task0.kind = TASK_KERNEL;
    task0.id = 0u;

    task1.rsp = 0u;
    task1.state = TASK_UNUSED;
    task1.kind = TASK_KERNEL;
    task1.id = 1u;

    if (!task_create_kernel(&task0, 0u, task_idle)) {
        for (;;) {
            __asm__ __volatile__("cli; hlt");
        }
    }

    if (user_task_enabled) {
        if (!task_create_user(&task1, 1u, user_task_entry)) {
            for (;;) {
                __asm__ __volatile__("cli; hlt");
            }
        }
    } else if (!task_create_kernel(&task1, 1u, task_worker)) {
        for (;;) {
            __asm__ __volatile__("cli; hlt");
        }
    }
}

uintptr_t sched_tick(uintptr_t current_rsp)
{
    if (!sched_started) {
        sched_started = 1;
        current = user_task_enabled ? &task1 : &task0;
        current->state = TASK_RUNNING;
        return current->rsp;
    }

    if (current->state == TASK_RUNNING) {
        current->rsp = current_rsp;
        current->state = TASK_RUNNABLE;
    }

    current = sched_select_next(current);

    current->state = TASK_RUNNING;
    return current->rsp;
}

uintptr_t sched_exit_current(uintptr_t current_rsp)
{
    (void)current_rsp;

    if (!sched_started || current == 0) {
        for (;;) {
            __asm__ __volatile__("cli; hlt");
        }
    }

    current->rsp = 0u;
    current->state = TASK_DONE;
    current = sched_select_next(current);
    current->state = TASK_RUNNING;
    return current->rsp;
}

uint64_t sched_debug_worker_counter(void)
{
    return worker_counter;
}

uint32_t sched_current_task_id(void)
{
    if (current == 0) {
        return 0u;
    }

    return current->id;
}

task_kind_t sched_current_task_kind(void)
{
    if (current == 0) {
        return TASK_KERNEL;
    }

    return current->kind;
}
