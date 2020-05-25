CC = gcc
CFLAGS = -Wall -I/opt/libevent/include -L/opt/libevent/lib
TARGETS = file-le-client file-le-server 

all: $(TARGETS) 

file-le-server.o file-le-client.o err.o: err.h

file-le-client: file-le-client.o err.o
	$(CC) $(CFLAGS) $^ -o $@ -levent

file-le-server: file-le-server.o err.o
	$(CC) $(CFLAGS) $^ -o $@ -levent

clean:
	rm -f *.o $(TARGETS) 
