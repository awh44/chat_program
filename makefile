runClient: client
	./client ${HOST}

client: client.c
	gcc client.c -o client -lpthread

runServer: server
	./server

server: server.c
	gcc server.c -o server -lpthread

runNcurses:
	./ncurses_client ${HOST}

ncurses:
	gcc ncurses_client.c -o ncurses_client -lncurses -lpthread

compile: server client ncurses

clean:
	rm -f client
	rm -f server
	rm -f ncurses_client

.PHONY: clean compile
