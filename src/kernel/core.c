#include "kernel/core.h"
#include "kernel/elf.h"
#include "kernel/gdt.h"
#include "kernel/idt.h"
#include "kernel/memmap.h"
#include "kernel/paging.h"
#include "kernel/palloc.h"
#include "kernel/sched.h"
#include "common/syscall.h"
#include "drivers/pic.h"
#include "drivers/pit.h"
#include "drivers/serial.h"

#include <stddef.h>
#include <stdint.h>

struct syscall_frame {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
};

static volatile uint64_t kernel_ticks;
static const struct user_virtual_layout first_user_layout = {
    0x0000000040000000ull,
    0x0000000040001000ull,
    0x0000000040200000ull
};

#ifndef USER_INIT_ENABLED
#define USER_INIT_ENABLED 0
#endif

#ifndef USER_INIT_USE_ELF
#define USER_INIT_USE_ELF 0
#endif

void user_init_main(void);
extern const uint8_t embedded_user_program_elf[];
extern const size_t embedded_user_program_elf_size;
static void write_line(const char *text);

static void configure_first_user_task(void) {
    struct user_task_bootstrap bootstrap;

    bootstrap.layout = first_user_layout;
    bootstrap.address_space = paging_create_user_address_space();
    if (bootstrap.address_space == 0u) {
        write_line("user as fail");
        arch_halt_forever();
    }

    if (USER_INIT_USE_ELF) {
        struct loaded_user_image loaded_image;
        uintptr_t trampoline_page;
        uintptr_t trampoline_entry;

        if (!elf_load_user_image(
                embedded_user_program_elf,
                embedded_user_program_elf_size,
                &bootstrap.layout,
                bootstrap.address_space,
                &loaded_image)) {
            write_line("user elf fail");
            arch_halt_forever();
        }

        trampoline_page = (uintptr_t)(void *)arch_user_program_entry & ~(uintptr_t)4095u;
        paging_map_user_page(bootstrap.address_space, bootstrap.layout.trampoline_base, trampoline_page);
        trampoline_entry = bootstrap.layout.trampoline_base +
            ((uintptr_t)(void *)arch_user_program_entry & (uintptr_t)4095u);

        bootstrap.trampoline_entry = (void (*)(void))trampoline_entry;
        bootstrap.user_entry = loaded_image.entry;
        bootstrap.user_stack_page = loaded_image.stack_page;
        bootstrap.user_stack_top = loaded_image.stack_top;
    } else {
        uintptr_t trampoline_page;
        uintptr_t user_entry_page;
        uintptr_t user_stack_page;
        uintptr_t trampoline_entry;
        uintptr_t user_entry;

        user_stack_page = palloc_alloc();
        if (user_stack_page == 0u) {
            write_line("user stack fail");
            arch_halt_forever();
        }

        trampoline_page = (uintptr_t)(void *)arch_user_program_entry & ~(uintptr_t)4095u;
        user_entry_page = (uintptr_t)(void *)user_init_main & ~(uintptr_t)4095u;

        paging_map_user_page(bootstrap.address_space, bootstrap.layout.trampoline_base, trampoline_page);
        paging_map_user_page(bootstrap.address_space, bootstrap.layout.image_base, user_entry_page);
        paging_map_user_page(bootstrap.address_space, bootstrap.layout.stack_top - 4096u, user_stack_page);

        trampoline_entry = bootstrap.layout.trampoline_base +
            ((uintptr_t)(void *)arch_user_program_entry & (uintptr_t)4095u);
        user_entry = bootstrap.layout.image_base +
            ((uintptr_t)(void *)user_init_main & (uintptr_t)4095u);

        bootstrap.trampoline_entry = (void (*)(void))trampoline_entry;
        bootstrap.user_entry = (void (*)(void))user_entry;
        bootstrap.user_stack_page = user_stack_page;
        bootstrap.user_stack_top = bootstrap.layout.stack_top;
    }

    sched_enable_user_task(&bootstrap);
}

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

static void write_hex_u64(uint64_t value) {
    static const char digits[] = "0123456789abcdef";
    char output[18];
    size_t index;

    output[0] = '0';
    output[1] = 'x';

    for (index = 0u; index < 16u; ++index) {
        unsigned int shift;
        uint64_t nibble;

        shift = (unsigned int)((15u - index) * 4u);
        nibble = (value >> shift) & 0xFu;
        output[2u + index] = digits[(unsigned int)nibble];
    }

    serial_write(output, sizeof(output));
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

void kernel_trap_error(uint32_t vector, uint64_t error_code) {
    write_text("trap ");
    write_decimal_u32(vector);
    write_text(" ec ");
    write_decimal_u64(error_code);
    if (vector == 14u) {
        write_text(" cr2 ");
        write_hex_u64(arch_read_cr2());
    }
    write_text("\r\n");
}

uintptr_t kernel_timer_tick(uintptr_t current_rsp) {
    ++kernel_ticks;

    if ((kernel_ticks % 100u) == 0u &&
        sched_current_task_kind() == TASK_KERNEL &&
        sched_current_task_id() == 1u) {
        write_text("worker ");
        write_decimal_u64(sched_debug_worker_counter());
        write_text("\r\n");
    }

    pic_eoi(0u);
    return sched_tick(current_rsp);
}

uintptr_t kernel_syscall_entry(uintptr_t current_rsp) {
    struct syscall_frame *frame = (struct syscall_frame *)(void *)current_rsp;

    if (frame->rax == SYS_PUTC) {
        char ch = (char)(frame->rdi & 0xFFu);
        serial_write(&ch, 1u);
        frame->rax = 0u;
        return current_rsp;
    }

    if (frame->rax == SYS_YIELD) {
        frame->rax = 0u;
        return sched_tick(current_rsp);
    }

    if (frame->rax == SYS_GET_TICKS) {
	frame->rax = kernel_ticks;
	return current_rsp;
    }

    if (frame->rax == SYS_EXIT) {
        write_line("ring3 exit");
        frame->rax = 0u;
        return sched_exit_current(current_rsp);
    }

    frame->rax = (uint64_t)-1;
    return current_rsp;
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
    idt_set_syscall_gate();
    write_line("idt ok");

    palloc_initialize();
    palloc_self_test();

    {
        uintptr_t kernel_rsp0 = palloc_alloc();

        if (kernel_rsp0 == 0u) {
            write_line("tss stack fail");
            arch_halt_forever();
        }

        gdt_set_tss_rsp0((uint64_t)(kernel_rsp0 + 4096u));
        write_line("tss rsp0 ok");
    }

    paging_initialize();
    write_line("paging 4k ok");

    pic_initialize();
    write_line("pic ok");

    pit_initialize(100u);
    write_line("pit ok");

    if (USER_INIT_ENABLED) {
        configure_first_user_task();
    }

    sched_initialize();
    write_line("sched ok");

    if (USER_INIT_ENABLED) {
        write_line("user init");
    }

    arch_enable_interrupts();
    write_line("irq on");

    write_line("hello from /p/OS");

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
