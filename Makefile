CC=c99
CFLAGS=-Wall

all:
	$(CC) $(CFLAGS) -o bf bf.c

clean:
	rm bf
