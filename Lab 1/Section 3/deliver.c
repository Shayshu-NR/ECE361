#include "packet.h"

int main(int argc, char const *argv[])
{

    int port;
    int socket_disc;
    int buffer_size = 2000;
    char ftp_filename[100];
    char buffer[buffer_size];
    char file_name[1000];
    char *message = "ftp";
    struct protoent *udp_protocol;
    struct sockaddr_in client_address, server_address;
    int server_address_length;
    struct timeval start_time, end_time;

    // We need a server address and a port number
    if (argc != 3)
    {
        fprintf(stderr, "Usage: deliver <server address> <server port number>");
        return 0;
    }

    // Get the port number
    port = atoi(argv[2]);

    // Get the server address in dot-and-number format
    memset(server_address.sin_zero, '\0', sizeof(server_address.sin_zero));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    if (inet_aton(argv[1], &server_address.sin_addr) < 0)
    {
        fprintf(stderr, "Failed to convert server address to dot and number format\n");
        return 0;
    }

    // Get a DGRAM socket discriptor
    udp_protocol = getprotobyname("udp");
    socket_disc = socket(PF_INET, SOCK_DGRAM, udp_protocol->p_proto);
    if (socket_disc < 0)
    {
        fprintf(stderr, "Error occured when getting socket discriptor\n");
        return 0;
    }

    // Get the filename from the user
    printf("Input \'ftp <file name>\':");

    fgets(ftp_filename, buffer_size, stdin);
    if (ftp_filename[0] == 'f' && ftp_filename[1] == 't' && ftp_filename[2] == 'p')
    {
        char *token = strtok(ftp_filename + 4, "\n\r\t");
        strcpy(file_name, token);

        if (access(file_name, F_OK) < 0)
        {
            fprintf(stderr, "Invalid filename\n");
            return 0;
        }
    }
    else
    {
        printf("Usage: ftp <file name>\n");
        return 0;
    }

    // Start a timer to measure round trip time
    gettimeofday(&start_time, NULL);
    double client_send_time = (double)start_time.tv_sec * 1000000 + (double)start_time.tv_usec;

    // Now send a message to the server
    if (sendto(socket_disc, message, strlen(message), 0, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        fprintf(stderr, "Failed to send packet\n");
        return 0;
    }

    // Now wait for a message back from the server
    memset(buffer, 0, sizeof(buffer));
    if (recvfrom(socket_disc, buffer, buffer_size, 0, (struct sockaddr *)&server_address, &server_address_length) < 0)
    {
        fprintf(stderr, "Error when receiving packet\n");
        return 0;
    }
    else
    {
        if (strcmp("yes", buffer) == 0)
        {
            gettimeofday(&end_time, NULL);
            double client_recv_time = (double)end_time.tv_sec * 1000000 + (double)end_time.tv_usec;
            printf("A file transfer can start\n");
            printf("Round trip time: %f useconds\n", (client_recv_time - client_send_time));
        }
    }

    // Now read the file and save 1000 bytes into the buffer at a time
    char file_buffer[1000];
    memset(file_buffer, 0, sizeof(file_buffer));
    ssize_t file_reader;
    FILE *file_descriptor = fopen(file_name, "rb");
    char PtoS[2000];
    unsigned int file_size;

    // Calculate the total number of fragments that are going to be sent
    struct stat st;
    stat(file_name, &st);
    int total_fragments = (int)ceil((double)st.st_size / 1000.0);

    int seq_no = 1;
    while ((file_size = fread(file_buffer, 1, sizeof(file_buffer), file_descriptor)) > 0)
    {
        printf("File_size: %d\n", file_size);
        // Create the packet...
        memset(PtoS, '\0', sizeof(PtoS));
        unsigned int header_size = sprintf(PtoS, "%d:%d:%d:%s:", total_fragments, seq_no, file_size, file_name);
        memcpy(PtoS + header_size, file_buffer, file_size);
        seq_no++;

        // Start a timer to measure round trip time
        gettimeofday(&start_time, NULL);
        client_send_time = (double)start_time.tv_sec * 1000000 + (double)start_time.tv_usec;

        // Now send a message to the server
        if (sendto(socket_disc, PtoS, sizeof(PtoS), MSG_CONFIRM, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
        {
            fprintf(stderr, "Failed to send packet\n");
            return 0;
        }

        // Now wait for a message back from the server
        memset(buffer, 0, sizeof(buffer));
        if (recvfrom(socket_disc, buffer, buffer_size, 0, (struct sockaddr *)&server_address, &server_address_length) < 0)
        {
            fprintf(stderr, "Error when receiving packet\n");
            return 0;
        }
        else
        {
            if (strcmp("ACK", buffer) == 0)
            {
                memset(file_buffer, 0, sizeof(file_buffer));
                gettimeofday(&end_time, NULL);
                double client_recv_time = (double)end_time.tv_sec * 1000000 + (double)end_time.tv_usec;
                printf("Packet was received successfully\n");
                printf("Round trip time: %f useconds\n", (client_recv_time - client_send_time));
            }
            else
            {
                gettimeofday(&start_time, NULL);
                double client_recv_time = (double)end_time.tv_sec * 1000000 + (double)end_time.tv_usec;

                while (strcmp("ACK", buffer) != 0)
                {

                    printf("Packet failure\n");
                    printf("Round trip time: %f useconds\n", (client_recv_time - client_send_time));

                    gettimeofday(&start_time, NULL);
                    client_send_time = (double)start_time.tv_sec * 1000000 + (double)start_time.tv_usec;
                    if (sendto(socket_disc, PtoS, sizeof(PtoS), MSG_CONFIRM, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
                    {
                        fprintf(stderr, "Failed to` send packet\n");
                        return 0;
                    }

                    // Now wait for a message back from the server
                    memset(buffer, 0, sizeof(buffer));
                    if (recvfrom(socket_disc, buffer, buffer_size, 0, (struct sockaddr *)&server_address, &server_address_length) < 0)
                    {
                        fprintf(stderr, "Error when receiving packet\n");
                        return 0;
                    }

                    gettimeofday(&end_time, NULL);
                    client_recv_time = (double)end_time.tv_sec * 1000000 + (double)end_time.tv_usec;
                }

                memset(file_buffer, 0, sizeof(file_buffer));
                gettimeofday(&end_time, NULL);
                client_recv_time = (double)end_time.tv_sec * 1000000 + (double)end_time.tv_usec;
                printf("Packet was received successfully\n");
                printf("Round trip time: %f useconds\n", (client_recv_time - client_send_time));
            }
        }
    }
    fclose(file_descriptor);
    close(socket_disc);
}