#!/bin/bash

MOUNT_POINT="../mnt"

# Unmount the filesystem
fusermount -u "$MOUNT_POINT"

echo "Filesystem unmounted from $MOUNT_POINT"
