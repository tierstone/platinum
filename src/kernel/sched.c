#include "kernel/sched.h"
#include "drivers/serial.h"

#include <stddef.h>
#include <stdint.h>

typedef struct task {
    uintptr_t rsp;
} task_t;

static uint64_t sched_ticks;

static task_t task0;
static task_t task1;

static task_t *current;

static size_t string_length(const char *text) {
    size_t length = 0u;

    while (text[length] != '\0') {
        ++length;
    }

    return length;
}

static void write_text(const char *text) {
    serial_write(text, string_length(text));
}

static void write_decimal_u64(uint64_t value) {
    char digits[32];
    size_t count = 0u;
    uint64_t current = value;

    if (current == 0u) {
        serial_write("0", 1u);
        return;
    }

    while (current != 0u) {
        digits[count] = (char)('0' + (current % 10u));
        current /= 10u;
        ++count;
    }

    while (count != 0u) {
        --count;
        serial_write(&digits[count], 1u);
    }
}

static uintptr_t current_task_rsp;

void sched_initialize(void) {
    sched_ticks = 0u;

    task0.rsp = 0u;
    task1.rsp = 0u;

    current = &task0;
}

uintptr_t sched_tick(uintptr_t current_rsp)
{
    ++sched_ticks;

    current->rsp = current_rsp;

    if ((sched_ticks % 100u) == 0u) {
        write_text("sched ");
        write_decimal_u64(sched_ticks);
        write_text("\r\n");
    }

    return current->rsp;
}
