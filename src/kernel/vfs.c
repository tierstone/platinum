#include "kernel/vfs.h"
#include "kernel/core.h"
#include "kernel/heap.h"
#include "drivers/serial.h"

#include <stddef.h>
#include <stdint.h>

struct vfs_self_test_data {
    uint32_t released;
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

static void vfs_fail(const char *text)
{
    write_line(text);
    arch_halt_forever();
}

void vfs_node_initialize(
    struct vfs_node *node,
    vfs_node_kind_t kind,
    const struct vfs_node_ops *ops,
    void *data
)
{
    if (node == 0 || kind == VFS_NODE_NONE || ops == 0) {
        vfs_fail("vfs init fail");
    }

    node->kind = kind;
    node->refs = 0u;
    node->ops = ops;
    node->data = data;
}

struct vfs_file *vfs_file_open(struct vfs_node *node)
{
    struct vfs_file *file;

    if (node == 0 || node->ops == 0) {
        return 0;
    }

    file = (struct vfs_file *)kzalloc(sizeof(*file));
    if (file == 0) {
        return 0;
    }

    file->refs = 1u;
    file->node = node;
    file->offset = 0u;
    ++node->refs;
    return file;
}

int vfs_file_retain(struct vfs_file *file)
{
    if (file == 0 || file->refs == 0u || file->node == 0) {
        return -1;
    }

    ++file->refs;
    return 0;
}

int vfs_file_close(struct vfs_file *file)
{
    struct vfs_node *node;
    int result;

    if (file == 0 || file->refs == 0u || file->node == 0) {
        return -1;
    }

    --file->refs;
    if (file->refs != 0u) {
        return 0;
    }

    node = file->node;
    file->node = 0;
    kfree(file);

    if (node->refs == 0u) {
        return -1;
    }

    --node->refs;
    result = 0;
    if (node->refs == 0u && node->ops->release != 0) {
        result = node->ops->release(node);
    }

    return result;
}

int vfs_file_read(struct vfs_file *file, void *buffer, size_t count)
{
    if (file == 0 || file->node == 0 || file->node->ops == 0 || file->node->ops->read == 0) {
        return -1;
    }

    return file->node->ops->read(file, buffer, count);
}

int vfs_file_write(struct vfs_file *file, const void *buffer, size_t count)
{
    if (file == 0 || file->node == 0 || file->node->ops == 0 || file->node->ops->write == 0) {
        return -1;
    }

    return file->node->ops->write(file, buffer, count);
}

static int vfs_self_test_release(struct vfs_node *node)
{
    struct vfs_self_test_data *data;

    data = (struct vfs_self_test_data *)node->data;
    data->released += 1u;
    kfree(node);
    kfree(data);
    return 0;
}

static int vfs_self_test_read(struct vfs_file *file, void *buffer, size_t count)
{
    struct vfs_self_test_data *data;

    data = (struct vfs_self_test_data *)file->node->data;
    if (count == 0u) {
        return 0;
    }

    if (data->used != 0u) {
        return 0;
    }

    ((char *)buffer)[0] = data->byte;
    data->used = 1u;
    file->offset += 1u;
    return 1;
}

static int vfs_self_test_write(struct vfs_file *file, const void *buffer, size_t count)
{
    (void)file;
    (void)buffer;
    return (int)count;
}

void vfs_self_test(void)
{
    static const struct vfs_node_ops ops = {
        vfs_self_test_release,
        vfs_self_test_read,
        vfs_self_test_write
    };
    struct vfs_node *node;
    struct vfs_self_test_data *data;
    struct vfs_file *file;
    char ch;

    data = (struct vfs_self_test_data *)kzalloc(sizeof(*data));
    node = (struct vfs_node *)kzalloc(sizeof(*node));
    if (data == 0 || node == 0) {
        write_line("vfs fail");
        kfree(data);
        kfree(node);
        return;
    }

    data->released = 0u;
    data->byte = '#';
    data->used = 0u;
    vfs_node_initialize(node, VFS_NODE_PLACEHOLDER, &ops, data);

    file = vfs_file_open(node);
    if (file == 0) {
        write_line("vfs fail");
        return;
    }

    ch = 0;
    if (vfs_file_read(file, &ch, 1u) != 1 || ch != '#') {
        write_line("vfs fail");
        return;
    }

    if (vfs_file_write(file, "", 0u) != 0) {
        write_line("vfs fail");
        return;
    }

    if (vfs_file_retain(file) != 0 || vfs_file_close(file) != 0 || vfs_file_close(file) != 0) {
        write_line("vfs fail");
        return;
    }

    write_line("vfs ok");
}
