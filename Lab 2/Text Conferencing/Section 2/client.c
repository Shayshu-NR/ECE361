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

int noSession()
{
    for (int i = 0; i < MAX_SESSIONS; i++)
    {
        if (current_session[i][0] != '\0')
        {
            return 1;
        }
    }
    return -1;
}

int main(int argc, char const *argv[])
{

    int socket_disc = -1;
    int server_socket;
    struct addrinfo hints, *res, *servinfo, *p;
    int rv;
    int texting = 1;
    char user_command[MAX_MSG];
    char PtoS[MAX_MSG];
    session_id = -1;
    logged_in = false;
    fd_set read_fds;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    memset(current_session, '\0', MAX_SESSION);

    // Create a read set for the client to receive
    // messages from a current session...

    // Now we have to wait till the user issues a command...
    while (texting > 0)
    {
        FD_ZERO(&read_fds);

        // Add stdin to the read set
        FD_SET(fileno(stdin), &read_fds);

        // Wait indefinetly for a message to be received...
        if (socket_disc > 0)
        {

            FD_SET(socket_disc, &read_fds);
            select(socket_disc + 1, &read_fds, NULL, NULL, NULL);
        }
        else
        {
            select(fileno(stdin) + 1, &read_fds, NULL, NULL, NULL);
        }

        // Listen for any received messages on the client socket....
        if (logged_in && FD_ISSET(socket_disc, &read_fds))
        {
            char new_msg[BUFFER_SIZE];
            memset(new_msg, '\0', BUFFER_SIZE);
            struct message new_user_msg;

            if (recv(socket_disc, new_msg, BUFFER_SIZE, 0) < 0)
            {
                fprintf(stderr, "Receive message error\n");
                return 0;
            }

            parseBuffer(new_msg, &new_user_msg);

            // Check what type of message was received...
            if (new_user_msg.type == MESSAGE)
            {
                fprintf(stderr, "%s in %s: %s", new_user_msg.source, new_user_msg.session, new_user_msg.data);
            }
            else if (new_user_msg.type == NEW_INV)
            {
                fprintf(stderr, "%s invited you to join session %s. y or n?\n", new_user_msg.data, new_user_msg.session);
                char response[MAX_MSG];
                scanf("%s", response);

                if (strcmp(response, "y") == 0 || strcmp(response, "yes") == 0)
                {
                    memset(join_session_name, '\0', MAX_SESSION);
                    strcpy(join_session_name, new_user_msg.session);

                    joinSession(PtoS, socket_disc, join_session_name);
                }
            }

            continue;
        }

        else if (FD_ISSET(fileno(stdin), &read_fds))
        {
            scanf("%s", user_command);

            // Log the user in by sending a login segment to the server
            // and waiting for an ack or nack
            if (strcmp(user_command, "/login") == 0 && !logged_in)
            {
                fprintf(stderr, "Enter your Cient ID, Password, Server IP, and Server port:\n");
                scanf("%s %s %s %s", client_id, password, server_ip, port);

                // Get the server information...
                if ((rv = getaddrinfo(server_ip, port, &hints, &servinfo)) != 0)
                {
                    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                    continue;
                }

                // loop through all the results and connect to the first we can
                for (p = servinfo; p != NULL; p = p->ai_next)
                {
                    if ((socket_disc = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
                    {
                        perror("socket");
                        continue;
                    }
                    if (connect(socket_disc, p->ai_addr, p->ai_addrlen) == -1)
                    {
                        perror("connect");
                        close(socket_disc);
                        continue;
                    }
                    break; // if we get here, we must have connected successfully
                }

                if (p == NULL)
                {
                    // looped off the end of the list with no connection
                    fprintf(stderr, "failed to connect\n");
                    return 0;
                }

                freeaddrinfo(servinfo);

                login(PtoS, socket_disc);
            }

            else if (strcmp(user_command, "/logout") == 0 && logged_in)
            {
                //
                if (noSession() > 0)
                {
                    for (int i = 0; i < MAX_SESSIONS; i++)
                    {
                        if (current_session[i][0] != '\0')
                        {
                            memset(leave_session_name, '\0', MAX_SESSION);
                            strcpy(leave_session_name, current_session[i]);
                            leaveSession(PtoS, socket_disc, leave_session_name);
                            usleep(100000);
                        }
                    }
                    usleep(100000);
                }
                logout(PtoS, socket_disc);
            }

            // Make sure to close stuff properly
            else if (strcmp(user_command, "/quit") == 0)
            {
                if (logged_in)
                {
                    if (noSession() > 0)
                    {
                        for (int i = 0; i < MAX_SESSIONS; i++)
                        {
                            if (current_session[i][0] != '\0')
                            {
                                memset(leave_session_name, '\0', MAX_SESSION);
                                strcpy(leave_session_name, current_session[i]);
                                leaveSession(PtoS, socket_disc, leave_session_name);
                                usleep(100000);
                            }
                        }
                        usleep(100000);
                    }
                    logout(PtoS, socket_disc);
                }
                return 0;
            }

            // Create a new session
            else if (strcmp(user_command, "/createsession") == 0 && logged_in)
            {
                memset(session_name, '\0', MAX_SESSION);
                fprintf(stderr, "Enter the session name you'd like to create:\n");
                scanf("%s", session_name);

                createSession(PtoS, socket_disc);
            }

            // Join a created session
            else if (strcmp(user_command, "/joinsession") == 0 && logged_in)
            {
                memset(join_session_name, '\0', MAX_SESSION);
                fprintf(stderr, "Which session would you like to join?\n");
                scanf("%s", join_session_name);

                joinSession(PtoS, socket_disc, join_session_name);
            }

            // Leave the current session
            else if (strcmp(user_command, "/leavesession") == 0 && logged_in)
            {
                if (noSession() < 0)
                {
                    continue;
                }
                memset(leave_session_name, '\0', MAX_SESSION);
                strcpy(leave_session_name, current_session[0]);
                leaveSession(PtoS, socket_disc, leave_session_name);
            }

            // Get a list of the connected clients and available sessions
            else if (strcmp(user_command, "/list") == 0 && logged_in)
            {
                list(PtoS, socket_disc);
            }

            // Invite a user to the session...
            else if (strcmp(user_command, "/invite") == 0 && logged_in)
            {
                if (noSession() < 0)
                {
                    fprintf(stderr, "Please join a session\n");
                }
                else
                {
                    memset(invite_user, '\0', MAX_NAME);
                    memset(invite_session, '\0', MAX_SESSION);
                    fprintf(stderr, "Which user? Which session?\n");
                    scanf("%s %s", invite_user, invite_session);
                    inviteSession(PtoS, socket_disc, invite_user, invite_session);
                }
            }

            // Otherwise send a message
            else
            {
                if (strcmp(user_command, "/login") == 0)
                {
                    fprintf(stderr, "Already logged in\n");
                }
                else if (!logged_in)
                {
                    fprintf(stderr, "Please login...\n");
                }

                else if (noSession() < 0)
                {
                    fprintf(stderr, "Please join a session\n");
                }

                else
                {
                    memset(msg, '\0', MAX_MSG);
                    strcpy(msg, user_command);

                    // Grab the rest of the words from the terminal
                    // since scanf only grabs the first word
                    int first_word = strlen(user_command);
                    fgets(msg + first_word, BUFFER_SIZE - first_word, stdin);

                    sendMessage(PtoS, socket_disc, msg);
                }
            }

            memset(user_command, '\0', MAX_MSG);
            fflush(stdin);
            fflush(stderr);
        }
    }
}