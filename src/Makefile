# Makefile
CC = gcc
CFLAGS = -Wall -D_FILE_OFFSET_BITS=64 `pkg-config fuse3 --cflags` -I.
LDFLAGS = `pkg-config fuse3 --libs` -lcjson
TARGET = myfs
SRCS = main.c file_metadata.c version_info.c metadata_manager.c version_manager.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(OBJS) $(TARGET)