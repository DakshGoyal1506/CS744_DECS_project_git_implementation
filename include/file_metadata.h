// include/file_metadata.h
#ifndef FILE_METADATA_H
#define FILE_METADATA_H

#include <sys/stat.h>
#include "version_info.h"

typedef struct {
    char *filename;
    struct stat attributes;
    int version_count;
    VersionInfo *version_list;
} FileMetadata;

FileMetadata *create_file_metadata(const char *filename);
void destroy_file_metadata(FileMetadata *metadata);

#endif // FILE_METADATA_H
