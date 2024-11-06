#ifndef VERSION_INFO_H
#define VERSION_INFO_H

#include <time.h>

typedef struct 
{
    int version_id;     // Version number
    time_t timestamp;   // Creation time
    char *data_pointer; // Path to versioned data
} VersionInfo;

void destroy_version_info(VersionInfo *version);

#endif
