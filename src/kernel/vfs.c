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

struct vfs_executable_data {
    const uint8_t *image;
    size_t size;
};

struct vfs_namespace_entry {
    const char *name;
    struct vfs_node *node;
    const struct vfs_namespace_entry *children;
};

extern const uint8_t embedded_user_program_elf[];
extern const size_t embedded_user_program_elf_size;

#ifdef USER_TEST_BAD_EXEC_IMAGE
static const uint8_t bad_exec_image[] = {
    0x00u, 0x45u, 0x4cu, 0x46u
};
#endif

static struct vfs_console_input_data namespace_console_input_data;
static struct vfs_node namespace_root_node;
static struct vfs_node namespace_dev_node;
static struct vfs_node namespace_etc_node;
static struct vfs_node namespace_bin_node;
static struct vfs_node namespace_console_input_node;
static struct vfs_node namespace_console_output_node;
static struct vfs_static_text_data namespace_banner_data;
static struct vfs_node namespace_banner_node;
static struct vfs_executable_data namespace_pulse_data;
static struct vfs_node namespace_pulse_node;
static const struct vfs_namespace_entry namespace_dev_entries[2];
static const struct vfs_namespace_entry namespace_etc_entries[2];
static const struct vfs_namespace_entry namespace_bin_entries[2];
static const struct vfs_namespace_entry namespace_root_entries[4];
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

static int string_equal(const char *left, const char *right)
{
    size_t index;

    index = 0u;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) {
            return 0;
        }
        ++index;
    }

    return left[index] == '\0' && right[index] == '\0';
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

static int vfs_open_flags_valid(uint32_t access)
{
    return access == VFS_ACCESS_READ ||
        access == VFS_ACCESS_WRITE ||
        access == (VFS_ACCESS_READ | VFS_ACCESS_WRITE);
}

static struct vfs_file *vfs_make_file(struct vfs_node *node, uint32_t access)
{
    struct vfs_file *file;

    file = (struct vfs_file *)kzalloc(sizeof(*file));
    if (file == 0) {
        return 0;
    }

    file->refs = 1u;
    file->node = node;
    file->offset = 0u;
    file->access = access;
    ++node->refs;
    return file;
}

struct vfs_file *vfs_file_open(struct vfs_node *node, uint32_t access)
{
    if (node == 0 || node->ops == 0 || node->kind == VFS_NODE_DIRECTORY || !vfs_open_flags_valid(access)) {
        return 0;
    }

    if ((access & VFS_ACCESS_READ) != 0u && node->ops->read == 0) {
        return 0;
    }

    if ((access & VFS_ACCESS_WRITE) != 0u && node->ops->write == 0) {
        return 0;
    }

    return vfs_make_file(node, access);
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
    if ((file->access & VFS_ACCESS_READ) == 0u) {
        return -1;
    }

    return file->node->ops->read(file, buffer, count);
}

int vfs_file_write(struct vfs_file *file, const void *buffer, size_t count)
{
    if (file == 0 || file->node == 0 || file->node->ops == 0 || file->node->ops->write == 0) {
        return -1;
    }
    if ((file->access & VFS_ACCESS_WRITE) == 0u) {
        return -1;
    }

    return file->node->ops->write(file, buffer, count);
}

