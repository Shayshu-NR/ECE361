#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include "client_helper.h"

int main(int argc, char const *argv[])
{

    int socket_disc;
    int server_socket;
    struct addrinfo hints, *res;
    int texting = 1;
    char user_command[50];
    char PtoS[MAX_MSG];
    session_id = -1;
    logged_in = false;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Now we have to wait till the user issues a command...
    while (texting > 0)
    {
        fprintf(stderr, "Enter a command...\n");
        scanf("%s", user_command);

        // Log the user in by sending a login segment to the server
        // and waiting for an ack or nack
        if (strcmp(user_command, "/login") == 0 && !logged_in)
        {
            fprintf(stderr, "Enter your Cient ID, Password, Server IP, and Server port:\n");
            scanf("%s %s %s %s", client_id, password, server_ip, port);

            // Get the server information...
            getaddrinfo(server_ip, port, &hints, &res);

            // Get a socket....
            socket_disc = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

            if (socket_disc < 0)
            {
                fprintf(stderr, "Socket connection error\n");
                return 0;
            }

            fprintf(stderr, "%d\n", socket_disc);

            //Now try to connect!
            if (connect(socket_disc, res->ai_addr, res->ai_addrlen) < 0)
            {
                fprintf(stderr, "Failed to connect to server\n");
                return 0;
            }

            login(PtoS, socket_disc);
        }

        else if (strcmp(user_command, "/logout") == 0 && logged_in)
        {
            //
        }

        // Make sure to close stuff properly
        else if (strcmp(user_command, "/quit") == 0)
        {
            return 0;
        }

        else if (strcmp(user_command, "/joinsession") == 0 && logged_in)
        {
            fprintf(stderr, "Enter the session ID you'd like to join:\n");
            scanf("%s", session_name);

            //
        }

        else if (strcmp(user_command, "/createsession") == 0 && logged_in)
        {
            fprintf(stderr, "Enter the session name you'd like to create:\n");
            scanf("%s", session_name);
            
            fprintf(stderr, "%d\n", socket_disc);
            createSession(PtoS, socket_disc);
        }

        else if (!logged_in)
        {
            fprintf(stderr, "Please login...\n");
        }

        memset(user_command, '\0', 50);
    }
}