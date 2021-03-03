#include "packet.h"

int main(int argc, char const *argv[])
{

    // Network variables
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

    // Timing varibales
    struct timeval start_time, end_time;
    double sample_RTT = 0;
    double estimate_RTT = 0;
    double dev_RTT = 0;
    double timeout;
    double alpha = 0.125;
    double beta = 0.25;

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

            // Now calculate the inital timeout, dev etc...
            // These are done in micro seconds...
            sample_RTT = (client_recv_time - client_send_time);
            estimate_RTT = (1.0 - alpha) * estimate_RTT + alpha * sample_RTT;
            dev_RTT = (1.0 - beta) * dev_RTT + beta * abs(sample_RTT - estimate_RTT);
            timeout = estimate_RTT + 4 * dev_RTT;

            printf("Initial timeout: %lf useconds\n", timeout);
        }
    }

    // Now read the file and save 1000 bytes into the buffer at a time
    char file_buffer[1000];
    memset(file_buffer, 0, sizeof(file_buffer));
    ssize_t file_reader;
    FILE *file_descriptor = fopen(file_name, "rb");
    char PtoS[2000];
    unsigned int file_size;
    int done = 1;
    bool timeout_triggered = false;

    // Calculate the total number of fragments that are going to be sent
    struct stat st;
    stat(file_name, &st);
    int total_fragments = (int)ceil((double)st.st_size / 1000.0);

    int seq_no = 0;
    while (done > 0)
    {
        // Only read more data from the file if the timeout wasn't triggered...
        if(!timeout_triggered && (file_size = fread(file_buffer, 1, sizeof(file_buffer), file_descriptor)) > 0){
            done = 1;
            seq_no++;
        }
        else if (seq_no == total_fragments && !timeout_triggered && file_size == 0){
            done = 0;
            break;
        }

        printf("File_size: %d\n", file_size);
        // Create the packet...
        memset(PtoS, '\0', sizeof(PtoS));
        unsigned int header_size = sprintf(PtoS, "%d:%d:%d:%s:", total_fragments, seq_no, file_size, file_name);
        memcpy(PtoS + header_size, file_buffer, file_size);
        printf("Sending Seq_no %d\n\n", seq_no);

        // Start a timer to measure round trip time
        gettimeofday(&start_time, NULL);
        client_send_time = (double)start_time.tv_sec * 1000000 + (double)start_time.tv_usec;
        // Now send a message to the server
        if (sendto(socket_disc, PtoS, sizeof(PtoS), MSG_CONFIRM, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
        {
            fprintf(stderr, "Failed to send packet\n");
            return 0;
        }

        // Now we wait and see if we get an ack from the server
        // during out timeout window, if we don't get anything 
        // then resend the packet 
        struct timeval tv;
        fd_set readfds;

        tv.tv_sec = 0;
        tv.tv_usec = timeout;

        FD_ZERO(&readfds);
        FD_SET(socket_disc, &readfds);

        select(socket_disc + 1, &readfds, NULL, NULL, &tv);

        if(!FD_ISSET(socket_disc, &readfds)){
            printf("Timeout Triggered, retransmit last segment\n");
            timeout_triggered = true;

            // Update the timer to make the window bigger to hopefully
            // capture the server ack...
            timeout = timeout * 2;

            // Don't wait for an ack, just resend the packet now...
            continue;
        }
        else{
            printf("Timeout not triggered continue...\n");
            timeout_triggered = false;
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
            // Extract the data from
            printf("%s\n", buffer);

            gettimeofday(&end_time, NULL);
            double client_recv_time = (double)end_time.tv_sec * 1000000 + (double)end_time.tv_usec;
            // Now calculate the inital timeout, dev etc...
            // These are done in micro seconds...
            sample_RTT = (client_recv_time - client_send_time);
            estimate_RTT = (1.0 - alpha) * estimate_RTT + alpha * sample_RTT;
            dev_RTT = (1.0 - beta) * dev_RTT + beta * abs(sample_RTT - estimate_RTT);
            timeout = estimate_RTT + 4 * dev_RTT;

            printf("New timeout: %lf useconds\n", timeout);

            memset(buffer, '\0', buffer_size);

        }
    }
    fclose(file_descriptor);
    close(socket_disc);
}