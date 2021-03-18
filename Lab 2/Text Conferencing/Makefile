TARGET = server client

all: ${TARGET}
server: server.c server_helper.c
	gcc -c server_helper.c 
	gcc -c server.c 
	gcc server_helper.o server.o -o server
client: client.c client_helper.c
	gcc -c client_helper.c
	gcc -c client.c
	gcc client_helper.o client.o -o client
clean:  
	rm -f ${TARGET}