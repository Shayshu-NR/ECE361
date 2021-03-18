#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include "server_helper.h"

int main(int argc, char const *argv[])
{

    int port;
    int socket_disc;
    int binded;
    struct sockaddr_in server_address;
    int fdmax;
    char buffer[BUFFER_SIZE];
    fd_set read_fds;
    fd_set master;
    active_sessions = 0;

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    // Make sure we get a TCP listening port...
    if (argc != 2)
    {
        fprintf(stdout, "Usage: Server <TCP listening port>\n");
        return 0;
    }

    // Create a data structure to hold all the client info...
    // Now passwords and usernames are stored in an array...
    getClientDataFromDB("./DataBase/client_data.csv", 2, 1);
    getClientDataFromDB("./DataBase/client_data.csv", 3, 0);

    for (int i = 0; i < MAX_USERS; i++)
    {
        client_sockets[i] = -1;
    }

    // We have up to MAX_SESSIONS sessions 
    // each session tracks the 
    for (int i = 0; i < MAX_SESSIONS; i++)
    {
        initSession(&client_sessions[i]);
    }

    // Get port number
    port = atoi(argv[1]);

    // Open a unix tcp socket...
    struct protoent *tcp_protocol = getprotobyname("tcp");
    socket_disc = socket(AF_INET, SOCK_STREAM, tcp_protocol->p_proto);
    if (socket_disc < 0)
    {
        fprintf(stderr, "Error occured when getting socket discriptor\n");
        return 0;
    }

    // Bind the socket to the requested port (This method is OLD!)
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    binded = bind(socket_disc, (struct sockaddr *)&server_address, sizeof(server_address));
    if (binded < 0)
    {
        fprintf(stderr, "Error occured when binding the socket\n");
        return 0;
    }

    // Now the server is setup to receive info from the client...
    // Listen and accept incoming connections
    if (listen(socket_disc, BACKLOG) < 0)
    {
        close(socket_disc);
        fprintf(stderr, "Failed to listen\n");
        return 0;
    }

    //Add the listener to the master set...
    FD_SET(socket_disc, &master);

    // Keep track of the biffest file descriptor
    fdmax = socket_disc;

    // We're listening on the given port...
    // Now accept any incoming connections and
    // start a session with the client....
    int connecting_socket = socket_disc;
    while (1)
    {
        // Copy master set to temp set
        read_fds = master;

        select(fdmax + 1, &read_fds, NULL, NULL, NULL);

        // If the client has made a connection
        // than accept it and check what they sent...
        struct sockaddr client_addr;
        socklen_t addr_len;
        int client_socket;

        for (int i = 0; i <= fdmax; i++)
        {

            // Client connecting...
            if (i == socket_disc)
            {
                // Handle new connection
                client_socket = accept(socket_disc, &client_addr, &addr_len);

                if (client_socket < 0)
                {
                    fprintf(stderr, "Client socket error\n");
                    return 0;
                }
                else
                {

                    // If the client is successfully logged in then
                    // add them to the master set....

                    if (login(client_socket))
                    {
                        // Add client socket to the master set
                        FD_SET(client_socket, &master);

                        if (client_socket > fdmax)
                        {
                            fdmax = client_socket;
                        }
                    }
                }
            }
            // Otherwise the client is sending the server
            // some data so parse it and then execute the correct command
            else
            {
                memset(buffer, '\0', BUFFER_SIZE);
                if (recv(i, (void *)buffer, BUFFER_SIZE, 0) < 0)
                {
                    fprintf(stderr, "Receive error\n");
                }
                else{
                    struct message client_request;
                    parseBuffer(buffer, &client_request);

                    if (client_request.type == NEW_SESS)
                    {
                        fprintf(stderr, "Creating new sesssion %s for %s\n", client_request.data, client_request.source);
                        createSession(client_request.data, i);
                    }
                }
            }
        }
    }
    return 0;
}