#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include "server_helper.h"
// /login Bob Jim 128.100.13.227 1789
// /login Shayshu password 128.100.13.227 1789
// /login Hamid ece361 128.100.13.227 1789

int main(int argc, char const *argv[])
{

    int port;
    int socket_disc;
    int binded;
    struct sockaddr_in server_address;
    struct addrinfo hints, *servinfo, *p;
    int rv;
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

    // Create a db of all the user data
    for (int i = 0; i < MAX_USERS; i++)
    {
        memset(clients[i].name, '\0', MAX_NAME);
        memset(clients[i].ip_addr, '\0', INET_ADDRSTRLEN);

        strcpy(clients[i].name, usernames[i]);
        strcpy(clients[i].ip_addr, "NULL");
        clients[i].port = -1;
        clients[i].sockfd = -1;
        clients[i].active = -1;
        clients[i].session_id = i;

        for (int j = 0; j < MAX_SESSIONS; j++){
            clients[i].chatRoom[j] = NULL;
        }
    }

    // Get port number
    port = atoi(argv[1]);

    // Set up the server socket and binding...
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "Get server address error\n");
        return 0;
    }

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((socket_disc = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            continue;
        }

        if (bind(socket_disc, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(socket_disc);
            continue;
        }
        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "Failed to bind\n");
        return 0;
    }

    fprintf(stderr, "Binded\n");
    freeaddrinfo(servinfo);

    // Now the server is setup to receive info from the client...
    // Listen and accept incoming connections
    if (listen(socket_disc, BACKLOG) < 0)
    {
        close(socket_disc);
        fprintf(stderr, "Failed to listen\n");
        return 0;
    }

    // Keep track of the biffest file descriptor
    fdmax = socket_disc;

    int connecting_socket = socket_disc;
    while (1)
    {
        FD_ZERO(&read_fds);
        FD_SET(socket_disc, &read_fds);

        // Only add active users to the read set
        for (int i = 0; i < MAX_USERS; i++)
        {
            if(clients[i].active == 1){
                FD_SET(clients[i].sockfd, &read_fds);

                if(clients[i].sockfd > fdmax){
                    fdmax = clients[i].sockfd;
                }
            }
        }

        // Add a listener
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) < 0)
        {
            fprintf(stderr, "Failed to select\n");
            return 0;
        }

        // Client connecting...
        if (FD_ISSET(socket_disc, &read_fds))
        {
            // Handle new connection
            // If the client has made a connection
            // than accept it and check what they sent...
            struct sockaddr_storage client_addr;
            socklen_t addr_len;
            addr_len = sizeof(client_addr);

            int client_socket = accept(socket_disc, (struct sockaddr *)&client_addr, &addr_len);

            if (client_socket < 0)
            {
                fprintf(stderr, "Client socket error\n");
                return 0;
            }
            else
            {
                // If the client is successfully logged in then
                // add them to the read set....
                if (login(client_socket, &client_addr) < 0)
                {
                    close(client_socket);
                }
                else{
                    fprintf(stderr, "Logged in user\n\n");
                }
            }
        }
        // Otherwise the client is sending the server
        // some data so parse it and then execute the correct command
        else
        {
            // Check which client socket sent the data...
            for (int i = 0; i < MAX_USERS; i++)
            {
                int client_socket = client_sockets[i];

                // If the client socket is set and in the read set then
                // execute the given command
                if (client_socket != -1 && FD_ISSET(client_socket, &read_fds))
                {

                    char buffer[2 * BUFFER_SIZE];
                    memset(buffer, '\0', 2 * BUFFER_SIZE);
                    if(recv(client_socket, buffer, 2 * BUFFER_SIZE, 0) < 0){
                        break;
                    }

                    if (buffer[0] == '\0'){
                        break;
                    }
                    
                    struct message client_request;
                    parseBuffer(buffer, &client_request);

                    if (client_request.type == NEW_SESS)
                    {
                        fprintf(stderr, "Creating new sesssion %s for %s\n", client_request.data, client_request.source);
                        int user_id = getUser(client_request.source);
                        createSession(client_request.data, client_socket, &clients[user_id]);
                        break;
                    }

                    else if (client_request.type == JOIN)
                    {
                        int user_id = getUser(client_request.source);
                        fprintf(stderr, "%s is joining session %s\n", clients[user_id].name, client_request.data);
                        joinClientSession(client_request.data, client_socket, &clients[user_id]);
                        break;
                    }

                    else if (client_request.type == LEAVE_SESS)
                    {
                        fprintf(stderr, "%s is leaving session %s\n", client_request.source, client_request.data);
                        int user_id = getUser(client_request.source);
                        leaveClientSession(client_request.data, client_socket, &clients[user_id]);
                        break;
                    }

                    else if (client_request.type == QUERY)
                    {
                        fprintf(stderr, "Getting list of active clients and sessions\n");
                        query(client_socket);
                        break;
                    }

                    else if (client_request.type == LOGOUT)
                    {
                        fprintf(stderr, "Logging out user %s\n", client_request.source);
                        int user_id = getUser(client_request.source);
                        logoutClient(client_socket, &clients[user_id]);
                        break;
                    }

                    else if (client_request.type == MESSAGE)
                    {
                        fprintf(stderr, "Forwarding message %s\n", client_request.data);
                        int user_id = getUser(client_request.source);
                        sendSessionMSG(client_socket, &clients[user_id], client_request.data);
                        break;
                    }

                    else if (client_request.type == NEW_INV){
                        fprintf(stderr, "Inviting %s to %s\n", client_request.data, client_request.session);
                        int user_id = getUser(client_request.data);

                        if(user_id == -1){
                            // User does not exist...
                            break;
                        }

                        inviteUserToSession(client_socket, &clients[user_id], client_request.session, client_request.data);
                        
                        break;
                    }
                }
            }
        }
    }
    return 0;
}