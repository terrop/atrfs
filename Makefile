# Makefile - 21.7.2008 - 21.7.2008 Ari & Tero Roponen

CFLAGS=$(shell pkg-config --cflags glib-2.0) -g
LIBS=$(shell pkg-config --libs glib-2.0)

oma: main.o
	$(CC) -o $@ $^ -lfuse $(LIBS)

.PHONY: clean
clean:
	rm -f oma *.o
