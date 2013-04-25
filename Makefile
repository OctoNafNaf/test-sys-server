CC=gcc
CFLAGS=-Wall -c -I./include -lpthread

all: core-server

core-server: bufstring.o core-server.o
	$(CC) bufstring.o core-server.o -o core-server -I./include -lpthread
	
bufstring.o:
	$(CC) src/bufstring.c $(CFLAGS)
	
core-server.o:
	$(CC) src/core-server.c $(CFLAGS)
	
clean:
	rm -rf *.o core-server
