#include "kernel/fd.h"
#include "kernel/core.h"
#include "kernel/heap.h"
#include "drivers/serial.h"

#include <stddef.h>
#include <stdint.h>

struct fd_console_output {
    uint32_t refs;
};

struct fd_console_input {
    uint32_t refs;
    char byte;
    uint32_t used;
};

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

static void fd_fail(const char *text)
{
    write_line(text);
    arch_halt_forever();
}

void fd_table_initialize(struct fd_table *table)
{
    int index;

    if (table == 0) {
        fd_fail("fd table fail");
    }

    for (index = 0; index < fd_table_capacity; ++index) {
        table->entries[index].used = 0u;
        table->entries[index].kind = FD_KIND_NONE;
        table->entries[index].object = 0;
        table->entries[index].ops = 0;
    }
}

int fd_table_install(struct fd_table *table, fd_kind_t kind, void *object, const struct fd_ops *ops)
{
    int index;

    if (table == 0 || kind == FD_KIND_NONE) {
        return -1;
    }

    for (index = 0; index < fd_table_capacity; ++index) {
        if (table->entries[index].used == 0u) {
            table->entries[index].used = 1u;
            table->entries[index].kind = kind;
            table->entries[index].object = object;
            table->entries[index].ops = ops;
            return index;
        }
    }

    return -1;
}

const struct fd_entry *fd_table_get(const struct fd_table *table, int fd)
{
    if (table == 0 || fd < 0 || fd >= fd_table_capacity) {
        return 0;
    }

    if (table->entries[fd].used == 0u) {
        return 0;
    }

    return &table->entries[fd];
}

int fd_table_dup(struct fd_table *table, int fd)
{
    const struct fd_entry *entry;

    entry = fd_table_get(table, fd);
    if (entry == 0) {
        return -1;
    }

    if (entry->ops != 0 && entry->ops->retain != 0) {
        if (entry->ops->retain(entry->object) != 0) {
            return -1;
        }
    }

    return fd_table_install(table, entry->kind, entry->object, entry->ops);
}

int fd_table_read(struct fd_table *table, int fd, void *buffer, size_t count)
{
    const struct fd_entry *entry;

    entry = fd_table_get(table, fd);
    if (entry == 0 || entry->ops == 0 || entry->ops->read == 0) {
        return -1;
    }

    return entry->ops->read(entry->object, buffer, count);
}

int fd_table_write(struct fd_table *table, int fd, const void *buffer, size_t count)
{
    const struct fd_entry *entry;

    entry = fd_table_get(table, fd);
    if (entry == 0 || entry->ops == 0 || entry->ops->write == 0) {
        return -1;
    }

    return entry->ops->write(entry->object, buffer, count);
}

int fd_table_close(struct fd_table *table, int fd)
{
    struct fd_entry *entry;
    int result;

    if (table == 0 || fd < 0 || fd >= fd_table_capacity) {
        return -1;
    }

    entry = &table->entries[fd];
    if (entry->used == 0u) {
        return -1;
    }

    result = 0;
    if (entry->ops != 0 && entry->ops->close != 0) {
        result = entry->ops->close(entry->object);
    }

    entry->used = 0u;
    entry->kind = FD_KIND_NONE;
    entry->object = 0;
    entry->ops = 0;
    return result;
}

void fd_table_close_all(struct fd_table *table)
{
    int index;

    if (table == 0) {
        return;
    }

    for (index = 0; index < fd_table_capacity; ++index) {
        if (table->entries[index].used != 0u) {
            (void)fd_table_close(table, index);
        }
    }
}

static int fd_console_output_retain(void *object)
{
    struct fd_console_output *console;

    console = (struct fd_console_output *)object;
    if (console == 0 || console->refs == 0u) {
        return -1;
    }

    ++console->refs;
    return 0;
}

static int fd_console_output_close(void *object)
{
    struct fd_console_output *console;

    console = (struct fd_console_output *)object;
    if (console == 0 || console->refs == 0u) {
        return -1;
    }

    --console->refs;
    if (console->refs == 0u) {
        kfree(console);
    }

    return 0;
}

static int fd_console_output_write(void *object, const void *buffer, size_t count)
{
    const struct fd_console_output *console;

    console = (const struct fd_console_output *)object;
    if (console == 0 || console->refs == 0u) {
        return -1;
    }

    if (buffer == 0 && count != 0u) {
        return -1;
    }

    if (count != 0u) {
        serial_write((const char *)buffer, count);
    }

    return (int)count;
}

