#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>

#include "file_metadata.h"
#include "metadata_manager.h"
#include "version_manager.h"


#define METADATA_DIR ".metadata"
#define VERSIONS_DIR ".versions"

static int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));

    // Root directory
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    // Check if it's a directory
    char dirpath[1024];
    snprintf(dirpath, sizeof(dirpath), ".metadata%s", path);
    struct stat st;
    if (stat(dirpath, &st) == 0 && S_ISDIR(st.st_mode)) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    // Load metadata for files
    FileMetadata *metadata = load_metadata(path + 1); // Skip leading '/'
    if (!metadata) {
        return -ENOENT;
    }

    // Set file attributes
    *stbuf = metadata->attributes;
    destroy_file_metadata(metadata);
    return 0;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    // Construct the directory path in .metadata
    char dirpath[1024];
    snprintf(dirpath, sizeof(dirpath), ".metadata%s", path);

    DIR *d;
    struct dirent *dir;
    d = opendir(dirpath);
    if (!d)
        return -ENOENT;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    while ((dir = readdir(d)) != NULL) {
        if (dir->d_name[0] == '.')
            continue; // Skip hidden files

        // Remove '.json' extension if it's a file
        char filename[256];
        if (dir->d_type == DT_REG) {
            size_t len = strlen(dir->d_name);
            if (len > 5 && strcmp(dir->d_name + len - 5, ".json") == 0) {
                strncpy(filename, dir->d_name, len - 5);
                filename[len - 5] = '\0';
                filler(buf, filename, NULL, 0, 0);
            }
        } else if (dir->d_type == DT_DIR) {
            // Directory
            filler(buf, dir->d_name, NULL, 0, 0);
        }
    }
    closedir(d);
    return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
    FileMetadata *metadata = load_metadata(path + 1);
    if (!metadata) {
        return -ENOENT;
    }

    // Check permissions
    if ((fi->flags & O_ACCMODE) != O_RDONLY && !(metadata->attributes.st_mode & 0222)) {
        destroy_file_metadata(metadata);
        return -EACCES;
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

static int fs_write(const char *path, const char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
    (void) fi;
    FileMetadata *metadata = load_metadata(path + 1);
    char *new_data = NULL;
    size_t new_size = 0;

    if (!metadata) {
        // Create new metadata
        metadata = create_file_metadata(path + 1);
        metadata->attributes.st_mode = S_IFREG | 0644;
        metadata->attributes.st_nlink = 1;
        metadata->attributes.st_size = 0;
    }

    // Load existing data if any
    if (metadata->version_count > 0) {
        VersionInfo *latest_version = &metadata->version_list[metadata->version_count - 1];
        size_t existing_size;
        char *existing_data = load_version(path + 1, latest_version->version_id, &existing_size);

        // Adjust the size if offset + size exceeds current size
        new_size = offset + size > existing_size ? offset + size : existing_size;
        new_data = malloc(new_size);
        memset(new_data, 0, new_size);

        // Copy existing data
        memcpy(new_data, existing_data, existing_size);
        free(existing_data);
    } else {
        new_size = offset + size;
        new_data = malloc(new_size);
        memset(new_data, 0, new_size);
    }

    // Write new data at the given offset
    memcpy(new_data + offset, buf, size);

    // Save new version
    int new_version_id = metadata->version_count + 1;
    if (save_version(path + 1, new_data, new_size, new_version_id) != 0) {
        free(new_data);
        destroy_file_metadata(metadata);
        return -EIO;
    }

    // Update metadata
    metadata->attributes.st_size = new_size;
    metadata->attributes.st_mtime = time(NULL);
    metadata->version_list = realloc(metadata->version_list, sizeof(VersionInfo) * new_version_id);
    metadata->version_count = new_version_id;
    VersionInfo *new_version = &metadata->version_list[new_version_id - 1];
    new_version->version_id = new_version_id;
    new_version->timestamp = time(NULL);
    new_version->data_pointer = malloc(256);
    snprintf(new_version->data_pointer, 256, ".versions/%s/version_%d", path + 1, new_version_id);

    if (save_metadata(metadata) != 0) {
        free(new_data);
        destroy_file_metadata(metadata);
        return -EIO;
    }

    free(new_data);
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

static int fs_mkdir(const char *path, mode_t mode) {
    // Create directory in .metadata
    char dirpath[1024];
    snprintf(dirpath, sizeof(dirpath), ".metadata%s", path);
    if (mkdir(dirpath, mode) != 0) {
        return -EIO;
    }

    // Create directory in .versions
    snprintf(dirpath, sizeof(dirpath), ".versions%s", path);
    if (mkdir(dirpath, mode) != 0) {
        return -EIO;
    }

    return 0;
}

static int fs_rmdir(const char *path) {
    // Remove directory from .metadata
    char dirpath[1024];
    snprintf(dirpath, sizeof(dirpath), ".metadata%s", path);
    if (rmdir(dirpath) != 0) {
        return -EIO;
    }

    // Remove directory from .versions
    snprintf(dirpath, sizeof(dirpath), ".versions%s", path);
    if (rmdir(dirpath) != 0) {
        return -EIO;
    }

    return 0;
}

static struct fuse_operations fs_operations = 
{
    .getattr    = fs_getattr,
    .readdir    = fs_readdir,
    .open       = fs_open,
    .read       = fs_read,
    .write      = fs_write,
    .create     = fs_create,
    .unlink     = fs_unlink,
    .mkdir      = fs_mkdir,
    .rmdir      = fs_rmdir,
};


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