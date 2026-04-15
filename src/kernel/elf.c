#include "kernel/elf.h"
#include "kernel/core.h"
#include "kernel/paging.h"
#include "kernel/palloc.h"

#include <stddef.h>
#include <stdint.h>

enum {
    elf_magic0 = 0x7Fu,
    elf_magic1 = 'E',
    elf_magic2 = 'L',
    elf_magic3 = 'F',
    elf_class64 = 2,
    elf_little_endian = 1,
    elf_version_current = 1,
    elf_type_exec = 2,
    elf_machine_x86_64 = 62,
    elf_phdr_type_load = 1
};

struct elf64_header {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
};

struct elf64_program_header {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
};

static void memory_copy(uint8_t *dst, const uint8_t *src, size_t count)
{
    size_t index;

    for (index = 0u; index < count; ++index) {
        dst[index] = src[index];
    }
}

static void memory_zero(uint8_t *dst, size_t count)
{
    size_t index;

    for (index = 0u; index < count; ++index) {
        dst[index] = 0u;
    }
}

static uintptr_t align_down(uintptr_t value)
{
    return value & ~(uintptr_t)4095u;
}

static uintptr_t align_up(uintptr_t value)
{
    return (value + 4095u) & ~(uintptr_t)4095u;
}

static int range_fits(size_t size, uint64_t offset, uint64_t count)
{
    if (offset > (uint64_t)size) {
        return 0;
    }

    if (count > (uint64_t)size - offset) {
        return 0;
    }

    return 1;
}

static int validate_header(const struct elf64_header *header, size_t image_size)
{
    if (image_size < sizeof(*header)) {
        return 0;
    }

    if (header->ident[0] != elf_magic0 ||
        header->ident[1] != elf_magic1 ||
        header->ident[2] != elf_magic2 ||
        header->ident[3] != elf_magic3) {
        return 0;
    }

    if (header->ident[4] != elf_class64 ||
        header->ident[5] != elf_little_endian ||
        header->ident[6] != elf_version_current) {
        return 0;
    }

    if (header->type != elf_type_exec ||
        header->machine != elf_machine_x86_64 ||
        header->version != elf_version_current) {
        return 0;
    }

    if (header->ehsize != sizeof(*header) ||
        header->phentsize != sizeof(struct elf64_program_header)) {
        return 0;
    }

    return range_fits(image_size, header->phoff, (uint64_t)header->phnum * (uint64_t)header->phentsize);
}

static int validate_program_headers(const struct elf64_program_header *phdrs, uint16_t phnum)
{
    uint16_t index;

    if (phnum == 0u) {
        return 0;
    }

    for (index = 0u; index < phnum; ++index) {
        if (phdrs[index].type != elf_phdr_type_load) {
            return 0;
        }
    }

    return 1;
}

static int compute_load_span(const struct elf64_program_header *phdrs, uint16_t phnum, uintptr_t *span_begin, uintptr_t *span_end)
{
    uint16_t index;
    uint64_t lowest;
    uint64_t highest;
    int found;

    lowest = 0u;
    highest = 0u;
    found = 0;

    for (index = 0u; index < phnum; ++index) {
        uint64_t begin;
        uint64_t end;

        if (phdrs[index].type != elf_phdr_type_load) {
            continue;
        }

        if (phdrs[index].memsz == 0u) {
            return 0;
        }

        begin = phdrs[index].vaddr;
        end = phdrs[index].vaddr + phdrs[index].memsz;
        if (end < begin) {
            return 0;
        }

        if (!found || begin < lowest) {
            lowest = begin;
        }

        if (!found || end > highest) {
            highest = end;
        }

        found = 1;
    }

    if (!found) {
        return 0;
    }

    *span_begin = align_down((uintptr_t)lowest);
    *span_end = align_up((uintptr_t)highest);
    return *span_end > *span_begin;
}

static int claim_window_pages(uintptr_t load_begin, uintptr_t load_end)
{
    uintptr_t page;

    for (page = load_begin; page < load_end; page += 4096u) {
        if (!palloc_take(page)) {
            return 0;
        }
    }

    return 1;
}

static void clear_window(uintptr_t load_begin, uintptr_t load_end)
{
    uintptr_t page;

    for (page = load_begin; page < load_end; page += 4096u) {
        memory_zero((uint8_t *)(void *)page, 4096u);
    }
}

static int load_segment(const uint8_t *image, size_t image_size, const struct elf64_program_header *phdr, uintptr_t source_base, uintptr_t load_begin)
{
    uintptr_t dst_begin;
    uint8_t *dst;

    if (phdr->memsz < phdr->filesz) {
        return 0;
    }

    if (!range_fits(image_size, phdr->offset, phdr->filesz)) {
        return 0;
    }

    if (phdr->vaddr < source_base) {
        return 0;
    }

    dst_begin = load_begin + (uintptr_t)(phdr->vaddr - source_base);
    dst = (uint8_t *)(void *)dst_begin;
    memory_copy(dst, &image[(size_t)phdr->offset], (size_t)phdr->filesz);
    memory_zero(dst + phdr->filesz, (size_t)(phdr->memsz - phdr->filesz));
    return 1;
}

int elf_load_user_image(
    const uint8_t *image,
    size_t image_size,
    const struct user_virtual_layout *layout,
    struct loaded_user_image *loaded_image
)
{
    const struct elf64_header *header;
    const struct elf64_program_header *phdrs;
    uintptr_t source_begin;
    uintptr_t source_end;
    uintptr_t load_begin;
    uintptr_t load_end;
    uintptr_t user_stack_page;
    uint16_t index;

    header = (const struct elf64_header *)(const void *)image;
    if (!validate_header(header, image_size)) {
        return 0;
    }

    phdrs = (const struct elf64_program_header *)(const void *)(image + header->phoff);
    if (!validate_program_headers(phdrs, header->phnum)) {
        return 0;
    }

    if (!compute_load_span(phdrs, header->phnum, &source_begin, &source_end)) {
        return 0;
    }

    if (header->entry < source_begin || header->entry >= source_end) {
        return 0;
    }

    if ((source_end - source_begin) > (uintptr_t)elf_user_load_size) {
        return 0;
    }

    if (layout->image_base == 0u || layout->stack_top == 0u) {
        return 0;
    }

    load_begin = (uintptr_t)elf_user_load_base;
    load_end = load_begin + (source_end - source_begin);

    if (!claim_window_pages(load_begin, load_end)) {
        return 0;
    }

    clear_window(load_begin, load_end);

    for (index = 0u; index < header->phnum; ++index) {
        if (!load_segment(image, image_size, &phdrs[index], source_begin, load_begin)) {
            return 0;
        }
    }

    for (index = 0u; index < (uint16_t)((load_end - load_begin) / 4096u); ++index) {
        paging_map_user_page(
            layout->image_base + (uintptr_t)index * 4096u,
            load_begin + (uintptr_t)index * 4096u
        );
    }

    user_stack_page = palloc_alloc();
    if (user_stack_page == 0u) {
        return 0;
    }

    paging_map_user_page(layout->stack_top - 4096u, user_stack_page);

    loaded_image->load_begin = layout->image_base;
    loaded_image->load_end = layout->image_base + (load_end - load_begin);
    loaded_image->entry = (void (*)(void))(layout->image_base + (uintptr_t)(header->entry - source_begin));
    loaded_image->stack_top = layout->stack_top;
    return 1;
}
