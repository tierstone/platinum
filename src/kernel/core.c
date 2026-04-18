#include "kernel/core.h"
#include "kernel/elf.h"
#include "kernel/vfs.h"
#include "kernel/fd.h"
#include "kernel/gdt.h"
#include "kernel/heap.h"
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

#ifndef USER_TEST_BAD_ELF
#define USER_TEST_BAD_ELF 0
#endif

void user_init_main(void);
extern const uint8_t embedded_user_program_elf[];
extern const size_t embedded_user_program_elf_size;
static void write_line(const char *text);

static void configure_first_user_task(void) {
    struct user_task_bootstrap bootstrap;

    bootstrap.layout = first_user_layout;
    bootstrap.address_space = paging_create_user_address_space();
    bootstrap.trampoline_entry = 0;
    bootstrap.user_entry = 0;
    bootstrap.image_physical_begin = 0u;
    bootstrap.image_physical_end = 0u;
    bootstrap.user_stack_page = 0u;
    bootstrap.user_stack_top = 0u;
    if (bootstrap.address_space == 0u || !paging_address_space_valid(bootstrap.address_space)) {
        write_line("user as fail");
        arch_halt_forever();
    }
    if (!paging_mapping_is_supervisor_only(bootstrap.address_space, 0u)) {
        write_line("user as fail");
        arch_halt_forever();
    }

    if (USER_INIT_USE_ELF) {
        struct loaded_user_image loaded_image;
        uintptr_t trampoline_page;
        uintptr_t trampoline_entry;
        const uint8_t *user_image;
        size_t user_image_size;

        if (USER_TEST_BAD_ELF) {
            static const uint8_t bad_user_image[] = { 0x00u, 0x45u, 0x4cu, 0x46u };
            user_image = bad_user_image;
            user_image_size = sizeof(bad_user_image);
        } else {
            user_image = embedded_user_program_elf;
            user_image_size = embedded_user_program_elf_size;
        }

        if (!elf_load_user_image(
                user_image,
                user_image_size,
                &bootstrap.layout,
                bootstrap.address_space,
                0u,
                0u,
                &loaded_image)) {
            write_line("user elf fail");
            arch_halt_forever();
        }

        trampoline_page = (uintptr_t)(void *)arch_user_program_entry & ~(uintptr_t)4095u;
        paging_map_user_page(bootstrap.address_space, bootstrap.layout.trampoline_base, trampoline_page);
        if (!paging_mapping_matches(bootstrap.address_space, bootstrap.layout.trampoline_base, trampoline_page, 1) ||
            !paging_mapping_is_supervisor_only(bootstrap.address_space, trampoline_page) ||
            !paging_mapping_is_supervisor_only(bootstrap.address_space, loaded_image.load_physical_begin) ||
            !paging_mapping_is_supervisor_only(bootstrap.address_space, loaded_image.load_physical_end - 4096u) ||
            !paging_mapping_is_supervisor_only(bootstrap.address_space, loaded_image.stack_page)) {
            write_line("user elf fail");
            arch_halt_forever();
        }
        trampoline_entry = bootstrap.layout.trampoline_base +
            ((uintptr_t)(void *)arch_user_program_entry & (uintptr_t)4095u);

        bootstrap.trampoline_entry = (void (*)(void))trampoline_entry;
        bootstrap.user_entry = loaded_image.entry;
        bootstrap.image_physical_begin = loaded_image.load_physical_begin;
        bootstrap.image_physical_end = loaded_image.load_physical_end;
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
        if (!paging_mapping_matches(bootstrap.address_space, bootstrap.layout.trampoline_base, trampoline_page, 1) ||
            !paging_mapping_matches(bootstrap.address_space, bootstrap.layout.image_base, user_entry_page, 1) ||
            !paging_mapping_matches(bootstrap.address_space, bootstrap.layout.stack_top - 4096u, user_stack_page, 1) ||
            !paging_mapping_is_supervisor_only(bootstrap.address_space, trampoline_page) ||
            !paging_mapping_is_supervisor_only(bootstrap.address_space, user_entry_page) ||
            !paging_mapping_is_supervisor_only(bootstrap.address_space, user_stack_page)) {
            write_line("user map fail");
            arch_halt_forever();
        }

        trampoline_entry = bootstrap.layout.trampoline_base +
            ((uintptr_t)(void *)arch_user_program_entry & (uintptr_t)4095u);
        user_entry = bootstrap.layout.image_base +
            ((uintptr_t)(void *)user_init_main & (uintptr_t)4095u);

        bootstrap.trampoline_entry = (void (*)(void))trampoline_entry;
        bootstrap.user_entry = (void (*)(void))user_entry;
        bootstrap.image_physical_begin = 0u;
        bootstrap.image_physical_end = 0u;
        bootstrap.user_stack_page = user_stack_page;
        bootstrap.user_stack_top = bootstrap.layout.stack_top;
    }

#ifdef USER_TEST_BAD_BOOTSTRAP
    bootstrap.trampoline_entry = 0;
#endif

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

static void syscall_return(struct syscall_frame *frame, uint64_t result)
{
    frame->rax = result;
}

static int syscall_buffer_valid(const void *buffer, size_t count)
{
    if (count == 0u) {
        return 1;
    }

    return buffer != 0;
}

static void syscall_clear_bootstrap(struct user_task_bootstrap *bootstrap)
{
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

static int syscall_user_range_in_layout(
    const struct user_virtual_layout *layout,
    uintptr_t start,
    size_t count
)
{
    uintptr_t end;

    if (layout == 0 || start < layout->trampoline_base || start >= layout->stack_top) {
        return 0;
    }

    if (count == 0u) {
        return 1;
    }

    end = start + (uintptr_t)(count - 1u);
    if (end < start || end >= layout->stack_top) {
        return 0;
    }

    return 1;
}

static int syscall_user_buffer_valid(
    uintptr_t address_space,
    const struct user_virtual_layout *layout,
    const void *buffer,
    size_t count
)
{
    uintptr_t start;

    /* Proof-stage contract: validate user virtual range and mapped user pages,
     * then dereference directly. This is not a fault-contained copyin/copyout path.
     */
    if (!syscall_buffer_valid(buffer, count)) {
        return 0;
    }

    if (count == 0u) {
        return 1;
    }

    start = (uintptr_t)buffer;
    if (!syscall_user_range_in_layout(layout, start, count)) {
        return 0;
    }

    return paging_user_range_mapped(address_space, start, count);
}

static int syscall_copy_path(
    char *dst,
    size_t dst_size,
    const char *src,
    uintptr_t address_space,
    const struct user_virtual_layout *layout
)
{
    size_t index;
    uintptr_t address;

    /* Same proof-stage contract as syscall_user_buffer_valid(): validate first,
     * then copy by direct dereference.
     */
    if (dst == 0 || dst_size == 0u || src == 0) {
        return 0;
    }

    for (index = 0u; index + 1u < dst_size; ++index) {
        address = (uintptr_t)src + index;
        if (address < (uintptr_t)src ||
            !syscall_user_range_in_layout(layout, address, 1u) ||
            !paging_user_range_mapped(address_space, address, 1u)) {
            return 0;
        }

        dst[index] = src[index];
        if (index == 0u && dst[index] != '/') {
            return 0;
        }
        if (src[index] == '\0') {
            return 1;
        }
    }

    return 0;
}

static void syscall_release_bootstrap(struct user_task_bootstrap *bootstrap)
{
    struct loaded_user_image loaded_image;

    if (bootstrap == 0) {
        return;
    }

    loaded_image.load_begin = 0u;
    loaded_image.load_end = 0u;
    loaded_image.load_physical_begin = bootstrap->image_physical_begin;
    loaded_image.load_physical_end = bootstrap->image_physical_end;
    loaded_image.reusable_physical_begin = 0u;
    loaded_image.reusable_physical_end = 0u;
    loaded_image.entry = 0;
    loaded_image.stack_page = bootstrap->user_stack_page;
    loaded_image.stack_top = bootstrap->user_stack_top;

    elf_release_user_image(&loaded_image);
    paging_release_user_address_space(bootstrap->address_space, &bootstrap->layout);
    syscall_clear_bootstrap(bootstrap);
}

static int syscall_prepare_exec_bootstrap(
    const uint8_t *image,
    size_t image_size,
    const struct user_task_bootstrap *current_bootstrap,
    struct user_task_bootstrap *bootstrap
)
{
    struct loaded_user_image loaded_image;
    uintptr_t address_space;
    uintptr_t trampoline_page;
    uintptr_t trampoline_entry;

    /* Ownership stays with caller until sched_exec_current() succeeds. */
    syscall_clear_bootstrap(bootstrap);
    loaded_image.load_begin = 0u;
    loaded_image.load_end = 0u;
    loaded_image.load_physical_begin = 0u;
    loaded_image.load_physical_end = 0u;
    loaded_image.reusable_physical_begin = 0u;
    loaded_image.reusable_physical_end = 0u;
    loaded_image.entry = 0;
    loaded_image.stack_page = 0u;
    loaded_image.stack_top = 0u;

    bootstrap->layout = first_user_layout;
    address_space = paging_create_user_address_space();
    if (address_space == 0u || !paging_address_space_valid(address_space)) {
        return 0;
    }

    if (!elf_load_user_image(
            image,
            image_size,
            &bootstrap->layout,
            address_space,
            current_bootstrap == 0 ? 0u : current_bootstrap->image_physical_begin,
            current_bootstrap == 0 ? 0u : current_bootstrap->image_physical_end,
            &loaded_image)) {
        paging_release_user_address_space(address_space, &bootstrap->layout);
        return 0;
    }

    trampoline_page = (uintptr_t)(void *)arch_user_program_entry & ~(uintptr_t)4095u;
    paging_map_user_page(address_space, bootstrap->layout.trampoline_base, trampoline_page);
    if (!paging_mapping_matches(address_space, bootstrap->layout.trampoline_base, trampoline_page, 1)) {
        elf_release_user_image(&loaded_image);
        paging_release_user_address_space(address_space, &bootstrap->layout);
        return 0;
    }

    trampoline_entry = bootstrap->layout.trampoline_base +
        ((uintptr_t)(void *)arch_user_program_entry & (uintptr_t)4095u);

    bootstrap->address_space = address_space;
    bootstrap->trampoline_entry = (void (*)(void))trampoline_entry;
    bootstrap->user_entry = loaded_image.entry;
    bootstrap->image_physical_begin = loaded_image.load_physical_begin;
    bootstrap->image_physical_end = loaded_image.load_physical_end;
    bootstrap->user_stack_page = loaded_image.stack_page;
    bootstrap->user_stack_top = loaded_image.stack_top;
    return 1;
}

uintptr_t kernel_syscall_entry(uintptr_t current_rsp) {
    struct syscall_frame *frame = (struct syscall_frame *)(void *)current_rsp;
    struct fd_table *fd_table;

    fd_table = sched_current_fd_table();

    switch (frame->rax) {
    case SYS_PUTC: {
        char ch;

        ch = (char)(frame->rdi & 0xFFu);
        serial_write(&ch, 1u);
        syscall_return(frame, SYS_RESULT_OK);
        return current_rsp;
    }
    case SYS_YIELD:
        syscall_return(frame, SYS_RESULT_OK);
        return sched_tick(current_rsp);
    case SYS_GET_TICKS:
        syscall_return(frame, kernel_ticks);
        return current_rsp;
    case SYS_EXIT:
        syscall_return(frame, SYS_RESULT_OK);
        return sched_exit_current(current_rsp);
    case SYS_READ:
    case SYS_WRITE:
    case SYS_OPEN:
    case SYS_EXEC: {
        struct user_virtual_layout user_layout;
        uintptr_t user_address_space;

        if (!sched_current_user_context(&user_layout, &user_address_space)) {
            syscall_return(frame, SYS_RESULT_ERROR);
            return current_rsp;
        }

        switch (frame->rax) {
        case SYS_READ:
        if (fd_table == 0 ||
            !syscall_user_buffer_valid(
                user_address_space,
                &user_layout,
                (void *)(uintptr_t)frame->rsi,
                (size_t)frame->rdx
            )) {
            syscall_return(frame, SYS_RESULT_ERROR);
            return current_rsp;
        }
        syscall_return(
            frame,
            (uint64_t)(int64_t)fd_table_read(
                fd_table,
                (int)frame->rdi,
                (void *)(uintptr_t)frame->rsi,
                (size_t)frame->rdx
            )
        );
        return current_rsp;
        case SYS_WRITE:
        if (fd_table == 0 ||
            !syscall_user_buffer_valid(
                user_address_space,
                &user_layout,
                (const void *)(uintptr_t)frame->rsi,
                (size_t)frame->rdx
            )) {
            syscall_return(frame, SYS_RESULT_ERROR);
            return current_rsp;
        }
        syscall_return(
            frame,
            (uint64_t)(int64_t)fd_table_write(
                fd_table,
                (int)frame->rdi,
                (const void *)(uintptr_t)frame->rsi,
                (size_t)frame->rdx
            )
        );
        return current_rsp;
        case SYS_OPEN: {
        char path[64];
        struct vfs_file *file;
        fd_kind_t kind;
        int fd;

        if (fd_table == 0 ||
            !syscall_copy_path(
                path,
                sizeof(path),
                (const char *)(uintptr_t)frame->rdi,
                user_address_space,
                &user_layout
            )) {
            syscall_return(frame, SYS_RESULT_ERROR);
            return current_rsp;
        }

        file = vfs_open_path(path, (uint32_t)frame->rsi);
        if (file == 0) {
            syscall_return(frame, SYS_RESULT_ERROR);
            return current_rsp;
        }

        kind = file->node->kind == VFS_NODE_CONSOLE ? FD_KIND_CONSOLE : FD_KIND_PLACEHOLDER;
        fd = fd_table_install(fd_table, kind, file);
        if (fd < 0) {
            (void)vfs_file_close(file);
            syscall_return(frame, SYS_RESULT_ERROR);
            return current_rsp;
        }

        syscall_return(frame, (uint64_t)(int64_t)fd);
        return current_rsp;
        }
        case SYS_EXEC: {
        char path[64];
        struct vfs_file *file;
        const uint8_t *image;
        size_t image_size;
        struct user_task_bootstrap bootstrap;
        struct user_task_bootstrap current_bootstrap;
        uintptr_t next_rsp;

        if (!syscall_copy_path(
                path,
                sizeof(path),
                (const char *)(uintptr_t)frame->rdi,
                user_address_space,
                &user_layout)) {
            syscall_return(frame, SYS_RESULT_ERROR);
            return current_rsp;
        }

        file = vfs_open_exec_path(path);
        if (file == 0) {
            syscall_return(frame, SYS_RESULT_ERROR);
            return current_rsp;
        }

        if (vfs_file_executable_image(file, &image, &image_size) != 0 || image == 0 || image_size == 0u) {
            (void)vfs_file_close(file);
            syscall_return(frame, SYS_RESULT_ERROR);
            return current_rsp;
        }

        if (vfs_file_close(file) != 0 ||
            !sched_current_user_bootstrap(&current_bootstrap) ||
            !syscall_prepare_exec_bootstrap(image, image_size, &current_bootstrap, &bootstrap)) {
            syscall_return(frame, SYS_RESULT_ERROR);
            return current_rsp;
        }

#ifdef USER_TEST_EXEC_TRANSFER_FAIL
        bootstrap.trampoline_entry = 0;
#endif

        next_rsp = sched_exec_current(&bootstrap);
        if (next_rsp == 0u) {
            syscall_release_bootstrap(&bootstrap);
            syscall_return(frame, SYS_RESULT_ERROR);
            return current_rsp;
        }

        return next_rsp;
        }
        default:
            syscall_return(frame, SYS_RESULT_ERROR);
            return current_rsp;
        }
    }
    case SYS_CLOSE:
        if (fd_table == 0) {
            syscall_return(frame, SYS_RESULT_ERROR);
            return current_rsp;
        }
        syscall_return(frame, (uint64_t)(int64_t)fd_table_close(fd_table, (int)frame->rdi));
        return current_rsp;
    case SYS_DUP:
        if (fd_table == 0) {
            syscall_return(frame, SYS_RESULT_ERROR);
            return current_rsp;
        }
        syscall_return(frame, (uint64_t)(int64_t)fd_table_dup(fd_table, (int)frame->rdi));
        return current_rsp;
    default:
        syscall_return(frame, SYS_RESULT_ERROR);
        return current_rsp;
    }
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

    heap_initialize();
    heap_self_test();

    vfs_namespace_initialize();
    vfs_self_test();
    fd_self_test();

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
