# Makefile - 21.7.2008 - 24.7.2008 Ari & Tero Roponen

CFLAGS=-D_GNU_SOURCE $(shell pkg-config --cflags fuse glib-2.0) -g
LIBS=$(shell pkg-config --libs fuse glib-2.0)

oma: entry.o asc-srt.o util.o main.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean
clean:
	rm -f oma *.o
