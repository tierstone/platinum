#include "kernel/core.h"
#include "kernel/gdt.h"
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

static const efi_status_t efi_buffer_too_small = 0x8000000000000005ULL;

static uint8_t efi_memory_map_buffer[16384];

static size_t string_length(const char *text) {
    size_t length = 0u;
    while (text[length] != '\0') {
        ++length;
    }
    return length;
}

static void write_line(const char *text) {
    serial_write(text, string_length(text));
    serial_write("\r\n", 2u);
}

static int exit_boot_services_minimal(efi_handle_t image_handle, struct efi_system_table *system_table) {
    struct efi_boot_services *bs = system_table->boot_services;
    size_t size = sizeof(efi_memory_map_buffer);
    size_t key = 0u;
    size_t desc_size = 0u;
    uint32_t desc_ver = 0u;
    efi_status_t status;

    status = bs->get_memory_map(&size, efi_memory_map_buffer, &key, &desc_size, &desc_ver);
    if (status != efi_success) {
        return 0;
    }

    status = bs->exit_boot_services(image_handle, key);
    if (status == efi_success) {
        return 1;
    }

    size = sizeof(efi_memory_map_buffer);

    status = bs->get_memory_map(&size, efi_memory_map_buffer, &key, &desc_size, &desc_ver);
    if (status != efi_success) {
        return 0;
    }

    status = bs->exit_boot_services(image_handle, key);
    if (status != efi_success) {
        return 0;
    }

    return 1;
}

void kernel_main(void *image_handle, void *system_table) {
    struct efi_system_table *st = (struct efi_system_table *)system_table;

    serial_init();
    write_line("kernel_main");
    write_line("efi live");

    if (!exit_boot_services_minimal((efi_handle_t)image_handle, st)) {
        write_line("exit fail");
        arch_halt_forever();
    }

    write_line("exit ok");
    write_line("hello from /p/OS");

    gdt_initialize();
    write_line("gdt ok");

    arch_halt_forever();
}
