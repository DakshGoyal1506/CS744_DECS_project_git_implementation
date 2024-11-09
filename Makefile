# Makefile

CC = gcc
CFLAGS = -Wall -Wextra -Iinclude `pkg-config fuse3 cjson --cflags` -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=31
LDFLAGS = `pkg-config fuse3 cjson --libs`

SRC = src/main.c src/file_metadata.c src/version_info.c src/metadata_manager.c src/version_manager.c
OBJ = $(SRC:.c=.o)
TARGET = myfs

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
