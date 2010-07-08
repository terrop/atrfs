# Makefile - 21.7.2008 - 28.7.2008 Ari & Tero Roponen

CFLAGS=-D_GNU_SOURCE -DFUSE_USE_VERSION=28 \
	$(shell pkg-config --cflags fuse glib-2.0) -g
LIBS=$(shell pkg-config --libs fuse glib-2.0) -lm -lsqlite3 -lcrypto

oma: entry.o asc-srt.o util.o main.o \
	atrfs_attr.o atrfs_link.o atrfs_ops.o atrfs_dir.o \
	atrfs_lock.o notify.o \
	statistics.o atrfs_ioctl.o atrfs_xattr.o atrfs_init.o \
	entrydb.o sha1.o subtitles.o entry_filter.o \
	filter_parse.o filter_lex.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

sha1: sha1.c
	$(CC) -o $@ $< $(CFLAGS) $(LIBS) -lcrypto -DSHA1_TEST

filtertest: filter_lex.c filter_parse.c
	$(CC) $(CFLAGS) -o  $@ $^ -DFILTER_TEST

filter_lex.c: filter_lex.l filter_misc.c
	flex -o$@ $<

filter_parse.c filter_parse.h: filter_parse.y
	bison -d -ofilter_parse.c $<

.PHONY: clean
clean:
	rm -f oma database sha1 *.o filter_lex.c filter_parse.c filter_parse.h
