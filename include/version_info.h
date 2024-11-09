#ifndef VERSION_INFO_H
#define VERSION_INFO_H

#include <time.h>

typedef struct {
    int version_id;
    time_t timestamp;
    char *data_pointer;
} VersionInfo;

void destroy_version_info(VersionInfo *version);

#endif // VERSION_INFO_H
