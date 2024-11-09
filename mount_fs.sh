#!/bin/bash

MOUNT_POINT="mnt"

# Create mount point if it doesn't exist
if [ ! -d "$MOUNT_POINT" ]; then
    mkdir "$MOUNT_POINT"
fi

# Run the filesystem in the background
./myfs "$MOUNT_POINT" -f &
FS_PID=$!

echo "Filesystem mounted at $MOUNT_POINT with PID $FS_PID"
echo $FS_PID > fs.pid
