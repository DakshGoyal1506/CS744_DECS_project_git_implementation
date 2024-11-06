#include "version_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define VERSIONS_DIR ".versions"

static int ensure_directory_exists(const char *path) 
{
    struct stat st = {0};
    if (stat(path, &st) == -1) 
    {
        return mkdir(path, 0755);
    }
    return 0;
}

int save_version(const char *filename, const char *data, size_t size, int version_id) 
{
    if (!filename || !data) return -1;

    char dirpath[1024];
    snprintf(dirpath, sizeof(dirpath), "%s/%s", VERSIONS_DIR, filename);
    ensure_directory_exists(dirpath);

    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/version_%d", dirpath, version_id);

    FILE *file = fopen(filepath, "w");
    if (!file) return -1;

    fwrite(data, 1, size, file);
    fclose(file);
    return 0;
}

char *load_version(const char *filename, int version_id, size_t *out_size) 
{
    if (!filename || !out_size) return NULL;

    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s/version_%d", VERSIONS_DIR, filename, version_id);

    FILE *file = fopen(filepath, "r");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    rewind(file);

    char *data = malloc(filesize);
    fread(data, 1, filesize, file);
    fclose(file);

    *out_size = filesize;
    return data;
}
