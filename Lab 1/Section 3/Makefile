TARGET = server deliver

all: ${TARGET}
server: server.c
	gcc server.c -o server
deliver: deliver.c
	gcc deliver.c -lm -o deliver
clean:  
	rm -f ${TARGET}