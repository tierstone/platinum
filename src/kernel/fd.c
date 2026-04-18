#include "kernel/fd.h"
#include "kernel/core.h"
#include "kernel/heap.h"
#include "drivers/serial.h"

#include <stddef.h>
#include <stdint.h>

struct fd_console_output {
    uint32_t released;
};

struct fd_console_input {
    uint32_t released;
    char byte;
    uint32_t used;
};

struct fd_placeholder {
    uint32_t *released;
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
        table->entries[index].file = 0;
    }
}

int fd_table_install(struct fd_table *table, fd_kind_t kind, struct vfs_file *file)
{
    int index;

    if (table == 0 || kind == FD_KIND_NONE || file == 0) {
        return -1;
    }

    for (index = 0; index < fd_table_capacity; ++index) {
        if (table->entries[index].used == 0u) {
            table->entries[index].used = 1u;
            table->entries[index].kind = kind;
            table->entries[index].file = file;
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
    int duplicate_fd;

    entry = fd_table_get(table, fd);
    if (entry == 0) {
        return -1;
    }

    if (vfs_file_retain(entry->file) != 0) {
        return -1;
    }

    duplicate_fd = fd_table_install(table, entry->kind, entry->file);
    if (duplicate_fd < 0) {
        (void)vfs_file_close(entry->file);
        return -1;
    }

    return duplicate_fd;
}

int fd_table_read(struct fd_table *table, int fd, void *buffer, size_t count)
{
    const struct fd_entry *entry;

    entry = fd_table_get(table, fd);
    if (entry == 0) {
        return -1;
    }

    return vfs_file_read(entry->file, buffer, count);
}

int fd_table_write(struct fd_table *table, int fd, const void *buffer, size_t count)
{
    const struct fd_entry *entry;

    entry = fd_table_get(table, fd);
    if (entry == 0) {
        return -1;
    }

    return vfs_file_write(entry->file, buffer, count);
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

    result = vfs_file_close(entry->file);
    entry->used = 0u;
    entry->kind = FD_KIND_NONE;
    entry->file = 0;
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

static int fd_console_output_release(struct vfs_node *node)
{
    struct fd_console_output *console;

    console = (struct fd_console_output *)node->data;
    console->released += 1u;
    kfree(console);
    kfree(node);
    return 0;
}

static int fd_console_output_write(struct vfs_file *file, const void *buffer, size_t count)
{
    const struct fd_console_output *console;

    console = (const struct fd_console_output *)file->node->data;
    if (console == 0) {
        return -1;
    }

    if (buffer == 0 && count != 0u) {
        return -1;
    }

    if (count != 0u) {
        serial_write((const char *)buffer, count);
    }

    file->offset += count;
    return (int)count;
}

static int fd_console_input_release(struct vfs_node *node)
{
    struct fd_console_input *console;

    console = (struct fd_console_input *)node->data;
    console->released += 1u;
    kfree(console);
    kfree(node);
    return 0;
}

static int fd_console_input_read(struct vfs_file *file, void *buffer, size_t count)
{
    struct fd_console_input *console;

    console = (struct fd_console_input *)file->node->data;
    if (console == 0) {
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
    file->offset += 1u;
    return 1;
}

int fd_table_seed_console(struct fd_table *table)
{
    static const struct vfs_node_ops console_input_ops = {
        fd_console_input_release,
        fd_console_input_read,
        0
    };
    static const struct vfs_node_ops console_output_ops = {
        fd_console_output_release,
        0,
        fd_console_output_write
    };
    struct fd_console_input *input_data;
    struct fd_console_output *output_data;
    struct vfs_node *input_node;
    struct vfs_node *output_node;
    struct vfs_file *input_file;
    struct vfs_file *stdout_file;
    struct vfs_file *stderr_file;

    if (table == 0) {
        return 0;
    }

    input_data = (struct fd_console_input *)kzalloc(sizeof(*input_data));
    output_data = (struct fd_console_output *)kzalloc(sizeof(*output_data));
    input_node = (struct vfs_node *)kzalloc(sizeof(*input_node));
    output_node = (struct vfs_node *)kzalloc(sizeof(*output_node));
    if (input_data == 0 || output_data == 0 || input_node == 0 || output_node == 0) {
        kfree(input_data);
        kfree(output_data);
        kfree(input_node);
        kfree(output_node);
        return 0;
    }

    input_data->released = 0u;
    input_data->byte = '@';
    input_data->used = 0u;
    output_data->released = 0u;

    vfs_node_initialize(input_node, VFS_NODE_CONSOLE, &console_input_ops, input_data);
    vfs_node_initialize(output_node, VFS_NODE_CONSOLE, &console_output_ops, output_data);

    input_file = vfs_file_open(input_node, VFS_ACCESS_READ);
    stdout_file = vfs_file_open(output_node, VFS_ACCESS_WRITE);
    stderr_file = vfs_file_open(output_node, VFS_ACCESS_WRITE);
    if (input_file == 0 || stdout_file == 0 || stderr_file == 0) {
        vfs_file_close(input_file);
        vfs_file_close(stdout_file);
        vfs_file_close(stderr_file);
        return 0;
    }

    if (fd_table_install(table, FD_KIND_CONSOLE, input_file) != 0 ||
        fd_table_install(table, FD_KIND_CONSOLE, stdout_file) != 1 ||
        fd_table_install(table, FD_KIND_CONSOLE, stderr_file) != 2) {
        vfs_file_close(input_file);
        vfs_file_close(stdout_file);
        vfs_file_close(stderr_file);
        return 0;
    }

    return 1;
}

static int fd_placeholder_release(struct vfs_node *node)
{
    struct fd_placeholder *placeholder;

    placeholder = (struct fd_placeholder *)node->data;
    *(placeholder->released) += 1u;
    kfree(placeholder);
    kfree(node);
    return 0;
}

static int fd_placeholder_write(struct vfs_file *file, const void *buffer, size_t count)
{
    (void)file;
    (void)buffer;
    return (int)count;
}

static struct vfs_file *fd_make_placeholder_file(uint32_t *released)
{
    static const struct vfs_node_ops placeholder_ops = {
        fd_placeholder_release,
        0,
        fd_placeholder_write
    };
    struct fd_placeholder *placeholder;
    struct vfs_node *node;

    placeholder = (struct fd_placeholder *)kzalloc(sizeof(*placeholder));
    node = (struct vfs_node *)kzalloc(sizeof(*node));
    if (placeholder == 0 || node == 0) {
        kfree(placeholder);
        kfree(node);
        return 0;
    }

    placeholder->released = released;
    vfs_node_initialize(node, VFS_NODE_PLACEHOLDER, &placeholder_ops, placeholder);
    return vfs_file_open(node, VFS_ACCESS_WRITE);
}

void fd_self_test(void)
{
    struct fd_table table;
    int first_fd;
    int second_fd;
    int duplicate_fd;
    int index;
    char ch;
    uint32_t placeholder_a_released;
    uint32_t placeholder_b_released;

    fd_table_initialize(&table);

    placeholder_a_released = 0u;
    placeholder_b_released = 0u;
    first_fd = fd_table_install(&table, FD_KIND_PLACEHOLDER, fd_make_placeholder_file(&placeholder_a_released));
    second_fd = fd_table_install(&table, FD_KIND_PLACEHOLDER, fd_make_placeholder_file(&placeholder_b_released));
    if (first_fd != 0 || second_fd != 1) {
        write_line("fd fail");
        return;
    }

    if (fd_table_close(&table, first_fd) != 0 || placeholder_a_released != 1u) {
        write_line("fd fail");
        return;
    }

    fd_table_close_all(&table);
    if (placeholder_b_released != 1u || fd_table_get(&table, second_fd) != 0) {
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
    fd_table_initialize(&table);
    placeholder_a_released = 0u;
    first_fd = fd_table_install(&table, FD_KIND_PLACEHOLDER, fd_make_placeholder_file(&placeholder_a_released));
    if (first_fd != 0) {
        write_line("fd fail");
        return;
    }

    for (index = 1; index < fd_table_capacity; ++index) {
        if (fd_table_install(&table, FD_KIND_PLACEHOLDER, fd_make_placeholder_file(&placeholder_b_released)) != index) {
            write_line("fd fail");
            return;
        }
    }

    if (fd_table_dup(&table, first_fd) != -1 || placeholder_a_released != 0u) {
        write_line("fd fail");
        return;
    }

    if (fd_table_close(&table, first_fd) != 0 || placeholder_a_released != 1u) {
        write_line("fd fail");
        return;
    }

    fd_table_close_all(&table);
    write_line("fd ok");
}
