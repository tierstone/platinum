#include "kernel/fd.h"
#include "kernel/elf.h"
#include "kernel/sched.h"
#include "kernel/paging.h"
#include "kernel/palloc.h"
#include "drivers/serial.h"

#include <stddef.h>
#include <stdint.h>

typedef enum task_state {
    TASK_UNUSED = 0,
    TASK_RUNNABLE = 1,
    TASK_RUNNING = 2,
    TASK_DONE = 3
} task_state_t;

typedef struct task {
    uintptr_t rsp;
    uintptr_t cr3;
    task_state_t state;
    task_kind_t kind;
    uint32_t id;
    struct fd_table fd_table;
    struct user_task_bootstrap user_bootstrap;
} task_t;

static task_t task0;
static task_t task1;
static task_t *current;
static int sched_started;
static int user_task_enabled;
static struct user_task_bootstrap user_task_bootstrap;
static volatile uint64_t worker_counter;

static size_t string_length(const char *text)
{
    size_t length;

    length = 0u;
    while (text[length] != '\0') {
        ++length;
    }

    return length;
}

static void write_text(const char *text)
{
    serial_write(text, string_length(text));
}

static void write_line(const char *text)
{
    write_text(text);
    serial_write("\r\n", 2u);
}

static void sched_fail(const char *text)
{
    write_line(text);

    for (;;) {
        __asm__ __volatile__("cli; hlt");
    }
}

static void sched_validate_task(const task_t *task, task_state_t expected_state, const char *text)
{
    if (task == 0) {
        sched_fail(text);
    }
    if (task->state != expected_state) {
        sched_fail(text);
    }
    if (task->rsp == 0u) {
        sched_fail(text);
    }
    if (task->kind == TASK_KERNEL) {
        if (task->cr3 != paging_kernel_address_space()) {
            sched_fail("sched kernel cr3");
        }
        return;
    }
    if (task->cr3 == 0u || task->cr3 == paging_kernel_address_space()) {
        sched_fail("sched user cr3");
    }
}

static void sched_clear_user_bootstrap(struct user_task_bootstrap *bootstrap)
{
    if (bootstrap == 0) {
        return;
    }

    bootstrap->layout.trampoline_base = 0u;
    bootstrap->layout.image_base = 0u;
    bootstrap->layout.stack_top = 0u;
    bootstrap->address_space = 0u;
    bootstrap->trampoline_entry = 0;
    bootstrap->user_entry = 0;
    bootstrap->image_physical_begin = 0u;
    bootstrap->image_physical_end = 0u;
    bootstrap->user_stack_page = 0u;
    bootstrap->user_stack_top = 0u;
}

static void sched_release_user_bootstrap(struct user_task_bootstrap *bootstrap)
{
    struct loaded_user_image loaded_image;

    if (bootstrap == 0 || bootstrap->address_space == 0u) {
        return;
    }

    loaded_image.load_begin = 0u;
    loaded_image.load_end = 0u;
    loaded_image.load_physical_begin = bootstrap->image_physical_begin;
    loaded_image.load_physical_end = bootstrap->image_physical_end;
    loaded_image.entry = 0;
    loaded_image.stack_page = bootstrap->user_stack_page;
    loaded_image.stack_top = bootstrap->user_stack_top;

    elf_release_user_image(&loaded_image);
    paging_release_user_address_space(bootstrap->address_space, &bootstrap->layout);
    sched_clear_user_bootstrap(bootstrap);
}

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

static uintptr_t task_build_user_stack(const struct user_task_bootstrap *bootstrap)
{
    uintptr_t stack_top;
    uintptr_t stack_page;
    uint64_t *sp;

    stack_top = bootstrap->user_stack_top;
    if (stack_top == 0u) {
        return 0u;
    }

    stack_page = bootstrap->user_stack_page;
    if (stack_page == 0u) {
        return 0u;
    }

    sp = (uint64_t *)(stack_page + 4096u);

    *--sp = 0x001Bull;
    *--sp = (uint64_t)stack_top;
    *--sp = 0x0000000000000202ull;
    *--sp = 0x0023ull;
    *--sp = (uint64_t)(uintptr_t)bootstrap->trampoline_entry;

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
    *--sp = (uint64_t)(uintptr_t)bootstrap->user_entry;
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
    task->cr3 = paging_kernel_address_space();
    task->state = TASK_RUNNABLE;
    task->kind = TASK_KERNEL;
    task->id = id;
    fd_table_initialize(&task->fd_table);
    sched_clear_user_bootstrap(&task->user_bootstrap);
    if (!fd_table_seed_console(&task->fd_table)) {
        return 0;
    }
    return 1;
}

static int task_create_user(task_t *task, uint32_t id, const struct user_task_bootstrap *bootstrap)
{
    uintptr_t rsp;

    if (bootstrap->address_space == 0u ||
        bootstrap->trampoline_entry == 0 ||
        bootstrap->user_entry == 0 ||
        bootstrap->user_stack_page == 0u ||
        bootstrap->user_stack_top == 0u) {
        return 0;
    }

    rsp = task_build_user_stack(bootstrap);
    if (rsp == 0u) {
        task->rsp = 0u;
        task->state = TASK_UNUSED;
        return 0;
    }

    task->rsp = rsp;
    task->cr3 = bootstrap->address_space;
    task->state = TASK_RUNNABLE;
    task->kind = TASK_USER;
    task->id = id;
    task->user_bootstrap = *bootstrap;
    fd_table_initialize(&task->fd_table);
    if (!fd_table_seed_console(&task->fd_table)) {
        return 0;
    }
    return 1;
}

