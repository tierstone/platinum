#include "kernel/core.h"
#include "kernel/gdt.h"
#include "kernel/idt.h"
#include "kernel/memmap.h"
#include "kernel/paging.h"
#include "kernel/palloc.h"
#include "drivers/serial.h"

#include <stddef.h>
#include <stdint.h>

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

void kernel_trap(uint32_t vector) {
    write_text("trap ");
    write_decimal_u32(vector);
    write_text("\r\n");
}

void kernel_main(void *image_handle, void *system_table) {
    struct efi_system_table *system = (struct efi_system_table *)system_table;

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
    write_line("idt ok");

    paging_initialize();
    write_line("paging ok");

    palloc_initialize();
    palloc_self_test();

    write_line("hello from /p/OS");

    arch_halt_forever();
}
