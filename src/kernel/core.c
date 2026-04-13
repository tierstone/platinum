#include "kernel/core.h"
#include "kernel/gdt.h"
#include "kernel/idt.h"
#include "kernel/paging.h"
#include "drivers/serial.h"

#include <stddef.h>
#include <stdint.h>

typedef uint64_t efi_status_t;
typedef void *efi_handle_t;

struct efi_table_header {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
};

struct efi_boot_services;

typedef efi_status_t (*efi_get_memory_map_t)(
    size_t *memory_map_size,
    void *memory_map,
    size_t *map_key,
    size_t *descriptor_size,
    uint32_t *descriptor_version
);

typedef efi_status_t (*efi_exit_boot_services_t)(
    efi_handle_t image_handle,
    size_t map_key
);

struct efi_boot_services {
    struct efi_table_header header;
    void *raise_tpl;
    void *restore_tpl;
    void *allocate_pages;
    void *free_pages;
    efi_get_memory_map_t get_memory_map;
    void *allocate_pool;
    void *free_pool;
    void *create_event;
    void *set_timer;
    void *wait_for_event;
    void *signal_event;
    void *close_event;
    void *check_event;
    void *install_protocol_interface;
    void *reinstall_protocol_interface;
    void *uninstall_protocol_interface;
    void *handle_protocol;
    void *reserved;
    void *register_protocol_notify;
    void *locate_handle;
    void *locate_device_path;
    void *install_configuration_table;
    void *load_image;
    void *start_image;
    void *exit;
    void *unload_image;
    efi_exit_boot_services_t exit_boot_services;
};

struct efi_system_table {
    struct efi_table_header header;
    uint16_t *firmware_vendor;
    uint32_t firmware_revision;
    void *console_in_handle;
    void *con_in;
    void *console_out_handle;
    void *con_out;
    void *standard_error_handle;
    void *std_err;
    void *runtime_services;
    struct efi_boot_services *boot_services;
};

enum {
    efi_success = 0
};

static uint8_t memmap[16384];

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

static int exit_bs(efi_handle_t image_handle, struct efi_system_table *system_table) {
    struct efi_boot_services *boot_services = system_table->boot_services;
    size_t memory_map_size = sizeof(memmap);
    size_t map_key = 0u;
    size_t descriptor_size = 0u;
    uint32_t descriptor_version = 0u;
    efi_status_t status;

    status = boot_services->get_memory_map(
        &memory_map_size,
        &memmap[0],
        &map_key,
        &descriptor_size,
        &descriptor_version
    );
    if (status != efi_success) {
        return 0;
    }

    status = boot_services->exit_boot_services(image_handle, map_key);
    if (status == efi_success) {
        return 1;
    }

    memory_map_size = sizeof(memmap);
    map_key = 0u;
    descriptor_size = 0u;
    descriptor_version = 0u;

    status = boot_services->get_memory_map(
        &memory_map_size,
        &memmap[0],
        &map_key,
        &descriptor_size,
        &descriptor_version
    );
    if (status != efi_success) {
        return 0;
    }

    status = boot_services->exit_boot_services(image_handle, map_key);
    if (status != efi_success) {
        return 0;
    }

    return 1;
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

    if (!exit_bs((efi_handle_t)image_handle, system)) {
        write_line("exit fail");
        arch_halt_forever();
    }

    write_line("exit ok");

    gdt_initialize();
    write_line("gdt ok");

    idt_initialize();
    write_line("idt ok");

    paging_initialize();
    write_line("paging ok");

    write_line("hello from /p/OS");

    arch_halt_forever();
}
