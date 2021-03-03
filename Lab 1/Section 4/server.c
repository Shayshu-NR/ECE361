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
    char *message = malloc(2000 * sizeof(char));
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

    // Variables used to keep track of packets...
    int seq_no = 0;
    int prev_no = 0;
    int total_fragments = 1000;
    int file_size = 0;
    char *file_name;
    char *file_data;
    int done = 0;
    FILE *file_descriptor;
    char *acknowledge;
    acknowledge = malloc((sizeof(total_fragments) + sizeof(prev_no) + sizeof(0) + sizeof("ACK") + sizeof("") + 4) * sizeof(char));
    int *received_packets = malloc(buffer_size * sizeof(int));
    memset(received_packets, 0, buffer_size);
    bool first_packet = true;

    while (done <= 0)
    {
        prev_no = seq_no;

        // Reveive the packet from the client...
        memset(buffer, '\0', buffer_size);
        memset(acknowledge, '\0', sizeof(acknowledge));
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

        // So the file data is located after the header,
        // also note that we can't use strcpy because the data
        // might not be ascii, it could be binary as well
        file_data = malloc(file_size * sizeof(char));
        memcpy(file_data, &buffer[header_size], file_size);

        fprintf(stderr, "Received packets: %d: %d : %d\n", total_fragments, seq_no, received_packets[seq_no - 1] == 10);
        // // Packet is a duplicate...
        if (received_packets[seq_no - 1] == 10)
        {
            // If we get a duplicate then don't bother
            // sending an ack, let the client timeout...
            fprintf(stderr, "Received duplicate...\n");
            continue;
        }
        else
        {
            // We got a new packet so make sure to send an ack...
            fprintf(stderr, "New packet...\n");
            received_packets[seq_no - 1] = 10;
        }

        

        // Open a file descripotor on our first packet...
        if (seq_no == 1)
        {
            file_descriptor = fopen(file_name, "w+");
        }

        memset(acknowledge, '\0', sizeof(acknowledge));
        // sleep(2);
        if (prev_no != seq_no - 1)
        {
            // Packet sequence out of order...
            // Acknowledge the last packet that arrived...
            // Packet : Total fragments : Packet received : Not used :  ACK : Not used
            sprintf(acknowledge, "%d:%d:%d:%s:%s", total_fragments, prev_no, 0, "ACK", "");
            printf("NACK %s\n", acknowledge);

            if (sendto(socket_disc, acknowledge, buffer_size, 0, (struct sockaddr *)&client_address, client_length) < 0)
            {
                fprintf(stderr, "Failed to send response\n");
                return 0;
            }
        }
        else
        {
            // Packet was received properly...
            sprintf(acknowledge, "%d:%d:%d:%s:%s", total_fragments, seq_no, 0, "ACK", "");
            printf("ACK %s\n", acknowledge);

            fwrite(file_data, sizeof(char), file_size, file_descriptor);

            if (sendto(socket_disc, acknowledge, buffer_size, 0, (struct sockaddr *)&client_address, client_length) < 0)
            {
                fprintf(stderr, "Failed to send response\n");
                return 0;
            }
        }

        if (seq_no == total_fragments)
        {
            done = 1;
        }
    }


    fclose(file_descriptor);
    shutdown(socket_disc, 2);
}