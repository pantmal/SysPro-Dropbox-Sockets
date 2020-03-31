CC=gcc

CFLAGS=-c
LP=-lpthread
all: dropbox_server dropbox_client

dropbox_server: dropbox_server.o Client_List.o 
	$(CC) -o dropbox_server dropbox_server.o Client_List.o

dropbox_client: dropbox_client.o Client_List.o Buffer.o Functions.o
	$(CC) -o dropbox_client dropbox_client.o Client_List.o Buffer.o Functions.o $(LP)

dropbox_server.o: dropbox_server.c
	$(CC) $(CFLAGS) dropbox_server.c

dropbox_client.o: dropbox_client.c
	$(CC) $(CFLAGS) $(LP) dropbox_client.c

Client_List.o: Client_List.c 
	$(CC) $(CFLAGS) Client_List.c

Buffer.o: Buffer.c 
	$(CC) $(CFLAGS) $(LP) Buffer.c

Functions.o: Functions.c 
	$(CC) $(CFLAGS) Functions.c

clean:
	rm -rf *o dropbox_server dropbox_client