void sched_enable_user_task(const struct user_task_bootstrap *bootstrap)
{
    if (bootstrap->address_space == 0u ||
        bootstrap->layout.trampoline_base == 0u ||
        bootstrap->layout.image_base == 0u ||
        bootstrap->layout.stack_top == 0u ||
        bootstrap->trampoline_entry == 0 ||
        bootstrap->user_entry == 0 ||
        bootstrap->user_stack_page == 0u ||
        bootstrap->user_stack_top == 0u) {
        sched_fail("sched bootstrap fail");
    }

    user_task_enabled = 1;
    user_task_bootstrap = *bootstrap;
}

void sched_initialize(void)
{
    worker_counter = 0u;
    current = 0;
    sched_started = 0;

    task0.rsp = 0u;
    task0.cr3 = 0u;
    task0.state = TASK_UNUSED;
    task0.kind = TASK_KERNEL;
    task0.id = 0u;
    sched_clear_user_bootstrap(&task0.user_bootstrap);
    fd_table_initialize(&task0.fd_table);

    task1.rsp = 0u;
    task1.cr3 = 0u;
    task1.state = TASK_UNUSED;
    task1.kind = TASK_KERNEL;
    task1.id = 1u;
    sched_clear_user_bootstrap(&task1.user_bootstrap);
    fd_table_initialize(&task1.fd_table);

    if (!task_create_kernel(&task0, 0u, task_idle)) {
        for (;;) {
            __asm__ __volatile__("cli; hlt");
        }
    }

    if (user_task_enabled) {
        if (!task_create_user(&task1, 1u, &user_task_bootstrap)) {
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
        sched_validate_task(current, TASK_RUNNABLE, "sched start state");
        paging_activate(current->cr3);
        current->state = TASK_RUNNING;
        return current->rsp;
    }

    if (current->state == TASK_RUNNING) {
        if (current->rsp == 0u) {
            sched_fail("sched current rsp");
        }
        current->rsp = current_rsp;
        current->state = TASK_RUNNABLE;
    } else {
        sched_fail("sched tick state");
    }

    current = sched_select_next(current);

    sched_validate_task(current, TASK_RUNNABLE, "sched next state");
    paging_activate(current->cr3);
    current->state = TASK_RUNNING;
    return current->rsp;
}

uintptr_t sched_exit_current(uintptr_t current_rsp)
{
    struct user_task_bootstrap finished_bootstrap;

    (void)current_rsp;

    if (!sched_started || current == 0) {
        sched_fail("sched exit state");
    }

    fd_table_close_all(&current->fd_table);
    finished_bootstrap = current->user_bootstrap;
    sched_clear_user_bootstrap(&current->user_bootstrap);
    current->rsp = 0u;
    current->state = TASK_DONE;
    current = sched_select_next(current);
    sched_validate_task(current, TASK_RUNNABLE, "sched exit next");
    paging_activate(current->cr3);
    current->state = TASK_RUNNING;
    sched_release_user_bootstrap(&finished_bootstrap);
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

struct fd_table *sched_current_fd_table(void)
{
    if (current == 0) {
        return 0;
    }

    return &current->fd_table;
}

int sched_current_user_context(struct user_virtual_layout *layout, uintptr_t *address_space)
{
    if (current == 0 || current->kind != TASK_USER || current->state != TASK_RUNNING) {
        return 0;
    }

    if (layout != 0) {
        *layout = current->user_bootstrap.layout;
    }

    if (address_space != 0) {
        *address_space = current->user_bootstrap.address_space;
    }

    return current->user_bootstrap.address_space != 0u;
}

int sched_current_user_bootstrap(struct user_task_bootstrap *bootstrap)
{
    if (bootstrap == 0 || current == 0 || current->kind != TASK_USER || current->state != TASK_RUNNING) {
        return 0;
    }

    *bootstrap = current->user_bootstrap;
    return current->user_bootstrap.address_space != 0u;
}

uintptr_t sched_exec_current(const struct user_task_bootstrap *bootstrap)
{
    struct user_task_bootstrap previous_bootstrap;
    uintptr_t rsp;

    if (!sched_started || current == 0 || current->kind != TASK_USER || current->state != TASK_RUNNING) {
        return 0u;
    }

    if (bootstrap->address_space == 0u ||
        bootstrap->trampoline_entry == 0 ||
        bootstrap->user_entry == 0 ||
        bootstrap->user_stack_page == 0u ||
        bootstrap->user_stack_top == 0u) {
        return 0u;
    }

    rsp = task_build_user_stack(bootstrap);
    if (rsp == 0u) {
        return 0u;
    }

    /* On success, scheduler consumes bootstrap ownership and releases prior task image. */
    fd_table_close_all(&current->fd_table);
    fd_table_initialize(&current->fd_table);
    if (!fd_table_seed_console(&current->fd_table)) {
        return 0u;
    }

    previous_bootstrap = current->user_bootstrap;
    if (previous_bootstrap.image_physical_begin == bootstrap->image_physical_begin &&
        previous_bootstrap.image_physical_end == bootstrap->image_physical_end) {
        previous_bootstrap.image_physical_begin = 0u;
        previous_bootstrap.image_physical_end = 0u;
    }
    current->rsp = rsp;
    current->cr3 = bootstrap->address_space;
    current->user_bootstrap = *bootstrap;
    paging_activate(current->cr3);
    sched_release_user_bootstrap(&previous_bootstrap);
    return current->rsp;
}
