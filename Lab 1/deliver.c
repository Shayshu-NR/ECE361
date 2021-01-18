#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char const *argv[])
{

    int port;
    int socket_disc;
    int buffer_size = 100;
    char ftp_filename[buffer_size];
    struct protoent *udp_protocol;
    struct sockaddr_in client_address, server_address;

    // We need a server address and a port number
    if (argc != 3)
    {
        fprintf(stderr, "Usage: deliver <server address> <server port number>");
        return 0;
    }

    // Get the port number
    port = atoi(argv[2]);

    // Get the server address in dot-and-number format
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
        char file_name[buffer_size];
        char *token = strtok(ftp_filename + 4, "\n\r\t");
        strcpy(file_name, token);

        if (access(file_name, F_OK) < 0)
        {
            printf("Invalid filename\n");
            return 0;
        }
    }
    else{
        printf("Usage: ftp <file name>");
        return 0;
    }
}