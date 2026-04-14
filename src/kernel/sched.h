#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>

typedef enum task_kind {
    TASK_KERNEL = 0,
    TASK_USER = 1
} task_kind_t;

void sched_enable_user_task(void (*entry)(void), void (*code)(void));
void sched_initialize(void);
uintptr_t sched_exit_current(uintptr_t current_rsp);
uintptr_t sched_tick(uintptr_t current_rsp);
uint32_t sched_current_task_id(void);
task_kind_t sched_current_task_kind(void);
uint64_t sched_debug_worker_counter(void);

#endif
