#ifndef VERSION_MANAGER_H
#define VERSION_MANAGER_H

#include <stddef.h>

int save_version(const char *filename, const char *data, size_t size, int version_id);
char *load_version(const char *filename, int version_id, size_t *out_size);

#endif
