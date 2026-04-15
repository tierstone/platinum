#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>

typedef enum task_kind {
    TASK_KERNEL = 0,
    TASK_USER = 1
} task_kind_t;

struct user_virtual_layout {
    uintptr_t trampoline_base;
    uintptr_t image_base;
    uintptr_t stack_top;
};

struct user_task_bootstrap {
    struct user_virtual_layout layout;
    uintptr_t address_space;
    void (*trampoline_entry)(void);
    void (*user_entry)(void);
    uintptr_t user_stack_page;
    uintptr_t user_stack_top;
};

void sched_enable_user_task(const struct user_task_bootstrap *bootstrap);
void sched_initialize(void);
uintptr_t sched_exit_current(uintptr_t current_rsp);
uintptr_t sched_tick(uintptr_t current_rsp);
uint32_t sched_current_task_id(void);
task_kind_t sched_current_task_kind(void);
uint64_t sched_debug_worker_counter(void);

#endif
