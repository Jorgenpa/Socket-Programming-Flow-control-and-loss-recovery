CFLAGS= -g -std=gnu11 -D_BSD_SOURCE -Wall -Wextra
BIN= client server

all: $(BIN)

client: client.c 
	gcc $(CFLAGS) client.c -o client send_packet.c

server: server.c 
	gcc $(CFLAGS) server.c -o server pgmread.c

clitest: client.c 
	gcc $(CFLAGS) client.c -o client send_packet.c && ./client 129.240.65.63 2020 list_of_filenames.txt 0.1

sertest: server.c 
	gcc $(CFLAGS) server.c -o server pgmread.c && ./server 2020 big_set output.txt

clivalgrind: client.c
	gcc $(CFLAGS) client.c -o client send_packet.c && valgrind ./client 129.240.65.63 2020 list_of_filenames.txt 0.1

servalgrind: server.c
	gcc $(CFLAGS) server.c -o server pgmread.c && valgrind ./server 2020 big_set output.txt

clean:
	rm -f $(BIN) *.o output.txt