static int fd_console_input_retain(void *object)
{
    struct fd_console_input *console;

    console = (struct fd_console_input *)object;
    if (console == 0 || console->refs == 0u) {
        return -1;
    }

    ++console->refs;
    return 0;
}

static int fd_console_input_close(void *object)
{
    struct fd_console_input *console;

    console = (struct fd_console_input *)object;
    if (console == 0 || console->refs == 0u) {
        return -1;
    }

    --console->refs;
    if (console->refs == 0u) {
        kfree(console);
    }

    return 0;
}

static int fd_console_input_read(void *object, void *buffer, size_t count)
{
    struct fd_console_input *console;

    console = (struct fd_console_input *)object;
    if (console == 0 || console->refs == 0u) {
        return -1;
    }

    if (buffer == 0 && count != 0u) {
        return -1;
    }

    if (count == 0u) {
        return 0;
    }

    if (console->used != 0u) {
        return 0;
    }

    ((char *)buffer)[0] = console->byte;
    console->used = 1u;
    return 1;
}

int fd_table_seed_console(struct fd_table *table)
{
    static const struct fd_ops console_input_ops = {
        fd_console_input_retain,
        fd_console_input_close,
        fd_console_input_read,
        0
    };
    static const struct fd_ops console_output_ops = {
        fd_console_output_retain,
        fd_console_output_close,
        0,
        fd_console_output_write
    };
    struct fd_console_input *input;
    struct fd_console_output *output;

    if (table == 0) {
        return 0;
    }

    input = (struct fd_console_input *)kzalloc(sizeof(*input));
    output = (struct fd_console_output *)kzalloc(sizeof(*output));
    if (input == 0 || output == 0) {
        kfree(input);
        kfree(output);
        return 0;
    }

    input->refs = 1u;
    input->byte = '@';
    input->used = 0u;
    output->refs = 2u;

    if (fd_table_install(table, FD_KIND_CONSOLE, input, &console_input_ops) != 0 ||
        fd_table_install(table, FD_KIND_CONSOLE, output, &console_output_ops) != 1 ||
        fd_table_install(table, FD_KIND_CONSOLE, output, &console_output_ops) != 2) {
        fd_console_input_close(input);
        fd_console_output_close(output);
        fd_console_output_close(output);
        return 0;
    }

    return 1;
}

static int fd_self_test_close(void *object)
{
    uint32_t *close_count;

    close_count = (uint32_t *)object;
    *close_count += 1u;
    return 0;
}

void fd_self_test(void)
{
    static const struct fd_ops close_ops = { 0, fd_self_test_close, 0, 0 };
    struct fd_table table;
    uint32_t close_count;
    int first_fd;
    int second_fd;
    int duplicate_fd;
    char ch;
    const struct fd_entry *entry;

    close_count = 0u;
    fd_table_initialize(&table);

    first_fd = fd_table_install(&table, FD_KIND_PLACEHOLDER, &close_count, &close_ops);
    second_fd = fd_table_install(&table, FD_KIND_PLACEHOLDER, &close_count, &close_ops);
    if (first_fd != 0 || second_fd != 1) {
        write_line("fd fail");
        return;
    }

    entry = fd_table_get(&table, first_fd);
    if (entry == 0 || entry->kind != FD_KIND_PLACEHOLDER) {
        write_line("fd fail");
        return;
    }

    if (fd_table_close(&table, first_fd) != 0 || close_count != 1u) {
        write_line("fd fail");
        return;
    }

    fd_table_close_all(&table);
    if (close_count != 2u || fd_table_get(&table, second_fd) != 0) {
        write_line("fd fail");
        return;
    }

    fd_table_initialize(&table);
    if (!fd_table_seed_console(&table)) {
        write_line("fd fail");
        return;
    }

    if (fd_table_read(&table, 0, &ch, 1u) != 1 || ch != '@') {
        write_line("fd fail");
        return;
    }

    duplicate_fd = fd_table_dup(&table, 1);
    if (duplicate_fd < 0 || fd_table_write(&table, duplicate_fd, "", 0u) != 0) {
        write_line("fd fail");
        return;
    }

    if (fd_table_close(&table, duplicate_fd) != 0 || fd_table_close(&table, 1) != 0) {
        write_line("fd fail");
        return;
    }

    fd_table_close_all(&table);
    write_line("fd ok");
}
