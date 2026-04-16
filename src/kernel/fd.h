#ifndef FD_H
#define FD_H

#include <stddef.h>
#include <stdint.h>

enum {
    fd_table_capacity = 16
};

typedef enum fd_kind {
    FD_KIND_NONE = 0,
    FD_KIND_PLACEHOLDER = 1,
    FD_KIND_CONSOLE = 2
} fd_kind_t;

struct fd_ops {
    int (*retain)(void *object);
    int (*close)(void *object);
    int (*read)(void *object, void *buffer, size_t count);
    int (*write)(void *object, const void *buffer, size_t count);
};

struct fd_entry {
    uint32_t used;
    fd_kind_t kind;
    void *object;
    const struct fd_ops *ops;
};

struct fd_table {
    struct fd_entry entries[fd_table_capacity];
};

void fd_table_initialize(struct fd_table *table);
int fd_table_install(struct fd_table *table, fd_kind_t kind, void *object, const struct fd_ops *ops);
const struct fd_entry *fd_table_get(const struct fd_table *table, int fd);
int fd_table_dup(struct fd_table *table, int fd);
int fd_table_read(struct fd_table *table, int fd, void *buffer, size_t count);
int fd_table_write(struct fd_table *table, int fd, const void *buffer, size_t count);
int fd_table_close(struct fd_table *table, int fd);
void fd_table_close_all(struct fd_table *table);
int fd_table_seed_console(struct fd_table *table);
void fd_self_test(void);

#endif
