#include "version_info.h"
#include <stdlib.h>

void destroy_version_info(VersionInfo *version) 
{
    if (version) 
    {
        free(version->data_pointer);
    }
}
