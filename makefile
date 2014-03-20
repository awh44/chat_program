runClient: client
	./client

client: client.c
	gcc client.c -o client -lpthread

runServer: server
	./server

server: server.c
	gcc server.c -o server -lpthread

compile: server client

clean:
	rm -f client
	rm -f server

.PHONY: clean compile
