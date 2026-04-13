#ifndef MEMMAP_H
#define MEMMAP_H

#include <stddef.h>
#include <stdint.h>

typedef uint64_t efi_status_t;
typedef void *efi_handle_t;

struct efi_system_table;

struct efi_memory_descriptor {
    uint32_t type;
    uint32_t pad;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
};

int memmap_capture_and_exit(efi_handle_t image_handle, struct efi_system_table *system_table);
void memmap_print_summary(void);
size_t memmap_get_descriptor_count(void);
size_t memmap_get_descriptor_size(void);
const struct efi_memory_descriptor *memmap_get_descriptor(size_t index);
uintptr_t memmap_reserved_begin(void);
uintptr_t memmap_reserved_end(void);
int memmap_find_largest_conventional_region(uintptr_t minimum_start, uintptr_t maximum_end, uintptr_t *region_begin, uintptr_t *region_end);

#endif
