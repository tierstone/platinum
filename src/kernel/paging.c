#include "kernel/paging.h"
#include "kernel/core.h"
#include "kernel/memmap.h"
#include "kernel/palloc.h"
#include "kernel/sched.h"

#include <stddef.h>
#include <stdint.h>

static const uint64_t paging_present = 0x001ull;
static const uint64_t paging_writable = 0x002ull;
static const uint64_t paging_user = 0x004ull;
static const uint64_t paging_flags_table = 0x003ull;
static const uint64_t paging_flags_page = 0x003ull;
static const uint64_t paging_page_size = 4096ull;
static const uint64_t paging_max_address = 0x100000000ull;
static const uint64_t paging_address_mask = 0x000FFFFFFFFFF000ull;

static uint64_t *paging_kernel_pml4;

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

static uint64_t *copy_table(const uint64_t *src) {
    uint64_t *table;
    size_t index;

    table = alloc_table();
    if (table == NULL) {
        return NULL;
    }

    for (index = 0u; index < 512u; ++index) {
        table[index] = src[index];
    }

    return table;
}

static uint64_t *entry_table(uint64_t entry) {
    return (uint64_t *)(void *)(uintptr_t)(entry & paging_address_mask);
}

static uintptr_t entry_address(uint64_t entry) {
    return (uintptr_t)(entry & paging_address_mask);
}

static uint64_t *get_or_create_next(uint64_t *table, size_t index, uint64_t flags) {
    uint64_t *next;

    if ((table[index] & paging_present) != 0u) {
        if ((flags & paging_user) != 0u) {
            table[index] |= paging_user;
        }
        return entry_table(table[index]);
    }

    next = alloc_table();
    if (next == NULL) {
        return NULL;
    }

    table[index] = ((uint64_t)(uintptr_t)next) | paging_flags_table | (flags & paging_user);
    return next;
}

static int map_page_4k(uint64_t *pml4, uintptr_t virtual_address, uintptr_t physical_address, uint64_t flags) {
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

    pdpt = get_or_create_next(pml4, pml4_index, flags);
    if (pdpt == NULL) {
        return 0;
    }

    pd = get_or_create_next(pdpt, pdpt_index, flags);
    if (pd == NULL) {
        return 0;
    }

    pt = get_or_create_next(pd, pd_index, flags);
    if (pt == NULL) {
        return 0;
    }

    pt[pt_index] = ((uint64_t)physical_address) | flags;
    return 1;
}

static uint64_t *walk_to_pt(uint64_t *pml4, uintptr_t virtual_address) {
    size_t pml4_index;
    size_t pdpt_index;
    size_t pd_index;
    uint64_t *pdpt;
    uint64_t *pd;

    pml4_index = (size_t)((virtual_address >> 39) & 0x1FFu);
    pdpt_index = (size_t)((virtual_address >> 30) & 0x1FFu);
    pd_index = (size_t)((virtual_address >> 21) & 0x1FFu);

    if ((pml4[pml4_index] & paging_present) == 0u) {
        return NULL;
    }

    pdpt = entry_table(pml4[pml4_index]);
    if ((pdpt[pdpt_index] & paging_present) == 0u) {
        return NULL;
    }

    pd = entry_table(pdpt[pdpt_index]);
    if ((pd[pd_index] & paging_present) == 0u) {
        return NULL;
    }

    return entry_table(pd[pd_index]);
}

