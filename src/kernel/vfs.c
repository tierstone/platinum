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

struct vfs_console_input_data {
    char byte;
    uint32_t used;
};

struct vfs_static_text_data {
    const char *text;
    size_t length;
};

struct vfs_namespace_entry {
    const char *path;
    struct vfs_node *node;
};

static struct vfs_console_input_data namespace_console_input_data;
static struct vfs_node namespace_console_input_node;
static struct vfs_node namespace_console_output_node;
static struct vfs_static_text_data namespace_banner_data;
static struct vfs_node namespace_banner_node;
static struct vfs_namespace_entry namespace_entries[3];
static int namespace_ready;

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

static int vfs_static_release(struct vfs_node *node)
{
    (void)node;
    return 0;
}

static int vfs_console_input_read(struct vfs_file *file, void *buffer, size_t count)
{
    struct vfs_console_input_data *data;

    data = (struct vfs_console_input_data *)file->node->data;
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

static int vfs_console_output_write(struct vfs_file *file, const void *buffer, size_t count)
{
    (void)file;

    if (buffer == 0 && count != 0u) {
        return -1;
    }

    if (count != 0u) {
        serial_write((const char *)buffer, count);
    }

    return (int)count;
}

static int vfs_static_text_read(struct vfs_file *file, void *buffer, size_t count)
{
    const struct vfs_static_text_data *data;
    size_t available;
    size_t index;

    data = (const struct vfs_static_text_data *)file->node->data;
    if (count == 0u) {
        return 0;
    }

    if ((size_t)file->offset >= data->length) {
        return 0;
    }

    available = data->length - (size_t)file->offset;
    if (count > available) {
        count = available;
    }

    for (index = 0u; index < count; ++index) {
        ((char *)buffer)[index] = data->text[(size_t)file->offset + index];
    }

    file->offset += count;
    return (int)count;
}

void vfs_namespace_initialize(void)
{
    static const struct vfs_node_ops console_input_ops = {
        vfs_static_release,
        vfs_console_input_read,
        0
    };
    static const struct vfs_node_ops console_output_ops = {
        vfs_static_release,
        0,
        vfs_console_output_write
    };
    static const struct vfs_node_ops static_text_ops = {
        vfs_static_release,
        vfs_static_text_read,
        0
    };

    namespace_console_input_data.byte = '@';
    namespace_console_input_data.used = 0u;
    namespace_banner_data.text = "platinum\n";
    namespace_banner_data.length = 9u;

    vfs_node_initialize(&namespace_console_input_node, VFS_NODE_CONSOLE, &console_input_ops, &namespace_console_input_data);
    vfs_node_initialize(&namespace_console_output_node, VFS_NODE_CONSOLE, &console_output_ops, 0);
    vfs_node_initialize(&namespace_banner_node, VFS_NODE_STATIC_TEXT, &static_text_ops, &namespace_banner_data);

    namespace_entries[0].path = "/dev/console";
    namespace_entries[0].node = &namespace_console_output_node;
    namespace_entries[1].path = "/etc/banner";
    namespace_entries[1].node = &namespace_banner_node;
    namespace_entries[2].path = 0;
    namespace_entries[2].node = 0;
    namespace_ready = 1;
}

static struct vfs_node *vfs_lookup_path(const char *path)
{
    size_t index;

    if (!namespace_ready || path == 0 || path[0] != '/') {
        return 0;
    }

    if (path[1] == 'd' &&
        path[2] == 'e' &&
        path[3] == 'v' &&
        path[4] == '/' &&
        path[5] == 's' &&
        path[6] == 't' &&
        path[7] == 'd' &&
        path[8] == 'i' &&
        path[9] == 'n' &&
        path[10] == '\0') {
        return &namespace_console_input_node;
    }

    for (index = 0u; namespace_entries[index].path != 0; ++index) {
        const char *entry_path;
        size_t char_index;

        entry_path = namespace_entries[index].path;
        char_index = 0u;
        while (entry_path[char_index] != '\0' && path[char_index] != '\0' &&
               entry_path[char_index] == path[char_index]) {
            ++char_index;
        }

        if (entry_path[char_index] == '\0' && path[char_index] == '\0') {
            return namespace_entries[index].node;
        }
    }

    return 0;
}

struct vfs_file *vfs_open_path(const char *path)
{
    struct vfs_node *node;

    node = vfs_lookup_path(path);
    if (node == 0) {
        return 0;
    }

    return vfs_file_open(node);
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

    file = vfs_open_path("/etc/banner");
    if (file == 0) {
        write_line("vfs fail");
        return;
    }

    ch = 0;
    if (vfs_file_read(file, &ch, 1u) != 1 || ch != 'p' || vfs_file_close(file) != 0) {
        write_line("vfs fail");
        return;
    }

    write_line("vfs ok");
}
