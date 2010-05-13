# Makefile - 21.7.2008 - 28.7.2008 Ari & Tero Roponen

CFLAGS=-D_GNU_SOURCE -DFUSE_USE_VERSION=28 \
	$(shell pkg-config --cflags fuse glib-2.0) -g
LIBS=$(shell pkg-config --libs fuse glib-2.0) -lm -ldb -lcrypto

oma: entry.o asc-srt.o util.o main.o \
	atrfs_attr.o atrfs_link.o atrfs_ops.o atrfs_dir.o \
	atrfs_lock.o notify.o \
	statistics.o atrfs_ioctl.o atrfs_xattr.o atrfs_init.o \
	database.o entrydb.o sha1.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

database: database.c database.h
	$(CC) -o $@ $< $(CFLAGS) $(LIBS) -ldb -DDATABASE_TEST

sha1: sha1.c
	$(CC) -o $@ $< $(CFLAGS) $(LIBS) -lcrypto -DSHA1_TEST

.PHONY: clean
clean:
	rm -f oma database sha1 *.o
