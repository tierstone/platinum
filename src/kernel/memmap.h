#ifndef MEMMAP_H
#define MEMMAP_H

#include <stddef.h>
#include <stdint.h>

typedef uint64_t efi_status_t;
typedef void *efi_handle_t;

struct efi_system_table;

int memmap_capture_and_exit(efi_handle_t image_handle, struct efi_system_table *system_table);
void memmap_print_summary(void);

#endif
