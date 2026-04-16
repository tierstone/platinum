#ifndef VFS_H
#define VFS_H

#include <stddef.h>
#include <stdint.h>

typedef enum vfs_node_kind {
    VFS_NODE_NONE = 0,
    VFS_NODE_PLACEHOLDER = 1,
    VFS_NODE_CONSOLE = 2
} vfs_node_kind_t;

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
};

void vfs_node_initialize(
    struct vfs_node *node,
    vfs_node_kind_t kind,
    const struct vfs_node_ops *ops,
    void *data
);
struct vfs_file *vfs_file_open(struct vfs_node *node);
int vfs_file_retain(struct vfs_file *file);
int vfs_file_close(struct vfs_file *file);
int vfs_file_read(struct vfs_file *file, void *buffer, size_t count);
int vfs_file_write(struct vfs_file *file, const void *buffer, size_t count);
void vfs_self_test(void);

#endif
