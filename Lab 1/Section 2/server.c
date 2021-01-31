#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char const *argv[])
{

    int port;
    int socket_disc;
    int binded;
    int buffer_size = 100;
    char buffer[buffer_size];
    char * message;
    struct protoent *udp_protocol;
    struct sockaddr_in server_address;
    struct sockaddr_storage client_address;
    int client_length;


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
    socket_disc = socket(PF_INET, SOCK_DGRAM, udp_protocol->p_proto);
    if(socket_disc < 0){
        fprintf(stderr, "Error occured when getting socket discriptor\n");
        return 0;
    }

    // Bind the socket to the requested port (This method is OLD!)
    memset(server_address.sin_zero, '\0', sizeof(server_address.sin_zero));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    binded = bind(socket_disc, (struct sockaddr *)&server_address, sizeof(server_address));
    if (binded < 0){
        fprintf(stderr, "Error occured when binding the socket\n");
        return 0;
    }

    // Now wait to receive a message from the client
    memset(buffer, 0, sizeof(buffer));
    if(recvfrom(socket_disc, buffer, buffer_size, 0, (struct sockaddr*)&client_address, &client_length) < 0){
        fprintf(stderr, "Error when receiving data\n");
        return 0;
    }

    // Now check if the received message stored in the buffer says 'ftp'
    if(strcmp("ftp", buffer) != 0){
        message = "no";
    }
    else{
        message = "yes";
    }

    printf("Message received: %s %s\n", buffer, message);
    
    // Add a sleep in order to prove the timing works
    // sleep(5);

    if(sendto(socket_disc, message, strlen(message), MSG_CONFIRM, (struct sockaddr *)&client_address, client_length) < 0){
        fprintf(stderr, "Failed to send response\n");
        return 0;
    }

    shutdown(socket_disc, 2);
}