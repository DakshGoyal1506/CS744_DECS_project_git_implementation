#ifndef METADATA_MANAGER_H
#define METADATA_MANAGER_H

#include "file_metadata.h"

int save_metadata(FileMetadata *metadata);
FileMetadata *load_metadata(const char *filename);
int ensure_directory_exists(const char *path);

#endif