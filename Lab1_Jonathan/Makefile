TARGET = server deliver
all: ${TARGET}

server: ./server_/server.c
	gcc ./server_/server.c -Wall -Wextra -o ./server_/server
deliver: ./deliver_/deliver.c ./deliver_/packet.h
	gcc ./deliver_/deliver.c -Wall -Wextra -o ./deliver_/deliver