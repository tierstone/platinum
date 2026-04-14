#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>

void sched_initialize(void);
uintptr_t sched_tick(uintptr_t current_rsp);
uint64_t sched_debug_worker_counter(void);

#endif
