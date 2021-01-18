#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>

int main(int argc, char const *argv[]){

    int port;
    int socket_disc;
    struct protoent *udp_protocol;
    struct sockaddr_in client_address;


    // We need a server address and a port number
    if(argc != 3){
        fprintf(stderr, "Usage: deliver <server address> <server port number>");
        return 0;
    }

    // Get the port number
    port = atoi(argv[3]);

    // Get the server address in dot-and-number format
    if(inet_aton() < 0){
        fprintf(stderr, "Failed to convert server address to dot and number format\n");
        return 0;
    }

    // Get a DGRAM socket discriptor
    udp_protocol = getprotobyname("udp");
    socket_disc = socket(PF_INET, SOCK_DGRAM, udp_protocol->p_proto);
    if(socket_disc < 0){
        fprintf(stderr, "Error occured when getting socket discriptor\n");
        return 0;
    }


}