# Makefile
FEATURE_TEST_MACROS = -D_XOPEN_SOURCE=700
FEATURE_TEST_MACROS += -D_POSIX_C_SOURCE=200809L
FEATURE_TEST_MACROS += -D_GNU_SOURCE

CC = gcc
CFLAGS = -Wall -D_FILE_OFFSET_BITS=64 $(FEATURE_TEST_MACROS) `pkg-config fuse3 --cflags` -I.
LDFLAGS = `pkg-config fuse3 --libs` -lcjson -lfuse
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