static int ensure_private_user_path(uint64_t *pml4, uintptr_t virtual_address) {
    size_t pml4_index;
    size_t pdpt_index;
    size_t pd_index;
    uint64_t kernel_pml4_entry;
    uint64_t *kernel_pdpt;
    uint64_t *pdpt;
    uint64_t kernel_pdpt_entry;
    uint64_t *kernel_pd;
    uint64_t *pd;
    uint64_t kernel_pd_entry;
    uint64_t *clone;

    pml4_index = (size_t)((virtual_address >> 39) & 0x1FFu);
    pdpt_index = (size_t)((virtual_address >> 30) & 0x1FFu);
    pd_index = (size_t)((virtual_address >> 21) & 0x1FFu);

    kernel_pml4_entry = paging_kernel_pml4[pml4_index];
    if ((pml4[pml4_index] & paging_present) != 0u &&
        entry_address(pml4[pml4_index]) == entry_address(kernel_pml4_entry)) {
        clone = copy_table(entry_table(pml4[pml4_index]));
        if (clone == NULL) {
            return 0;
        }
        pml4[pml4_index] = ((uint64_t)(uintptr_t)clone) | paging_flags_table | paging_user;
    } else if ((pml4[pml4_index] & paging_present) != 0u) {
        pml4[pml4_index] |= paging_user;
    }

    pdpt = entry_table(pml4[pml4_index]);
    kernel_pdpt = (kernel_pml4_entry & paging_present) == 0u ? NULL : entry_table(kernel_pml4_entry);
    kernel_pdpt_entry = kernel_pdpt == NULL ? 0u : kernel_pdpt[pdpt_index];
    if ((pdpt[pdpt_index] & paging_present) != 0u &&
        entry_address(pdpt[pdpt_index]) == entry_address(kernel_pdpt_entry)) {
        clone = copy_table(entry_table(pdpt[pdpt_index]));
        if (clone == NULL) {
            return 0;
        }
        pdpt[pdpt_index] = ((uint64_t)(uintptr_t)clone) | paging_flags_table | paging_user;
    } else if ((pdpt[pdpt_index] & paging_present) != 0u) {
        pdpt[pdpt_index] |= paging_user;
    }

    if ((pdpt[pdpt_index] & paging_present) == 0u) {
        return 1;
    }

    pd = entry_table(pdpt[pdpt_index]);
    kernel_pd = (kernel_pdpt_entry & paging_present) == 0u ? NULL : entry_table(kernel_pdpt_entry);
    kernel_pd_entry = kernel_pd == NULL ? 0u : kernel_pd[pd_index];
    if ((pd[pd_index] & paging_present) != 0u &&
        entry_address(pd[pd_index]) == entry_address(kernel_pd_entry)) {
        clone = copy_table(entry_table(pd[pd_index]));
        if (clone == NULL) {
            return 0;
        }
        pd[pd_index] = ((uint64_t)(uintptr_t)clone) | paging_flags_table | paging_user;
    } else if ((pd[pd_index] & paging_present) != 0u) {
        pd[pd_index] |= paging_user;
    }

    if ((pd[pd_index] & paging_present) == 0u) {
        return 1;
    }

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

    paging_kernel_pml4 = alloc_table();
    if (paging_kernel_pml4 == NULL) {
        arch_halt_forever();
    }

    limit = highest_physical_end();
    if (limit == 0u) {
        arch_halt_forever();
    }

    address = 0u;
    while (address < limit) {
        if (!map_page_4k(paging_kernel_pml4, address, address, paging_flags_page)) {
            arch_halt_forever();
        }

        address += (uintptr_t)paging_page_size;
    }

    arch_load_cr3((uint64_t)(uintptr_t)paging_kernel_pml4);
}

uintptr_t paging_kernel_address_space(void) {
    return (uintptr_t)paging_kernel_pml4;
}

uintptr_t paging_create_user_address_space(void) {
    uint64_t *pml4;
    size_t index;

    pml4 = alloc_table();
    if (pml4 == NULL) {
        return 0u;
    }

    for (index = 0u; index < 512u; ++index) {
        pml4[index] = paging_kernel_pml4[index];
    }

    return (uintptr_t)pml4;
}

void paging_map_user_page(uintptr_t address_space, uintptr_t virtual_address, uintptr_t physical_address) {
    uint64_t *pml4;

    virtual_address &= ~((uintptr_t)paging_page_size - 1u);
    physical_address &= ~((uintptr_t)paging_page_size - 1u);
    pml4 = (uint64_t *)(void *)address_space;

    if (address_space != (uintptr_t)paging_kernel_pml4 &&
        !ensure_private_user_path(pml4, virtual_address)) {
        arch_halt_forever();
    }

    if (!map_page_4k(pml4, virtual_address, physical_address, paging_present | paging_writable | paging_user)) {
        arch_halt_forever();
    }
}

void paging_activate(uintptr_t address_space) {
    arch_load_cr3((uint64_t)address_space);
}

int paging_address_space_valid(uintptr_t address_space) {
    uint64_t *pml4;
    size_t index;

    if (address_space == 0u) {
        return 0;
    }

    pml4 = (uint64_t *)(void *)address_space;
    for (index = 0u; index < 512u; ++index) {
        if ((pml4[index] & paging_present) != (paging_kernel_pml4[index] & paging_present)) {
            return 0;
        }
    }

    return 1;
}

