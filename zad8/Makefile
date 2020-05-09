CC = gcc
CFLAGS = -Wall
TARGETS = time-client time-server

all: $(TARGETS) 

err.o: err.c err.h

time-server.o: time-server.c err.h
time-client.p: time-client.c err.h

time-server: time-server.o err.o
time-client: time-client.o err.o

clean:
	rm -f *.o $(TARGETS) 
