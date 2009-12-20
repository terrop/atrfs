# Makefile - 21.7.2008 - 28.7.2008 Ari & Tero Roponen

CFLAGS=-D_GNU_SOURCE -DFUSE_USE_VERSION=28 \
	$(shell pkg-config --cflags fuse glib-2.0) -g
LIBS=$(shell pkg-config --libs fuse glib-2.0) -lm

oma: entry.o asc-srt.o util.o main.o \
	atrfs_attr.o atrfs_link.o atrfs_ops.o atrfs_dir.o \
	atrfs_lock.o notify.o \
	statistics.o atrfs_ioctl.o atrfs_xattr.o atrfs_init.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean
clean:
	rm -f oma *.o
