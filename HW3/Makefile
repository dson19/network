CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2

.PHONY: all clean

all: server client

server: UDP_Server/server.c
	$(CC) $(CFLAGS) -o server UDP_Server/server.c

client: UDP_Client/client.c
	$(CC) $(CFLAGS) -o client UDP_Client/client.c

clean:
	rm -f server client
