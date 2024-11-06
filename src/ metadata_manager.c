#include "metadata_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "cjson/cJSON.h"

#define METADATA_DIR ".metadata"

static int ensure_directory_exists(const char *path) 
{
    struct stat st = {0};
    if (stat(path, &st) == -1) 
    {
        return mkdir(path, 0755);
    }
    return 0;
}

int save_metadata(FileMetadata *metadata) 
{
    if (!metadata || !metadata->filename) return -1;

    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s.json", METADATA_DIR, metadata->filename);

    // Ensure the directory exists
    char dirpath[1024];
    strcpy(dirpath, filepath);
    char *last_slash = strrchr(dirpath, '/');
    if (last_slash) 
    {
        *last_slash = '\0'; // Remove the filename
        ensure_directory_exists(dirpath);
    }

    // Create JSON object
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "filename", metadata->filename);
    cJSON_AddNumberToObject(json, "version_count", metadata->version_count);

    // Add attributes
    cJSON *attr = cJSON_CreateObject();
    cJSON_AddNumberToObject(attr, "st_mode", metadata->attributes.st_mode);
    cJSON_AddNumberToObject(attr, "st_size", metadata->attributes.st_size);
    cJSON_AddNumberToObject(attr, "st_mtime", metadata->attributes.st_mtime);
    cJSON_AddItemToObject(json, "attributes", attr);

    // Add versions
    cJSON *versions = cJSON_CreateArray();
    for (int i = 0; i < metadata->version_count; i++) 
    {
        cJSON *ver = cJSON_CreateObject();
        cJSON_AddNumberToObject(ver, "version_id", metadata->version_list[i].version_id);
        cJSON_AddNumberToObject(ver, "timestamp", metadata->version_list[i].timestamp);
        cJSON_AddStringToObject(ver, "data_pointer", metadata->version_list[i].data_pointer);
        cJSON_AddItemToArray(versions, ver);
    }
    cJSON_AddItemToObject(json, "version_list", versions);

    // Write to file
    FILE *file = fopen(filepath, "w");
    if (!file) 
    {
        cJSON_Delete(json);
        return -1;
    }
    char *json_str = cJSON_Print(json);
    fputs(json_str, file);
    fclose(file);

    // Clean up
    free(json_str);
    cJSON_Delete(json);
    return 0;
}

FileMetadata *load_metadata(const char *filename) 
{
    if (!filename) return NULL;

    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s.json", METADATA_DIR, filename);

    FILE *file = fopen(filepath, "r");
    if (!file) return NULL;

    // Read file content
    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    rewind(file);
    char *content = malloc(filesize + 1);
    fread(content, 1, filesize, file);
    content[filesize] = '\0';
    fclose(file);

    // Parse JSON
    cJSON *json = cJSON_Parse(content);
    free(content);
    if (!json) return NULL;

    FileMetadata *metadata = create_file_metadata(filename);
    metadata->version_count = cJSON_GetObjectItem(json, "version_count")->valueint;

    // Load attributes
    cJSON *attr = cJSON_GetObjectItem(json, "attributes");
    metadata->attributes.st_mode = cJSON_GetObjectItem(attr, "st_mode")->valueint;
    metadata->attributes.st_size = cJSON_GetObjectItem(attr, "st_size")->valueint;
    metadata->attributes.st_mtime = cJSON_GetObjectItem(attr, "st_mtime")->valueint;

    // Load versions
    cJSON *versions = cJSON_GetObjectItem(json, "version_list");
    metadata->version_list = malloc(sizeof(VersionInfo) * metadata->version_count);
    int i = 0;
    cJSON *ver;
    cJSON_ArrayForEach(ver, versions) 
    {
        metadata->version_list[i].version_id = cJSON_GetObjectItem(ver, "version_id")->valueint;
        metadata->version_list[i].timestamp = cJSON_GetObjectItem(ver, "timestamp")->valueint;
        metadata->version_list[i].data_pointer = strdup(cJSON_GetObjectItem(ver, "data_pointer")->valuestring);
        i++;
    }

    cJSON_Delete(json);
    return metadata;
}
