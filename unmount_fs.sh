#!/bin/bash

MOUNT_POINT="mnt"

# Read the process ID
if [ -f fs.pid ]; then
    FS_PID=$(cat fs.pid)
    # Kill the filesystem process
    kill -TERM $FS_PID
    rm fs.pid
    echo "Filesystem process $FS_PID terminated."
else
    echo "Filesystem PID file not found."
fi

# Unmount the filesystem
fusermount3 -u "$MOUNT_POINT"
echo "Filesystem unmounted from $MOUNT_POINT"
