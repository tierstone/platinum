#ifndef VFS_H
#define VFS_H

#include <stddef.h>
#include <stdint.h>

typedef enum vfs_node_kind {
    VFS_NODE_NONE = 0,
    VFS_NODE_PLACEHOLDER = 1,
    VFS_NODE_CONSOLE = 2,
    VFS_NODE_STATIC_TEXT = 3,
    VFS_NODE_EXECUTABLE = 4,
    VFS_NODE_DIRECTORY = 5
} vfs_node_kind_t;

enum {
    VFS_ACCESS_READ = 1u,
    VFS_ACCESS_WRITE = 2u
};

struct vfs_node;
struct vfs_file;

struct vfs_node_ops {
    int (*release)(struct vfs_node *node);
    int (*read)(struct vfs_file *file, void *buffer, size_t count);
    int (*write)(struct vfs_file *file, const void *buffer, size_t count);
};

struct vfs_node {
    vfs_node_kind_t kind;
    uint32_t refs;
    const struct vfs_node_ops *ops;
    void *data;
};

struct vfs_file {
    uint32_t refs;
    struct vfs_node *node;
    uint64_t offset;
    uint32_t access;
};

void vfs_node_initialize(
    struct vfs_node *node,
    vfs_node_kind_t kind,
    const struct vfs_node_ops *ops,
    void *data
);
void vfs_namespace_initialize(void);
struct vfs_file *vfs_open_path(const char *path, uint32_t flags);
struct vfs_file *vfs_open_exec_path(const char *path);
int vfs_file_executable_image(struct vfs_file *file, const uint8_t **image, size_t *size);
struct vfs_file *vfs_file_open(struct vfs_node *node, uint32_t access);
int vfs_file_retain(struct vfs_file *file);
int vfs_file_close(struct vfs_file *file);
int vfs_file_read(struct vfs_file *file, void *buffer, size_t count);
int vfs_file_write(struct vfs_file *file, const void *buffer, size_t count);
void vfs_self_test(void);

#endif
