#include "kernel/memmap.h"
#include "drivers/serial.h"

#include <stddef.h>
#include <stdint.h>

struct efi_table_header {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
};

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

struct efi_memory_descriptor {
    uint32_t type;
    uint32_t pad;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
};

enum {
    efi_success = 0,
    efi_conventional_memory = 7
};

static uint8_t memory_map_buffer[16384];
static size_t memory_map_size_used;
static size_t memory_map_descriptor_size;
static uint32_t memory_map_descriptor_version;
static size_t memory_map_descriptor_count;
static size_t memory_map_conventional_region_count;
static uint64_t memory_map_conventional_page_count;

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

static void memmap_parse(size_t memory_map_size, size_t descriptor_size, uint32_t descriptor_version) {
    size_t offset;

    memory_map_size_used = memory_map_size;
    memory_map_descriptor_size = descriptor_size;
    memory_map_descriptor_version = descriptor_version;
    memory_map_descriptor_count = 0u;
    memory_map_conventional_region_count = 0u;
    memory_map_conventional_page_count = 0u;

    if (descriptor_size == 0u) {
        return;
    }

    memory_map_descriptor_count = memory_map_size / descriptor_size;

    for (offset = 0u; offset + descriptor_size <= memory_map_size; offset += descriptor_size) {
        const struct efi_memory_descriptor *descriptor;

        descriptor = (const struct efi_memory_descriptor *)(const void *)&memory_map_buffer[offset];

        if (descriptor->type == efi_conventional_memory) {
            ++memory_map_conventional_region_count;
            memory_map_conventional_page_count += descriptor->number_of_pages;
        }
    }
}

int memmap_capture_and_exit(efi_handle_t image_handle, struct efi_system_table *system_table) {
    struct efi_boot_services *boot_services = system_table->boot_services;
    size_t memory_map_size = sizeof(memory_map_buffer);
    size_t map_key = 0u;
    size_t descriptor_size = 0u;
    uint32_t descriptor_version = 0u;
    efi_status_t status;

    status = boot_services->get_memory_map(
        &memory_map_size,
        &memory_map_buffer[0],
        &map_key,
        &descriptor_size,
        &descriptor_version
    );
    if (status != efi_success) {
        return 0;
    }

    memmap_parse(memory_map_size, descriptor_size, descriptor_version);

    status = boot_services->exit_boot_services(image_handle, map_key);
    if (status == efi_success) {
        return 1;
    }

    memory_map_size = sizeof(memory_map_buffer);
    map_key = 0u;
    descriptor_size = 0u;
    descriptor_version = 0u;

    status = boot_services->get_memory_map(
        &memory_map_size,
        &memory_map_buffer[0],
        &map_key,
        &descriptor_size,
        &descriptor_version
    );
    if (status != efi_success) {
        return 0;
    }

    memmap_parse(memory_map_size, descriptor_size, descriptor_version);

    status = boot_services->exit_boot_services(image_handle, map_key);
    if (status != efi_success) {
        return 0;
    }

    return 1;
}

void memmap_print_summary(void) {
    write_line("memmap ok");

    write_text("mem desc ");
    write_decimal_u64((uint64_t)memory_map_descriptor_count);
    write_text("\r\n");

    write_text("mem conv regions ");
    write_decimal_u64((uint64_t)memory_map_conventional_region_count);
    write_text("\r\n");

    write_text("mem conv pages ");
    write_decimal_u64(memory_map_conventional_page_count);
    write_text("\r\n");

    write_text("mem desc size ");
    write_decimal_u64((uint64_t)memory_map_descriptor_size);
    write_text("\r\n");

    write_text("mem desc ver ");
    write_decimal_u64((uint64_t)memory_map_descriptor_version);
    write_text("\r\n");

    write_text("mem bytes ");
    write_decimal_u64((uint64_t)memory_map_size_used);
    write_text("\r\n");
}
