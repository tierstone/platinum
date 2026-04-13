#include "kernel.h"
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

struct efi_input_key {
    uint16_t scan_code;
    uint16_t unicode_char;
};

struct efi_simple_text_input_protocol;
struct efi_simple_text_output_protocol;
struct efi_runtime_services;
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
    void *get_next_monotonic_count;
    void *stall;
    void *set_watchdog_timer;
    void *connect_controller;
    void *disconnect_controller;
    void *open_protocol;
    void *close_protocol;
    void *open_protocol_information;
    void *protocols_per_handle;
    void *locate_handle_buffer;
    void *locate_protocol;
    void *install_multiple_protocol_interfaces;
    void *uninstall_multiple_protocol_interfaces;
    void *calculate_crc32;
    void *copy_mem;
    void *set_mem;
    void *create_event_ex;
};

struct efi_system_table {
    struct efi_table_header header;
    uint16_t *firmware_vendor;
    uint32_t firmware_revision;
    efi_handle_t console_in_handle;
    struct efi_simple_text_input_protocol *con_in;
    efi_handle_t console_out_handle;
    struct efi_simple_text_output_protocol *con_out;
    efi_handle_t standard_error_handle;
    struct efi_simple_text_output_protocol *std_err;
    struct efi_runtime_services *runtime_services;
    struct efi_boot_services *boot_services;
    size_t number_of_table_entries;
    void *configuration_table;
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

static void write_text(const char *text) {
    serial_write(text, string_length(text));
}

static void write_line(const char *text) {
    write_text(text);
    serial_write("\r\n", 2u);
}

static int exit_boot_services_minimal(efi_handle_t image_handle, struct efi_system_table *system_table) {
    struct efi_boot_services *boot_services = system_table->boot_services;
    size_t memory_map_size = sizeof(efi_memory_map_buffer);
    size_t map_key = 0u;
    size_t descriptor_size = 0u;
    uint32_t descriptor_version = 0u;
    efi_status_t status;

    status = boot_services->get_memory_map(
        &memory_map_size,
        &efi_memory_map_buffer[0],
        &map_key,
        &descriptor_size,
        &descriptor_version
    );

    if (status == efi_buffer_too_small) {
        return 0;
    }

    if (status != efi_success) {
        return 0;
    }

    status = boot_services->exit_boot_services(image_handle, map_key);
    if (status == efi_success) {
        return 1;
    }

    memory_map_size = sizeof(efi_memory_map_buffer);
    map_key = 0u;
    descriptor_size = 0u;
    descriptor_version = 0u;

    status = boot_services->get_memory_map(
        &memory_map_size,
        &efi_memory_map_buffer[0],
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

void kernel_main(void *image_handle, void *system_table) {
    struct efi_system_table *efi_system_table = (struct efi_system_table *)system_table;

    serial_init();
    write_line("kernel_main");
    write_line("efi live");

    if (!exit_boot_services_minimal((efi_handle_t)image_handle, efi_system_table)) {
        write_line("exit fail");
        arch_halt_forever();
    }

    write_line("exit ok");
    write_line("hello from /p/OS");

    arch_halt_forever();
}
