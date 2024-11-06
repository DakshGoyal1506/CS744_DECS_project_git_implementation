#ifndef FILE_METADATA_H
#define FILE_METADATA_H

#include <sys/stat.h>
#include "version_info.h"

typedef struct 
{
    char *filename;            // File path
    struct stat attributes;    // Permissions, size, timestamps
    int version_count;         // Number of versions
    VersionInfo *version_list; // Dynamic array of VersionInfo
} FileMetadata;

FileMetadata *create_file_metadata(const char *filename);
void destroy_file_metadata(FileMetadata *metadata);

#endif
