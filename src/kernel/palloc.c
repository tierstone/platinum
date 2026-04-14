#include "kernel/palloc.h"
#include "kernel/memmap.h"
#include "drivers/serial.h"

#include <stddef.h>
#include <stdint.h>

struct palloc_node {
    struct palloc_node *next;
};

static const uintptr_t page_size = 4096u;
static const uintptr_t palloc_low_cutoff = 0x02000000u;
static const uintptr_t palloc_high_limit = (uintptr_t)0x100000000ull;

static struct palloc_node *free_list_head;
static uintptr_t alloc_region_begin;
static uintptr_t alloc_region_end;
static uint64_t free_page_count;

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

static void write_hex_uintptr(uintptr_t value) {
    static const char digits[] = "0123456789abcdef";
    char output[2 + sizeof(uintptr_t) * 2];
    size_t index;

    output[0] = '0';
    output[1] = 'x';

    for (index = 0u; index < sizeof(uintptr_t) * 2u; ++index) {
        unsigned int shift;
        uintptr_t nibble;

        shift = (unsigned int)((sizeof(uintptr_t) * 2u - 1u - index) * 4u);
        nibble = (value >> shift) & (uintptr_t)0xFu;
        output[2u + index] = digits[(unsigned int)nibble];
    }

    serial_write(output, sizeof(output));
}

void palloc_initialize(void) {
    uintptr_t region_begin;
    uintptr_t region_end;
    uintptr_t page;

    free_list_head = NULL;
    alloc_region_begin = 0u;
    alloc_region_end = 0u;
    free_page_count = 0u;

    if (!memmap_find_largest_conventional_region(palloc_low_cutoff, palloc_high_limit, &region_begin, &region_end)) {
        return;
    }

    alloc_region_begin = region_begin;
    alloc_region_end = region_end;

    page = region_begin;

    while (page + page_size <= region_end) {
        struct palloc_node *node = (struct palloc_node *)(void *)page;
        node->next = free_list_head;
        free_list_head = node;
        ++free_page_count;
        page += page_size;
    }
}

uintptr_t palloc_alloc(void) {
    struct palloc_node *node;

    if (free_list_head == NULL) {
        return 0u;
    }

    node = free_list_head;
    free_list_head = node->next;
    --free_page_count;

    return (uintptr_t)(void *)node;
}

void palloc_free(uintptr_t page) {
    struct palloc_node *node;

    if (page == 0u) {
        return;
    }

    node = (struct palloc_node *)(void *)page;
    node->next = free_list_head;
    free_list_head = node;
    ++free_page_count;
}

int palloc_take(uintptr_t page) {
    struct palloc_node **link;

    page &= ~(page_size - 1u);

    link = &free_list_head;
    while (*link != NULL) {
        if ((uintptr_t)(void *)(*link) == page) {
            struct palloc_node *node = *link;
            *link = node->next;
            --free_page_count;
            return 1;
        }

        link = &((*link)->next);
    }

    return 0;
}

void palloc_self_test(void) {
    uintptr_t first;
    uintptr_t second;

    write_text("palloc region ");
    write_hex_uintptr(alloc_region_begin);
    write_text(" ");
    write_hex_uintptr(alloc_region_end);
    write_text("\r\n");

    write_text("palloc pages ");
    write_decimal_u64(free_page_count);
    write_text("\r\n");

    first = palloc_alloc();
    second = palloc_alloc();

    if (first == 0u || second == 0u) {
        write_line("palloc fail");
        return;
    }

    write_line("palloc ok");

    write_text("page ");
    write_hex_uintptr(first);
    write_text("\r\n");

    write_text("page ");
    write_hex_uintptr(second);
    write_text("\r\n");

    palloc_free(second);
    palloc_free(first);

    write_line("free ok");
}
