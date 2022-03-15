DEBUG ?= 1
ifeq ($(DEBUG), 0)
    CCFLAGS=-Wall -g3 -pedantic
else
    CCFLAGS=-Wall -g3 -pedantic -D DEBUG_FLAG
endif

CC=gcc

all: client server

client: client.c proj.c p4.c
	$(CC) $(CCFLAGS) -o client client.c proj.c p4.c

server: server.c proj.c p4.c
	$(CC) $(CCFLAGS) -o server server.c proj.c p4.c


clean:
	rm -f client server
