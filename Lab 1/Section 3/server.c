#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "packet.h"

int main(int argc, char const *argv[])
{

    int port;
    int socket_disc;
    int binded;
    int buffer_size = 2000;
    char buffer[2000];
    char *message = malloc(2000*sizeof(char));
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
    if (socket_disc < 0)
    {
        fprintf(stderr, "Error occured when getting socket discriptor\n");
        return 0;
    }

    // Bind the socket to the requested port (This method is OLD!)
    memset(server_address.sin_zero, '\0', sizeof(server_address.sin_zero));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    binded = bind(socket_disc, (struct sockaddr *)&server_address, sizeof(server_address));
    if (binded < 0)
    {
        fprintf(stderr, "Error occured when binding the socket\n");
        return 0;
    }

    // Now wait to receive a message from the client
    memset(buffer, 0, sizeof(buffer));
    if (recvfrom(socket_disc, buffer, buffer_size, 0, (struct sockaddr *)&client_address, &client_length) < 0)
    {
        fprintf(stderr, "Error when receiving data\n");
        return 0;
    }

    // Now check if the received message stored in the buffer says 'ftp'
    if (strcmp("ftp", buffer) != 0)
    {
        message = "no";
    }
    else
    {
        message = "yes";
    }

    printf("Message received: %s %s\n", buffer, message);

    // Add a sleep in order to prove the timing works
    // sleep(5);

    if (sendto(socket_disc, message, buffer_size, 0, (struct sockaddr *)&client_address, client_length) < 0)
    {
        fprintf(stderr, "Failed to send response\n");
        return 0;
    }

    //
    int seq_no = 0;
    int prev_no = 0;
    int total_fragments = 1000;
    int file_size = 0;
    char *file_name;
    char *file_data;
    int done = 0;
    FILE * file_descriptor;

    while(done <= 0){
        prev_no = seq_no;

        // Reveive the packet from the client...
        memset(buffer, '\0', buffer_size);
        if (recvfrom(socket_disc, buffer, buffer_size, 0, (struct sockaddr *)&client_address, &client_length) < 0)
        {
            fprintf(stderr, "Error when receiving data\n");
            return 0;
        }

        fprintf(stderr, "Received packet!\n");

        // Now extract the data....
        int header_size = 0;

        char *token = strtok(buffer, ":");
        total_fragments = atoi(token);
        header_size += strlen(token);

        token = strtok(NULL, ":");
        seq_no = atoi(token);
        header_size += strlen(token);

        token = strtok(NULL, ":");
        file_size = atoi(token);
        header_size += strlen(token);

        token = strtok(NULL, ":");
        file_name = token;
        header_size += strlen(token);

        header_size += 4;

        file_data = malloc(file_size * sizeof(char));
        memcpy(file_data, &buffer[header_size], file_size);

        // Open a file descripotor on our first packet...
        if(seq_no == 1){
            file_descriptor = fopen(file_name, "w+");
        }

        fwrite(file_data, sizeof(char), file_size, file_descriptor);

        if (prev_no != seq_no - 1){
            if (sendto(socket_disc, "NACK", buffer_size, 0, (struct sockaddr *)&client_address, client_length) < 0)
            {
                fprintf(stderr, "Failed to send response\n");
                return 0;
            }
        }
        else{
            if (sendto(socket_disc, "ACK", buffer_size, 0, (struct sockaddr *)&client_address, client_length) < 0)
            {
                fprintf(stderr, "Failed to send response\n");
                return 0;
            }
        }

        if(seq_no == total_fragments){
            done = 1;
        }

    } 
    fclose(file_descriptor);
    shutdown(socket_disc, 2);
}