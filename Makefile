# Makefile - 21.7.2008 - 23.7.2008 Ari & Tero Roponen

CFLAGS=$(shell pkg-config --cflags fuse glib-2.0) -g
LIBS=$(shell pkg-config --libs fuse glib-2.0)

oma: main.o asc-srt.o
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean
clean:
	rm -f oma *.o
