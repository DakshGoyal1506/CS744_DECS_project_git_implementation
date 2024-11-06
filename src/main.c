#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <fuse/fuse_common.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "file_metadata.h"
#include "metadata_manager.h"
#include "version_manager.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

static int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) 
{
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));

    // Root directory
    if (strcmp(path, "/") == 0) 
    {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    // Load metadata
    FileMetadata *metadata = load_metadata(path + 1); // Skip leading '/'
    if (!metadata) 
    {
        return -ENOENT;
    }

    *stbuf = metadata->attributes;
    destroy_file_metadata(metadata);
    return 0;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) 
{
    (void) offset;
    (void) fi;
    (void) flags;

    // Only root directory is supported
    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    // List files based on metadata in .metadata/
    DIR *d;
    struct dirent *dir;
    d = opendir(".metadata");
    if (d) 
    {
        while ((dir = readdir(d)) != NULL) 
        {
            if (dir->d_type == DT_REG) 
            {
                // Remove '.json' extension
                char filename[256];
                strncpy(filename, dir->d_name, strlen(dir->d_name) - 5);
                filename[strlen(dir->d_name) - 5] = '\0';
                filler(buf, filename, NULL, 0, 0);
            }
        }
        closedir(d);
    }
    return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) 
{
    FileMetadata *metadata = load_metadata(path + 1);
    if (!metadata) 
    {
        return -ENOENT;
    }
    destroy_file_metadata(metadata);
    return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) 
{
    (void) fi;
    size_t len;
    size_t out_size;
    FileMetadata *metadata = load_metadata(path + 1);
    if (!metadata) 
    {
        return -ENOENT;
    }
    if (metadata->version_count == 0) 
    {
        destroy_file_metadata(metadata);
        return -ENOENT;
    }

    // Read the latest version
    VersionInfo *latest_version = &metadata->version_list[metadata->version_count - 1];
    char *data = load_version(path + 1, latest_version->version_id, &out_size);
    if (!data) 
    {
        destroy_file_metadata(metadata);
        return -EIO;
    }
    len = out_size;
    if (offset < len) 
    {
        if (offset + size > len)
            size = len - offset;
        memcpy(buf, data + offset, size);
    } else
        size = 0;

    free(data);
    destroy_file_metadata(metadata);
    return size;
}

static int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) 
{
    (void) fi;
    (void) offset;
    FileMetadata *metadata = load_metadata(path + 1);
    if (!metadata) 
    {
        // Create new metadata
        metadata = create_file_metadata(path + 1);
        metadata->attributes.st_mode = S_IFREG | 0644;
        metadata->attributes.st_nlink = 1;
        metadata->attributes.st_size = 0;
    }
    metadata->attributes.st_size = size;
    metadata->attributes.st_mtime = time(NULL);

    // Save new version
    int new_version_id = metadata->version_count + 1;
    if (save_version(path + 1, buf, size, new_version_id) != 0) 
    {
        destroy_file_metadata(metadata);
        return -EIO;
    }

    // Update metadata
    metadata->version_list = realloc(metadata->version_list, sizeof(VersionInfo) * new_version_id);
    metadata->version_count = new_version_id;
    VersionInfo *new_version = &metadata->version_list[new_version_id - 1];
    new_version->version_id = new_version_id;
    new_version->timestamp = time(NULL);
    new_version->data_pointer = malloc(256);
    snprintf(new_version->data_pointer, 256, ".versions/%s/version_%d", path + 1, new_version_id);

    if (save_metadata(metadata) != 0) 
    {
        destroy_file_metadata(metadata);
        return -EIO;
    }
    destroy_file_metadata(metadata);
    return size;
}

static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi) 
{
    (void) fi;
    FileMetadata *metadata = create_file_metadata(path + 1);
    if (!metadata) return -ENOMEM;
    metadata->attributes.st_mode = S_IFREG | mode;
    metadata->attributes.st_nlink = 1;
    metadata->attributes.st_size = 0;
    metadata->attributes.st_mtime = time(NULL);
    if (save_metadata(metadata) != 0) 
    {
        destroy_file_metadata(metadata);
        return -EIO;
    }
    destroy_file_metadata(metadata);
    return 0;
}

static int fs_unlink(const char *path) 
{
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/%s.json", METADATA_DIR, path + 1);
    if (unlink(filepath) != 0) 
    {
        return -ENOENT;
    }

    // Remove version files
    char dirpath[256];
    snprintf(dirpath, sizeof(dirpath), "%s/%s", VERSIONS_DIR, path + 1);
    char command[512];
    snprintf(command, sizeof(command), "rm -rf \"%s\"", dirpath);
    system(command);
    return 0;
}

static struct fuse_operations fs_operations = 
{
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .open = fs_open,
    .read = fs_read,
    .write = fs_write,
    .create = fs_create,
    .unlink = fs_unlink,
};

int main(int argc, char *argv[]) 
{
    mkdir(".metadata", 0755);
    mkdir(".versions", 0755);
    return fuse_main(argc, argv, &fs_operations, NULL);
}
