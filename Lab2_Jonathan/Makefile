TARGET = server client
all: ${TARGET}

RM := rm -f

server: ./server.c
	gcc ./server.c -Wall -Wextra -g -o server

client: ./client.c
	gcc ./client.c -Wall -Wextra  -g -o client

clean:
	$(RM) $(TARGET)