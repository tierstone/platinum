#ifndef EMBEDDED_USER_PROGRAMS_H
#define EMBEDDED_USER_PROGRAMS_H

#include <stddef.h>
#include <stdint.h>

struct embedded_user_program {
    const char *name;
    const uint8_t *image;
    size_t size;
};

extern const struct embedded_user_program embedded_user_program_registry[];
extern const size_t embedded_user_program_registry_count;
extern const uint8_t *embedded_first_user_program_image;
extern const size_t embedded_first_user_program_size;

#endif
