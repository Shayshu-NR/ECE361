#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>

int main(int argc, char const *argv[])
{

    int port;
    int socket_disc;
    int bind;
    struct protoent *udp_protocol;
    struct sockaddr server_address;

    // We need a UDF listening port...
    if (argc != 2)
    {
        fprintf(stdout, "Usage: Server <UDP listen port>\n");
        return 0;
    }

    // Get port number
    port = atoi(argv[1]);

    // Open a DGRAM socket discriptor
    udp_protocol = getprotobyname("udp");
    socket_disc = socket(AF_INET, SOCK_DGRAM, udp_protocol->p_proto);
    if(socket_disc < 0){
        fprintf(stderr, "Error occured when getting socket discriptor");
        return 0;
    }

    // Bind the socket to the requested port
    memset(&server_address, 0, sizeof(server_address));
    server_address.
}