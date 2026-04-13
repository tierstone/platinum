#include "kernel/paging.h"
#include "kernel/core.h"

#include <stdint.h>
#include <stddef.h>

enum {
    paging_present = 0x001u,
    paging_writable = 0x002u,
    paging_large_page = 0x080u,
    paging_table_entries = 512u,
    paging_page_size_2m = 0x200000u,
    paging_pdpt_count = 4u
};

_Alignas(4096) static uint64_t paging_pml4[paging_table_entries];
_Alignas(4096) static uint64_t paging_pdpt[paging_table_entries];
_Alignas(4096) static uint64_t paging_pd[paging_pdpt_count][paging_table_entries];

void paging_initialize(void) {
    uint64_t flags_table = (uint64_t)(paging_present | paging_writable);
    uint64_t flags_page = (uint64_t)(paging_present | paging_writable | paging_large_page);
    size_t pdpt_index;
    size_t pd_index;

    for (pd_index = 0u; pd_index < paging_table_entries; ++pd_index) {
        paging_pml4[pd_index] = 0u;
        paging_pdpt[pd_index] = 0u;
    }

    for (pdpt_index = 0u; pdpt_index < paging_pdpt_count; ++pdpt_index) {
        for (pd_index = 0u; pd_index < paging_table_entries; ++pd_index) {
            paging_pd[pdpt_index][pd_index] = 0u;
        }
    }

    paging_pml4[0] = ((uint64_t)(uintptr_t)&paging_pdpt[0]) | flags_table;

    for (pdpt_index = 0u; pdpt_index < paging_pdpt_count; ++pdpt_index) {
        paging_pdpt[pdpt_index] = ((uint64_t)(uintptr_t)&paging_pd[pdpt_index][0]) | flags_table;

        for (pd_index = 0u; pd_index < paging_table_entries; ++pd_index) {
            uint64_t physical_base;

            physical_base = ((uint64_t)pdpt_index * (uint64_t)paging_table_entries + (uint64_t)pd_index) * (uint64_t)paging_page_size_2m;
            paging_pd[pdpt_index][pd_index] = physical_base | flags_page;
        }
    }

    arch_load_cr3((uint64_t)(uintptr_t)&paging_pml4[0]);
}

uintptr_t paging_reserved_begin(void) {
    return (uintptr_t)&paging_pml4[0];
}

uintptr_t paging_reserved_end(void) {
    return (uintptr_t)(&paging_pd[paging_pdpt_count][0]);
}
