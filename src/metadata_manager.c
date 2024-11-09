#include "metadata_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "cJSON.h"

#define METADATA_DIR ".metadata"

int ensure_directory_exists(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) != 0)
            return -1;
    }
    return 0;
}

int save_metadata(FileMetadata *metadata) {
    if (!metadata || !metadata->filename) return -1;

    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s.json", METADATA_DIR, metadata->filename);

    if (ensure_directory_exists(METADATA_DIR) != 0)
        return -1;

    cJSON *json = cJSON_CreateObject();
    if (!json)
        return -1;

    cJSON_AddStringToObject(json, "filename", metadata->filename);
    cJSON_AddNumberToObject(json, "version_count", metadata->version_count);

    cJSON *attr = cJSON_CreateObject();
    if (!attr) {
        cJSON_Delete(json);
        return -1;
    }
    cJSON_AddNumberToObject(attr, "st_mode", metadata->attributes.st_mode);
    cJSON_AddNumberToObject(attr, "st_size", metadata->attributes.st_size);
    cJSON_AddNumberToObject(attr, "st_mtime", metadata->attributes.st_mtime);
    cJSON_AddItemToObject(json, "attributes", attr);

    cJSON *versions = cJSON_CreateArray();
    if (!versions) {
        cJSON_Delete(json);
        return -1;
    }
    for (int i = 0; i < metadata->version_count; i++) {
        cJSON *ver = cJSON_CreateObject();
        if (!ver)
            continue;
        cJSON_AddNumberToObject(ver, "version_id", metadata->version_list[i].version_id);
        cJSON_AddNumberToObject(ver, "timestamp", metadata->version_list[i].timestamp);
        cJSON_AddStringToObject(ver, "data_pointer", metadata->version_list[i].data_pointer);
        cJSON_AddItemToArray(versions, ver);
    }
    cJSON_AddItemToObject(json, "version_list", versions);

    FILE *file = fopen(filepath, "w");
    if (!file) {
        cJSON_Delete(json);
        return -1;
    }
    char *json_str = cJSON_Print(json);
    if (!json_str) {
        fclose(file);
        cJSON_Delete(json);
        return -1;
    }
    fputs(json_str, file);
    fclose(file);

    free(json_str);
    cJSON_Delete(json);
    return 0;
}

FileMetadata *load_metadata(const char *filename) {
    if (!filename) return NULL;

    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s.json", METADATA_DIR, filename);

    FILE *file = fopen(filepath, "r");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    rewind(file);
    char *content = malloc(filesize + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }
    size_t read_size = fread(content, 1, filesize, file);
    content[read_size] = '\0';
    fclose(file);

    cJSON *json = cJSON_Parse(content);
    free(content);
    if (!json) return NULL;

    FileMetadata *metadata = create_file_metadata(filename);
    if (!metadata) {
        cJSON_Delete(json);
        return NULL;
    }

    cJSON *version_count_item = cJSON_GetObjectItemCaseSensitive(json, "version_count");
    if (cJSON_IsNumber(version_count_item))
        metadata->version_count = version_count_item->valueint;

    cJSON *attr = cJSON_GetObjectItemCaseSensitive(json, "attributes");
    if (cJSON_IsObject(attr)) {
        cJSON *st_mode_item = cJSON_GetObjectItemCaseSensitive(attr, "st_mode");
        cJSON *st_size_item = cJSON_GetObjectItemCaseSensitive(attr, "st_size");
        cJSON *st_mtime_item = cJSON_GetObjectItemCaseSensitive(attr, "st_mtime");

        if (cJSON_IsNumber(st_mode_item))
            metadata->attributes.st_mode = (mode_t) st_mode_item->valueint;
        if (cJSON_IsNumber(st_size_item))
            metadata->attributes.st_size = (off_t) st_size_item->valueint;
        if (cJSON_IsNumber(st_mtime_item))
            metadata->attributes.st_mtime = (time_t) st_mtime_item->valueint;
    }

    cJSON *versions = cJSON_GetObjectItemCaseSensitive(json, "version_list");
    if (cJSON_IsArray(versions)) {
        metadata->version_list = malloc(sizeof(VersionInfo) * metadata->version_count);
        if (!metadata->version_list) {
            destroy_file_metadata(metadata);
            cJSON_Delete(json);
            return NULL;
        }

        int i = 0;
        cJSON *ver;
        cJSON_ArrayForEach(ver, versions) {
            if (i >= metadata->version_count)
                break;

            cJSON *version_id_item = cJSON_GetObjectItemCaseSensitive(ver, "version_id");
            cJSON *timestamp_item = cJSON_GetObjectItemCaseSensitive(ver, "timestamp");
            cJSON *data_pointer_item = cJSON_GetObjectItemCaseSensitive(ver, "data_pointer");

            if (cJSON_IsNumber(version_id_item))
                metadata->version_list[i].version_id = version_id_item->valueint;
            if (cJSON_IsNumber(timestamp_item))
                metadata->version_list[i].timestamp = (time_t) timestamp_item->valuedouble;
            if (cJSON_IsString(data_pointer_item))
                metadata->version_list[i].data_pointer = strdup(data_pointer_item->valuestring);
            i++;
        }
    }

    cJSON_Delete(json);
    return metadata;
}
