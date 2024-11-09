#include "file_metadata.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

FileMetadata *create_file_metadata(const char *filename) {
    if (!filename) return NULL;

    FileMetadata *metadata = malloc(sizeof(FileMetadata));
    if (!metadata) return NULL;

    metadata->filename = strdup(filename);
    if (!metadata->filename) {
        free(metadata);
        return NULL;
    }

    memset(&metadata->attributes, 0, sizeof(struct stat));
    metadata->attributes.st_mode = S_IFREG | 0644;
    metadata->attributes.st_nlink = 1;
    metadata->attributes.st_size = 0;
    metadata->attributes.st_mtime = time(NULL);

    metadata->version_count = 0;
    metadata->version_list = NULL;

    return metadata;
}

void destroy_file_metadata(FileMetadata *metadata) {
    if (metadata) {
        free(metadata->filename);
        if (metadata->version_list) {
            for (int i = 0; i < metadata->version_count; i++) {
                destroy_version_info(&metadata->version_list[i]);
            }
            free(metadata->version_list);
        }
        free(metadata);
    }
}