int paging_mapping_matches(uintptr_t address_space, uintptr_t virtual_address, uintptr_t physical_address, int user_accessible) {
    size_t pt_index;
    uint64_t *pml4;
    uint64_t *pt;
    uint64_t entry;

    if (address_space == 0u) {
        return 0;
    }

    virtual_address &= ~((uintptr_t)paging_page_size - 1u);
    physical_address &= ~((uintptr_t)paging_page_size - 1u);
    pml4 = (uint64_t *)(void *)address_space;
    pt = walk_to_pt(pml4, virtual_address);
    if (pt == NULL) {
        return 0;
    }

    pt_index = (size_t)((virtual_address >> 12) & 0x1FFu);
    entry = pt[pt_index];
    if ((entry & paging_present) == 0u) {
        return 0;
    }
    if ((entry & 0x000FFFFFFFFFF000ull) != (uint64_t)physical_address) {
        return 0;
    }
    if (user_accessible) {
        return (entry & paging_user) != 0u;
    }
    return (entry & paging_user) == 0u;
}

int paging_mapping_is_supervisor_only(uintptr_t address_space, uintptr_t virtual_address) {
    return paging_mapping_matches(address_space, virtual_address, virtual_address, 0);
}

int paging_user_range_mapped(uintptr_t address_space, uintptr_t virtual_address, size_t count)
{
    uintptr_t start;
    uintptr_t end;
    uintptr_t page;

    if (address_space == 0u) {
        return 0;
    }

    if (count == 0u) {
        return 1;
    }

    start = virtual_address & ~(uintptr_t)(paging_page_size - 1u);
    end = virtual_address + (uintptr_t)(count - 1u);
    if (end < virtual_address) {
        return 0;
    }

    end &= ~(uintptr_t)(paging_page_size - 1u);
    for (page = start;; page += (uintptr_t)paging_page_size) {
        uint64_t *pt;
        size_t pt_index;
        uint64_t entry;

        pt = walk_to_pt((uint64_t *)(void *)address_space, page);
        if (pt == NULL) {
            return 0;
        }

        pt_index = (size_t)((page >> 12) & 0x1FFu);
        entry = pt[pt_index];
        if ((entry & paging_present) == 0u || (entry & paging_user) == 0u) {
            return 0;
        }

        if (page == end) {
            break;
        }
    }

    return 1;
}

void paging_release_user_address_space(uintptr_t address_space, const struct user_virtual_layout *layout)
{
    uint64_t *pml4;
    size_t pml4_index;
    size_t pdpt_index;
    size_t pd_index;
    uint64_t kernel_pml4_entry;
    uintptr_t kernel_pdpt_address;
    uint64_t *pdpt;
    uint64_t *kernel_pdpt;
    uint64_t *pd;
    uint64_t *kernel_pd;
    uint64_t *pt;
    uintptr_t pdpt_address;
    uintptr_t kernel_pd_address;
    uintptr_t pd_address;
    uintptr_t kernel_pt_address;
    uintptr_t pt_address;

    if (address_space == 0u || address_space == (uintptr_t)paging_kernel_pml4 || layout == 0) {
        return;
    }

    pml4 = (uint64_t *)(void *)address_space;
    pml4_index = (size_t)((layout->trampoline_base >> 39) & 0x1FFu);
    pdpt_index = (size_t)((layout->trampoline_base >> 30) & 0x1FFu);
    pd_index = (size_t)((layout->trampoline_base >> 21) & 0x1FFu);

    kernel_pml4_entry = paging_kernel_pml4[pml4_index];
    kernel_pdpt_address = entry_address(kernel_pml4_entry);
    if ((pml4[pml4_index] & paging_present) == 0u) {
        palloc_free(address_space);
        return;
    }

    pdpt = entry_table(pml4[pml4_index]);
    pdpt_address = entry_address(pml4[pml4_index]);
    kernel_pdpt = kernel_pdpt_address == 0u ? NULL : (uint64_t *)(void *)kernel_pdpt_address;
    kernel_pd_address = kernel_pdpt == NULL ? 0u : entry_address(kernel_pdpt[pdpt_index]);
    if ((pdpt[pdpt_index] & paging_present) != 0u) {
        pd = entry_table(pdpt[pdpt_index]);
        pd_address = entry_address(pdpt[pdpt_index]);
        kernel_pd = kernel_pd_address == 0u ? NULL : (uint64_t *)(void *)kernel_pd_address;
        kernel_pt_address = kernel_pd == NULL ? 0u : entry_address(kernel_pd[pd_index]);
        if ((pd[pd_index] & paging_present) != 0u) {
            pt = entry_table(pd[pd_index]);
            pt_address = entry_address(pd[pd_index]);
            if (pt_address != kernel_pt_address) {
                palloc_free((uintptr_t)(void *)pt);
            }
        }

        if (pd_address != kernel_pd_address) {
            palloc_free((uintptr_t)(void *)pd);
        }
    }

    if (pdpt_address != kernel_pdpt_address) {
        palloc_free((uintptr_t)(void *)pdpt);
    }

    palloc_free(address_space);
}
