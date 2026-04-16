#include "kernel/heap.h"
#include "kernel/core.h"
#include "kernel/palloc.h"
#include "drivers/serial.h"

#include <stddef.h>
#include <stdint.h>

enum {
    heap_page_size = 4096u,
    heap_align = 16u,
    heap_magic = 0x48454150u
};

struct heap_block {
    uint32_t magic;
    uint32_t free;
    size_t size;
    struct heap_block *next;
    struct heap_block *prev;
};

static struct heap_block *heap_head;

static size_t string_length(const char *text)
{
    size_t length;

    length = 0u;
    while (text[length] != '\0') {
        ++length;
    }

    return length;
}

static void write_text(const char *text)
{
    serial_write(text, string_length(text));
}

static void write_line(const char *text)
{
    write_text(text);
    serial_write("\r\n", 2u);
}

static void heap_fail(const char *text)
{
    write_line(text);
    arch_halt_forever();
}

static size_t align_up(size_t value)
{
    return (value + (heap_align - 1u)) & ~(heap_align - 1u);
}

static uintptr_t block_end(const struct heap_block *block)
{
    return (uintptr_t)(const void *)block + sizeof(*block) + block->size;
}

static int blocks_touch(const struct heap_block *left, const struct heap_block *right)
{
    return block_end(left) == (uintptr_t)(const void *)right;
}

static void split_block(struct heap_block *block, size_t size)
{
    struct heap_block *next;
    uintptr_t next_address;
    size_t remaining;

    if (block->size <= size + sizeof(*block) + heap_align) {
        return;
    }

    remaining = block->size - size - sizeof(*block);
    next_address = (uintptr_t)(void *)block + sizeof(*block) + size;
    next = (struct heap_block *)(void *)next_address;
    next->magic = heap_magic;
    next->free = 1u;
    next->size = remaining;
    next->next = block->next;
    next->prev = block;

    if (next->next != NULL) {
        next->next->prev = next;
    }

    block->size = size;
    block->next = next;
}

static void coalesce_forward(struct heap_block *block)
{
    while (block->next != NULL &&
           block->next->magic == heap_magic &&
           block->next->free != 0u &&
           blocks_touch(block, block->next)) {
        struct heap_block *next;

        next = block->next;
        block->size += sizeof(*next) + next->size;
        block->next = next->next;
        if (block->next != NULL) {
            block->next->prev = block;
        }
    }
}

static struct heap_block *insert_block_sorted(struct heap_block *block)
{
    struct heap_block *current;
    struct heap_block *previous;

    current = heap_head;
    previous = NULL;

    while (current != NULL && (uintptr_t)(void *)current < (uintptr_t)(void *)block) {
        previous = current;
        current = current->next;
    }

    block->prev = previous;
    block->next = current;

    if (previous != NULL) {
        previous->next = block;
    } else {
        heap_head = block;
    }

    if (current != NULL) {
        current->prev = block;
    }

    if (block->prev != NULL &&
        block->prev->magic == heap_magic &&
        block->prev->free != 0u &&
        blocks_touch(block->prev, block)) {
        block = block->prev;
        coalesce_forward(block);
    } else {
        coalesce_forward(block);
    }

    return block;
}

static struct heap_block *grow_heap(size_t size)
{
    uintptr_t page;
    struct heap_block *block;
    size_t capacity;

    capacity = heap_page_size - sizeof(struct heap_block);
    if (size > capacity) {
        return NULL;
    }

    page = palloc_alloc();
    if (page == 0u) {
        return NULL;
    }

    block = (struct heap_block *)(void *)page;
    block->magic = heap_magic;
    block->free = 1u;
    block->size = capacity;
    block->next = NULL;
    block->prev = NULL;

    return insert_block_sorted(block);
}

static struct heap_block *find_block(size_t size)
{
    struct heap_block *block;

    block = heap_head;
    while (block != NULL) {
        if (block->magic != heap_magic) {
            heap_fail("heap corrupt");
        }

        if (block->free != 0u && block->size >= size) {
            return block;
        }

        block = block->next;
    }

    return NULL;
}

void heap_initialize(void)
{
    heap_head = NULL;
}

void *kmalloc(size_t size)
{
    struct heap_block *block;

    if (size == 0u) {
        return NULL;
    }

    size = align_up(size);
    block = find_block(size);
    if (block == NULL) {
        block = grow_heap(size);
        if (block == NULL) {
            return NULL;
        }
    }

    split_block(block, size);
    block->free = 0u;
    return (void *)(block + 1);
}

void *kzalloc(size_t size)
{
    uint8_t *ptr;
    size_t index;

    ptr = (uint8_t *)kmalloc(size);
    if (ptr == NULL) {
        return NULL;
    }

    for (index = 0u; index < size; ++index) {
        ptr[index] = 0u;
    }

    return ptr;
}

void kfree(void *ptr)
{
    struct heap_block *block;

    if (ptr == NULL) {
        return;
    }

    block = ((struct heap_block *)ptr) - 1;
    if (block->magic != heap_magic || block->free != 0u) {
        heap_fail("heap free fail");
    }

    block->free = 1u;
    coalesce_forward(block);

    if (block->prev != NULL &&
        block->prev->magic == heap_magic &&
        block->prev->free != 0u &&
        blocks_touch(block->prev, block)) {
        coalesce_forward(block->prev);
    }
}

void heap_self_test(void)
{
    void *first;
    void *second;
    uint8_t *third;
    void *merged;
    size_t index;

    first = kmalloc(32u);
    second = kmalloc(64u);
    third = (uint8_t *)kzalloc(24u);

    if (first == NULL || second == NULL || third == NULL) {
        write_line("heap fail");
        return;
    }

    for (index = 0u; index < 24u; ++index) {
        if (third[index] != 0u) {
            write_line("heap fail");
            return;
        }
    }

    kfree(second);
    kfree(first);
    merged = kmalloc(80u);
    if (merged == NULL) {
        write_line("heap fail");
        return;
    }

    kfree(merged);
    kfree(third);
    write_line("heap ok");
}