int vfs_file_executable_image(struct vfs_file *file, const uint8_t **image, size_t *size)
{
    const struct vfs_executable_data *data;

    if (file == 0 || file->node == 0 || file->node->kind != VFS_NODE_EXECUTABLE || image == 0 || size == 0) {
        return -1;
    }

    data = (const struct vfs_executable_data *)file->node->data;
    if (data == 0 || data->image == 0 || data->size == 0u) {
        return -1;
    }

    *image = data->image;
    *size = data->size;
    return 0;
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

static const struct vfs_namespace_entry *vfs_find_child(
    const struct vfs_namespace_entry *entries,
    const char *name
)
{
    size_t index;

    if (entries == 0 || name == 0) {
        return 0;
    }

    for (index = 0u; entries[index].name != 0; ++index) {
        if (string_equal(entries[index].name, name)) {
            return &entries[index];
        }
    }

    return 0;
}

static struct vfs_node *vfs_walk_path(const char *path)
{
    char component[16];
    const struct vfs_namespace_entry *entries;
    const struct vfs_namespace_entry *entry;
    size_t path_index;
    size_t component_index;

    if (!namespace_ready || path == 0 || path[0] != '/') {
        return 0;
    }

    if (path[1] == '\0') {
        return 0;
    }

    entries = namespace_root_entries;
    path_index = 1u;

    for (;;) {
        component_index = 0u;
        while (path[path_index] != '\0' && path[path_index] != '/') {
            if (component_index + 1u >= sizeof(component)) {
                return 0;
            }
            component[component_index] = path[path_index];
            ++component_index;
            ++path_index;
        }

        if (component_index == 0u) {
            return 0;
        }

        component[component_index] = '\0';
        entry = vfs_find_child(entries, component);
        if (entry == 0) {
            return 0;
        }

        if (path[path_index] == '\0') {
            return entry->node;
        }

        if (entry->children == 0) {
            return 0;
        }

        ++path_index;
        if (path[path_index] == '\0') {
            return 0;
        }

        entries = entry->children;
    }
}

static const struct vfs_namespace_entry namespace_dev_entries[2] = {
    { "console", &namespace_console_output_node, 0 },
    { 0, 0, 0 }
};

static const struct vfs_namespace_entry namespace_etc_entries[2] = {
    { "banner", &namespace_banner_node, 0 },
    { 0, 0, 0 }
};

static const struct vfs_namespace_entry namespace_bin_entries[2] = {
    { "pulse", &namespace_pulse_node, 0 },
    { 0, 0, 0 }
};

static const struct vfs_namespace_entry namespace_root_entries[4] = {
    { "dev", &namespace_dev_node, namespace_dev_entries },
    { "etc", &namespace_etc_node, namespace_etc_entries },
    { "bin", &namespace_bin_node, namespace_bin_entries },
    { 0, 0, 0 }
};

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
    static const struct vfs_node_ops executable_ops = {
        vfs_static_release,
        0,
        0
    };
    static const struct vfs_node_ops directory_ops = {
        vfs_static_release,
        0,
        0
    };

    namespace_console_input_data.byte = '@';
    namespace_console_input_data.used = 0u;
    namespace_banner_data.text = "platinum\n";
    namespace_banner_data.length = 9u;
#ifdef USER_TEST_BAD_EXEC_IMAGE
    namespace_pulse_data.image = bad_exec_image;
    namespace_pulse_data.size = sizeof(bad_exec_image);
#else
    namespace_pulse_data.image = embedded_user_program_elf;
    namespace_pulse_data.size = embedded_user_program_elf_size;
#endif

    vfs_node_initialize(&namespace_root_node, VFS_NODE_DIRECTORY, &directory_ops, 0);
    vfs_node_initialize(&namespace_dev_node, VFS_NODE_DIRECTORY, &directory_ops, 0);
    vfs_node_initialize(&namespace_etc_node, VFS_NODE_DIRECTORY, &directory_ops, 0);
    vfs_node_initialize(&namespace_bin_node, VFS_NODE_DIRECTORY, &directory_ops, 0);
    vfs_node_initialize(&namespace_console_input_node, VFS_NODE_CONSOLE, &console_input_ops, &namespace_console_input_data);
    vfs_node_initialize(&namespace_console_output_node, VFS_NODE_CONSOLE, &console_output_ops, 0);
    vfs_node_initialize(&namespace_banner_node, VFS_NODE_STATIC_TEXT, &static_text_ops, &namespace_banner_data);
    vfs_node_initialize(&namespace_pulse_node, VFS_NODE_EXECUTABLE, &executable_ops, &namespace_pulse_data);
    namespace_ready = 1;
}

struct vfs_file *vfs_open_path(const char *path, uint32_t flags)
{
    struct vfs_node *node;

    node = vfs_walk_path(path);
    if (node == 0) {
        return 0;
    }

    return vfs_file_open(node, flags);
}

struct vfs_file *vfs_open_exec_path(const char *path)
{
    struct vfs_node *node;

    node = vfs_walk_path(path);
    if (node == 0 || node->kind != VFS_NODE_EXECUTABLE) {
        return 0;
    }

    return vfs_make_file(node, 0u);
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

    file = vfs_file_open(node, VFS_ACCESS_READ | VFS_ACCESS_WRITE);
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

    file = vfs_open_path("/etc/banner", VFS_ACCESS_READ);
    if (file == 0) {
        write_line("vfs fail");
        return;
    }

    ch = 0;
    if (vfs_file_read(file, &ch, 1u) != 1 || ch != 'p' || vfs_file_close(file) != 0) {
        write_line("vfs fail");
        return;
    }

    file = vfs_open_path("/dev/console", VFS_ACCESS_WRITE);
    if (file == 0) {
        write_line("vfs fail");
        return;
    }

    if (vfs_file_write(file, "", 0u) != 0 || vfs_file_close(file) != 0) {
        write_line("vfs fail");
        return;
    }

    file = vfs_open_exec_path("/bin/pulse");
    if (file == 0) {
        write_line("vfs fail");
        return;
    }

    {
        const uint8_t *image;
        size_t size;

        if (vfs_file_executable_image(file, &image, &size) != 0 || image == 0 || size == 0u || vfs_file_close(file) != 0) {
            write_line("vfs fail");
            return;
        }
    }

    if (vfs_open_path("/dev", VFS_ACCESS_READ) != 0 ||
        vfs_open_path("etc/banner", VFS_ACCESS_READ) != 0 ||
        vfs_open_path("/etc/missing", VFS_ACCESS_READ) != 0 ||
        vfs_open_path("/etc/banner", VFS_ACCESS_WRITE) != 0 ||
        vfs_open_path("/dev/console", VFS_ACCESS_READ) != 0 ||
        vfs_open_exec_path("/etc/banner") != 0) {
        write_line("vfs fail");
        return;
    }

    write_line("vfs ok");
}
