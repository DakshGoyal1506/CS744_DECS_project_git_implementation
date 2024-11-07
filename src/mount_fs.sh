#!/bin/bash

# Create mount point if it doesn't exist
MOUNT_POINT="../mnt"
if [ ! -d "$MOUNT_POINT" ]; then
    mkdir "$MOUNT_POINT"
fi

# Run the filesystem in the background
./myfs "$MOUNT_POINT" &
FS_PID=$!

echo "Filesystem mounted at $MOUNT_POINT with PID $FS_PID"
