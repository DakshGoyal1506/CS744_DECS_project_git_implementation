#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include "file_metadata.h"
#include "metadata_manager.h"
#include "version_manager.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#define METADATA_DIR ".metadata"
#define VERSIONS_DIR ".versions"

// Function declarations
static int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
                     off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);
static int fs_open(const char *path, struct fuse_file_info *fi);
static int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
static int fs_unlink(const char *path);

// Define FUSE operations
static struct fuse_operations fs_operations = 
{
    .getattr    = fs_getattr,
    .readdir    = fs_readdir,
    .open       = fs_open,
    .read       = fs_read,
    .write      = fs_write,
    .create     = fs_create,
    .unlink     = fs_unlink,
};

// Implementation of fs_getattr
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

// Implementation of fs_readdir
static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
                     off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
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
    d = opendir(METADATA_DIR);
    if (d) 
    {
        while ((dir = readdir(d)) != NULL) 
        {
            if (dir->d_type == DT_REG) // Only regular files
            {
                // Remove .json extension
                size_t len = strlen(dir->d_name);
                if (len > 5 && strcmp(dir->d_name + len - 5, ".json") == 0)
                {
                    char filename[256];
                    strncpy(filename, dir->d_name, len - 5);
                    filename[len - 5] = '\0';
                    filler(buf, filename, NULL, 0, 0);
                }
            }
        }
        closedir(d);
    }
    return 0;
}

// Implementation of fs_open
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

// Implementation of fs_read
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
        return 0;
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
    }
    else
    {
        size = 0;
    }

    free(data);
    destroy_file_metadata(metadata);
    return size;
}

// Implementation of fs_write
static int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    (void) fi;
    (void) offset;
    FileMetadata *metadata = load_metadata(path + 1);
    if (!metadata) 
    {
        // Create new metadata if it doesn't exist
        metadata = create_file_metadata(path + 1);
        if (!metadata)
            return -ENOMEM;
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
    VersionInfo *new_version = malloc(sizeof(VersionInfo));
    if (!new_version) 
    {
        destroy_file_metadata(metadata);
        return -ENOMEM;
    }
    new_version->version_id = new_version_id;
    new_version->timestamp = time(NULL);
    new_version->data_pointer = strdup(buf); // Assuming data_pointer holds data as string

    VersionInfo *temp = realloc(metadata->version_list, sizeof(VersionInfo) * new_version_id);
    if (!temp) 
    {
        destroy_version_info(new_version);
        destroy_file_metadata(metadata);
        return -ENOMEM;
    }
    metadata->version_list = temp;
    metadata->version_list[new_version_id - 1] = *new_version;
    metadata->version_count = new_version_id;

    // Save updated metadata
    if (save_metadata(metadata) != 0) 
    {
        destroy_file_metadata(metadata);
        return -EIO;
    }

    destroy_file_metadata(metadata);
    return size;
}

// Implementation of fs_create
static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    (void) fi;
    FileMetadata *metadata = create_file_metadata(path + 1);
    if (!metadata)
        return -ENOMEM;
    
    metadata->attributes.st_mode = S_IFREG | mode;
    metadata->attributes.st_nlink = 1;
    metadata->attributes.st_size = 0;
    metadata->version_count = 0;
    metadata->version_list = NULL;

    if (save_metadata(metadata) != 0) 
    {
        destroy_file_metadata(metadata);
        return -EIO;
    }

    destroy_file_metadata(metadata);
    return 0;
}

// Implementation of fs_unlink
static int fs_unlink(const char *path) 
{
    // Remove metadata file
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s.json", METADATA_DIR, path + 1);
    if (unlink(filepath) != 0)
        return -errno;

    // Remove version directory
    char dirpath[1024];
    snprintf(dirpath, sizeof(dirpath), "%s/%s", VERSIONS_DIR, path + 1);
    if (rmdir(dirpath) != 0 && errno != ENOENT)
        return -errno;

    return 0;
}

int main(int argc, char *argv[]) 
{
    // Ensure metadata and versions directories exist
    if (ensure_directory_exists(METADATA_DIR) != 0) 
    {
        fprintf(stderr, "Failed to create metadata directory.\n");
        return 1;
    }
    if (ensure_directory_exists(VERSIONS_DIR) != 0) 
    {
        fprintf(stderr, "Failed to create versions directory.\n");
        return 1;
    }

    return fuse_main(argc, argv, &fs_operations, NULL);
}