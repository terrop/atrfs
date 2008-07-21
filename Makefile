# Makefile - 21.7.2008 - 21.7.2008 Ari & Tero Roponen

oma: main.o
	$(CC) -o $@ $^ -lfuse

.PHONY: clean
clean:
	rm -f oma *.o
