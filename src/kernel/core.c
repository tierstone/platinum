#include "kernel/core.h"
#include "kernel/gdt.h"
#include "kernel/idt.h"
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
    size_t *, void *, size_t *, size_t *, uint32_t *
);

typedef efi_status_t (*efi_exit_boot_services_t)(
    efi_handle_t, size_t
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

enum { efi_success = 0 };

static uint8_t memmap[16384];

static size_t strlen2(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static void print(const char *s) {
    serial_write(s, strlen2(s));
}

static void println(const char *s) {
    print(s);
    serial_write("\r\n", 2);
}

static int exit_bs(efi_handle_t h, struct efi_system_table *st) {
    struct efi_boot_services *bs = st->boot_services;
    size_t sz = sizeof(memmap), key, ds;
    uint32_t dv;
    if (bs->get_memory_map(&sz, memmap, &key, &ds, &dv) != efi_success) return 0;
    if (bs->exit_boot_services(h, key) == efi_success) return 1;
    sz = sizeof(memmap);
    if (bs->get_memory_map(&sz, memmap, &key, &ds, &dv) != efi_success) return 0;
    return bs->exit_boot_services(h, key) == efi_success;
}

void kernel_trap(uint32_t v) {
    char buf[10];
    size_t i = 0;

    if (v == 0) {
        serial_write("trap 0\r\n", 8);
        return;
    }

    while (v) {
        buf[i++] = (char)('0' + (v % 10));
        v /= 10;
    }

    serial_write("trap ", 5);

    while (i) {
        serial_write(&buf[--i], 1);
    }

    serial_write("\r\n", 2);
}

void kernel_main(void *image_handle, void *system_table) {
    struct efi_system_table *st = (struct efi_system_table *)system_table;

    serial_init();
    println("kernel_main");
    println("efi live");

    if (!exit_bs((efi_handle_t)image_handle, st)) {
        println("exit fail");
        arch_halt_forever();
    }

    println("exit ok");

    gdt_initialize();
    println("gdt ok");

    idt_initialize();
    println("idt ok");

    __asm__ __volatile__("ud2");

    arch_halt_forever();
}
