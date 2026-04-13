#include "kernel/paging.h"
#include "kernel/core.h"
#include "kernel/memmap.h"
#include "kernel/palloc.h"

#include <stddef.h>
#include <stdint.h>

static const uint64_t paging_present = 0x001ull;
static const uint64_t paging_writable = 0x002ull;
static const uint64_t paging_flags_table = 0x003ull;
static const uint64_t paging_flags_page = 0x003ull;
static const uint64_t paging_page_size = 4096ull;
static const uint64_t paging_max_address = 0x100000000ull;

static uint64_t *paging_pml4;

static void zero_page(uint64_t *page) {
    size_t index;

    for (index = 0u; index < 512u; ++index) {
        page[index] = 0u;
    }
}

static uint64_t *alloc_table(void) {
    uintptr_t page = palloc_alloc();
    uint64_t *table;

    if (page == 0u) {
        return NULL;
    }

    table = (uint64_t *)(void *)page;
    zero_page(table);
    return table;
}

static uint64_t *entry_table(uint64_t entry) {
    return (uint64_t *)(void *)(uintptr_t)(entry & 0x000FFFFFFFFFF000ull);
}

static uint64_t *get_or_create_next(uint64_t *table, size_t index) {
    uint64_t *next;

    if ((table[index] & paging_present) != 0u) {
        return entry_table(table[index]);
    }

    next = alloc_table();
    if (next == NULL) {
        return NULL;
    }

    table[index] = ((uint64_t)(uintptr_t)next) | paging_flags_table;
    return next;
}

static int map_page_4k(uintptr_t virtual_address, uintptr_t physical_address) {
    size_t pml4_index;
    size_t pdpt_index;
    size_t pd_index;
    size_t pt_index;
    uint64_t *pdpt;
    uint64_t *pd;
    uint64_t *pt;

    pml4_index = (size_t)((virtual_address >> 39) & 0x1FFu);
    pdpt_index = (size_t)((virtual_address >> 30) & 0x1FFu);
    pd_index = (size_t)((virtual_address >> 21) & 0x1FFu);
    pt_index = (size_t)((virtual_address >> 12) & 0x1FFu);

    pdpt = get_or_create_next(paging_pml4, pml4_index);
    if (pdpt == NULL) {
        return 0;
    }

    pd = get_or_create_next(pdpt, pdpt_index);
    if (pd == NULL) {
        return 0;
    }

    pt = get_or_create_next(pd, pd_index);
    if (pt == NULL) {
        return 0;
    }

    pt[pt_index] = ((uint64_t)physical_address) | paging_flags_page;
    return 1;
}

static uintptr_t highest_physical_end(void) {
    uintptr_t highest = 0u;
    size_t index;

    for (index = 0u; index < memmap_get_descriptor_count(); ++index) {
        const struct efi_memory_descriptor *descriptor;
        uint64_t end64;
        uintptr_t end;

        descriptor = memmap_get_descriptor(index);
        if (descriptor == NULL) {
            continue;
        }

        end64 = descriptor->physical_start + descriptor->number_of_pages * paging_page_size;
        if (end64 > paging_max_address) {
            end64 = paging_max_address;
        }

        end = (uintptr_t)end64;
        if (end > highest) {
            highest = end;
        }
    }

    if (highest == 0u) {
        return 0u;
    }

    highest = (highest + (uintptr_t)(paging_page_size - 1u)) & ~(uintptr_t)(paging_page_size - 1u);
    return highest;
}

void paging_initialize(void) {
    uintptr_t limit;
    uintptr_t address;

    paging_pml4 = alloc_table();
    if (paging_pml4 == NULL) {
        arch_halt_forever();
    }

    limit = highest_physical_end();
    if (limit == 0u) {
        arch_halt_forever();
    }

    address = 0u;
    while (address < limit) {
        if (!map_page_4k(address, address)) {
            arch_halt_forever();
        }

        address += (uintptr_t)paging_page_size;
    }

    arch_load_cr3((uint64_t)(uintptr_t)paging_pml4);
}
