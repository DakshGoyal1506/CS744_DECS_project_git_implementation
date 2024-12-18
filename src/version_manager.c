#include "version_manager.h"
#include "metadata_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define VERSIONS_DIR ".versions"

int save_version(const char *filename, const char *data, size_t size, int version_id) {
    if (!filename || !data) return -1;

    char dirpath[512];
    snprintf(dirpath, sizeof(dirpath), "%s/%s", VERSIONS_DIR, filename);
    if (ensure_directory_exists(dirpath) != 0)
        return -1;

    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/version_%d", dirpath, version_id);

    FILE *file = fopen(filepath, "wb");
    if (!file) return -1;

    size_t written = fwrite(data, 1, size, file);
    fclose(file);

    return written == size ? 0 : -1;
}

char *load_version(const char *filename, int version_id, size_t *out_size) {
    if (!filename || !out_size) return NULL;

    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s/version_%d", VERSIONS_DIR, filename, version_id);

    FILE *file = fopen(filepath, "rb");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    rewind(file);

    char *data = malloc(filesize);
    if (!data) {
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(data, 1, filesize, file);
    fclose(file);

    if (read_size != filesize) {
        free(data);
        return NULL;
    }

    *out_size = filesize;
    return data;
}
