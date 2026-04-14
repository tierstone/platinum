#include "kernel/core.h"
#include "kernel/gdt.h"
#include "kernel/idt.h"
#include "kernel/memmap.h"
#include "kernel/paging.h"
#include "kernel/palloc.h"
#include "kernel/sched.h"
#include "drivers/pic.h"
#include "drivers/pit.h"
#include "drivers/serial.h"

#include <stddef.h>
#include <stdint.h>

static volatile uint64_t kernel_ticks;

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

static void write_line(const char *text) {
    write_text(text);
    serial_write("\r\n", 2u);
}

static void write_decimal_u32(uint32_t value) {
    char digits[10];
    size_t count = 0u;
    uint32_t current = value;

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

void kernel_trap(uint32_t vector) {
    write_text("trap ");
    write_decimal_u32(vector);
    write_text("\r\n");
}

uintptr_t kernel_timer_tick(uintptr_t current_rsp) {
    ++kernel_ticks;

    if ((kernel_ticks % 100u) == 0u) {
        write_text("worker ");
        write_decimal_u64(sched_debug_worker_counter());
        write_text("\r\n");
    }

    pic_eoi(0u);
    return sched_tick(current_rsp);
}

void kernel_main(void *image_handle, void *system_table) {
    struct efi_system_table *system = (struct efi_system_table *)system_table;

    kernel_ticks = 0u;

    serial_init();
    write_line("kernel_main");
    write_line("efi live");

    if (!memmap_capture_and_exit((efi_handle_t)image_handle, system)) {
        write_line("exit fail");
        arch_halt_forever();
    }

    memmap_print_summary();
    write_line("exit ok");

    gdt_initialize();
    write_line("gdt ok");

    idt_initialize();
    idt_set_irq0_gate();
    write_line("idt ok");

    palloc_initialize();
    palloc_self_test();

    paging_initialize();
    write_line("paging 4k ok");

    pic_initialize();
    write_line("pic ok");

    pit_initialize(100u);
    write_line("pit ok");

    sched_initialize();
    write_line("sched ok");

    arch_enable_interrupts();
    write_line("irq on");

    write_line("hello from /p/OS");

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
