#include "file_metadata.h"
#include <stdlib.h>
#include <string.h>

FileMetadata *create_file_metadata(const char *filename) 
{
    FileMetadata *metadata = malloc(sizeof(FileMetadata));
    if (!metadata) return NULL;

    metadata->filename = strdup(filename);
    metadata->version_count = 0;
    metadata->version_list = NULL; // No versions yet
    memset(&metadata->attributes, 0, sizeof(struct stat));
    return metadata;
}

void destroy_file_metadata(FileMetadata *metadata) 
{
    if (metadata) 
    {
        free(metadata->filename);
        if (metadata->version_list) 
        {
            for (int i = 0; i < metadata->version_count; i++) 
            {
                destroy_version_info(&metadata->version_list[i]);
            }
            free(metadata->version_list);
        }
        free(metadata);
    }
}
