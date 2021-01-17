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
    struct sockaddr_in server_address;

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
        fprintf(stderr, "Error occured when getting socket discriptor\n");
        return 0;
    }

    // Bind the socket to the requested port (This method is OLD!)
    memset(server_address.sin_zero, '\0', sizeof(server_address.sin_zero));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    bind = bind(socket_disc, (struct sockaddr *)&server_address, sizeof(server_address));
    if (bind < 0){
        fprinf(stderr, "Error occured when binding the socket\n");
        return 0;
    }

    // Now wait to receive a message from the client
    
}