#include <sys/types.h>
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

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return (void *)&(((struct sockaddr_in *)sa)->sin_addr);
    }
    else
    {
        return (void *)&(((struct sockaddr_in6 *)sa)->sin6_addr);
    }
}

char *getfield(char *line, int num)
{
    char *tok;
    for (tok = strtok(line, ";");
         tok && *tok;
         tok = strtok(NULL, ";\n"))
    {
        if (!--num)
            return tok;
    }
    return NULL;
}

// Get the user index of a user...
int getUser(char *name)
{
    for (int i = 0; i < MAX_USERS; i++)
    {
        if (strcmp(usernames[i], name) == 0)
        {
            return i;
        }
    }
    return -1;
}

// Go into the csv file indicated by path and grab the usernames,
// or passwords and store them in a 2D array.
void getClientDataFromDB(char *path, int col, int password)
{
    // Code found here: https://stackoverflow.com/questions/12911299/read-csv-file-in-c
    FILE *stream = fopen(path, "r");

    char line[1024];
    int i = 0;
    while (fgets(line, 1024, stream))
    {
        char *tmp = strdup(line);

        // Now add the usernames to our data structure...
        if (i > 0 && i < MAX_USERS)
        {
            char *name = getfield(tmp, col);

            if (password > 0)
            {
                strcpy(usernames[i - 1], name);
                fprintf(stderr, "%s\n", usernames[i - 1]);
            }
            else
            {
                strcpy(passwords[i - 1], name);
                fprintf(stderr, "%s\n", passwords[i - 1]);
            }
        }
        // NOTE strtok clobbers tmp
        free(tmp);
        i++;
    }
    return;
}

// Upon receiving a data buffer, parse it and fill the cmd structure
// with all the necessary info....
void parseBuffer(char *buffer, struct message *cmd)
{
    fprintf(stderr, "%s\n", buffer);

    int header_size = 0;

    // Tokenize the buffer and grab all the data from the packet
    // and put it in the message structure
    char *token = strtok(buffer, ":");
    cmd->type = atoi(token);

    token = strtok(NULL, ":");
    cmd->size = atoi(token);

    token = strtok(NULL, ":");
    strcpy(cmd->source, token);

    token = strtok(NULL, ":");
    strcpy(cmd->data, token);

    return;
}

// Check if the client id and pass match what's in the DB
// If verified return the user index...
int verify(char *id, char *pass)
{

    int index = 0;
    // Go through the user names and find the correct one...
    for (; index < MAX_USERS; index++)
    {
        if (strcmp(usernames[index], id) == 0)
        {
            // We've found the username!
            break;
        }
    }

    // Make sure
    if (strcmp(passwords[index], pass) == 0)
    {
        return index;
    }

    return -1;
}

// After getting a connection read the received packet
// and check if the user has the correct set of login information
// Return true or false depending on success...
bool login(int socket, struct sockaddr_storage *client_addr)
{
    fprintf(stderr, "Logging in user...\n");

    // Expecting: LOGIN:x:x:client_id:password
    int incoming_sock = socket;
    char buffer[BUFFER_SIZE];
    memset(buffer, '\0', BUFFER_SIZE);

    if (socket < 0)
    {
        fprintf(stderr, "Socket option error\n");
        return false;
    }

    // Now get the data from the client...
    if (recv(socket, (void *)buffer, BUFFER_SIZE, 0) < 0)
    {
        fprintf(stderr, "Receiving error\n");
        return false;
    }

    // Now parse the incoming info into the message packet...
    struct message received_cmd;
    parseBuffer(buffer, &received_cmd);

    // Now check if the password and client id supplied were correct...
    int verified = verify(received_cmd.source, received_cmd.data);

    //Send an ACK
    char ACK[BUFFER_SIZE];
    memset(ACK, '\0', BUFFER_SIZE);
    // Failed to find username/pass or wrong username/pass
    if (verified < 0)
    {
        createSegment(ACK, LO_NACK, "Login failed, wrong username or password");
        send(socket, ACK, BUFFER_SIZE, 0);
        return false;
    }
    // User is already logged in
    else if (client_sockets[verified] > 0)
    {
        createSegment(ACK, LO_NACK, "Login failed, already logged in");
        send(socket, ACK, BUFFER_SIZE, 0);
        return true;
    }
    // Correct username and password
    else
    {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(client_addr->ss_family, get_in_addr((struct sockaddr *)client_addr), ip, sizeof(ip));

        // Update the client data...
        memset(clients[verified].ip_addr, '\0', INET_ADDRSTRLEN);
        strcpy(clients[verified].ip_addr, ip);
        clients[verified].sockfd = socket;

        fprintf(stderr, "%s : %s\n", clients[verified].name, ip);

        client_sockets[verified] = socket;
        createSegment(ACK, LO_ACK, "Login successful");
        send(socket, ACK, BUFFER_SIZE, 0);
        return true;
    }
}

//
bool logoutClient(int socket, struct user *leaver)
{
    // We assume that the user has left all their current sessions
    int socket_index = getUser(leaver->name);

    leaver->active = -1;
    leaver->sockfd = -1;
    leaver->port = -1;
    memset(leaver->ip_addr, '\0', INET_ADDRSTRLEN);

    // Remove the socket!
    close(client_sockets[socket_index]);
    client_sockets[socket_index] = -1;

    return true;
}

// Create a control segment to send to the client
int createSegment(char *PtoS, int packet_type, char *msg)
{
    memset(PtoS, '\0', BUFFER_SIZE);
    int bytes_read;

    int size = strlen(msg);
    bytes_read = sprintf(PtoS, "%d:%d:%s:%s", packet_type, size, "Server", msg);

    fprintf(stderr, "%s\n", PtoS);

    return bytes_read;
}

// Initialize a session structure
void initSession(struct session *room)
{
    memset(room->name, '\0', MAX_SESSION);

    for (int i = 0; i < MAX_USERS; i++)
    {
        room->users[i] = -1;
    }

    room->session_id = -1;

    room->active = -1;
    return;
}

//
void closeSession(struct session *room)
{
    memset(room->name, '\0', MAX_SESSION);

    for (int i = 0; i < MAX_USERS; i++)
    {
        room->users[i] = -1;
    }

    room->session_id = -1;

    room->active = -1;

    return;
}

//
int createSession(char *session_name, int socket, struct user *creator)
{

    // If there are too many active sessions, or the
    // creator is already in  a session then reject the create request
    if (active_sessions >= MAX_SESSIONS || !oneSession(creator))
    {
        // Send a failed ACK and let the client interpret it...
        char ACK[BUFFER_SIZE];
        createSegment(ACK, NS_ACK, "-1");
        send(socket, ACK, BUFFER_SIZE, 0);
        return -1;
    }
    else
    {

        // If there are sessions availible then find its index
        // and change the session name, and increase the active sessions
        int i = 0;
        for (; i < MAX_SESSIONS; i++)
        {
            if (client_sessions[i].session_id == -1)
            {
                client_sessions[i].session_id = i;
                strcpy(client_sessions[i].name, session_name);
                creator->active = 1;
                creator->chatRoom = &client_sessions[i];
                active_sessions++;
                client_sessions[i].active = 1;

                // Now add the creator of the session to the list of
                // active users
                for (int j = 0; j < MAX_USERS; i++)
                {
                    if (client_sessions[i].users[j] == -1)
                    {
                        client_sessions[i].users[j] = creator->session_id;
                        break;
                    }
                }

                // Now send a response to the client...
                char ACK[BUFFER_SIZE];
                memset(ACK, '\0', BUFFER_SIZE);

                createSegment(ACK, NS_ACK, client_sessions[i].name);
                fprintf(stderr, "%s\n", ACK);

                if (send(socket, ACK, BUFFER_SIZE, 0) < 0)
                {
                    fprintf(stderr, "Failed send\n");
                }
                return i;
            }
        }

        // Send a failed ACK and let the client interpret it...
        char ACK[BUFFER_SIZE];
        createSegment(ACK, NS_ACK, "-1");
        send(socket, ACK, BUFFER_SIZE, 0);
        return -1;
    }
}

//
bool oneSession(struct user *joiner)
{

    for (int i = 0; i < MAX_SESSIONS; i++)
    {
        for (int j = 0; j < MAX_USERS; j++)
        {
            if (client_sessions[i].users[j] == joiner->session_id)
            {
                return false;
            }
        }
    }
    return true;
}

//
int joinClientSession(char *session_name, int socket, struct user *joiner)
{
    // Make sure the joiner isn't apart of any other session
    if (!oneSession(joiner))
    {
        // If they are in another session then send a NACK
        char ACK[BUFFER_SIZE];
        char msg[MAX_MSG];
        strcpy(msg, session_name);
        strcat(msg, ", user already in a session");

        createSegment(ACK, JN_NACK, msg);
        send(socket, ACK, BUFFER_SIZE, 0);
        return -1;
    }

    // Find the correct session..
    int index = 0;
    for (; index < MAX_SESSIONS; index++)
    {
        fprintf(stderr, "Searching: %s %s\n", client_sessions[index].name, session_name);
        if (strcmp(client_sessions[index].name, session_name) == 0)
        {
            // Now that we've found the correct session
            // Add the user to the list of active users...
            // And make the joiner active
            for (int i = 0; i < MAX_USERS; i++)
            {
                if (client_sessions[index].users[i] == -1)
                {
                    fprintf(stderr, "Found space...\n");
                    client_sessions[index].users[i] = joiner->session_id;
                    joiner->active = 1;
                    joiner->chatRoom = &client_sessions[index];
                    break;
                }
            }

            // Send the user an acknowledgement...
            char ACK[BUFFER_SIZE];
            createSegment(ACK, JN_ACK, session_name);
            send(socket, ACK, BUFFER_SIZE, 0);

            return index;
        }
    }

    // If we can't find tthe correct session then return a JN_NACK
    char ACK[BUFFER_SIZE];
    char msg[MAX_MSG];
    strcpy(msg, session_name);
    strcat(msg, ", cound not find session");

    createSegment(ACK, JN_NACK, msg);
    send(socket, ACK, BUFFER_SIZE, 0);
    return -1;
}

//
int leaveClientSession(char *session_name, int socket, struct user *leaver)
{
    // Find the correct session..

    for (int i = 0; i < MAX_USERS; i++)
    {
        if (leaver->chatRoom->users[i] == leaver->session_id)
        {
            fprintf(stderr, "Removing %s\n", leaver->name);
            // Remove the user from the list of active users
            leaver->chatRoom->users[i] = -1;
            leaver->active = -1;

            // Close the session if their are no users in this session...
            int active_users = 0;
            for (int j = 0; j < MAX_USERS; j++)
            {
                if (leaver->chatRoom->users[i] == -1)
                {
                    active_users++;
                }
            }

            if (active_users == MAX_USERS)
            {
                closeSession(leaver->chatRoom);
                active_sessions--;
            }

            leaver->chatRoom = NULL;
            return i;
        }
    }

    return -1;
}

//
void query(int socket)
{
    char active_session_names[MAX_MSG] = " ";
    char active_users[MAX_MSG] = " ";

    // Go through all the active sessions and grab the names...
    for (int i = 0; i < MAX_SESSIONS; i++)
    {
        if (client_sessions[i].active == 1)
        {
            strcat(active_session_names, client_sessions[i].name);
            strcat(active_session_names, ", ");
        }
    }

    fprintf(stderr, "Active sessions: %s", active_session_names);

    // Go through all the clients and grab them...
    int found_users = 0;
    for (int i = 0; i < MAX_USERS; i++)
    {
        if (clients[i].active > 0)
        {
            if (found_users == 0)
            {
                strcpy(active_users, clients[i].name);
                strcat(active_users, ", ");
            }
            else
            {
                strcat(active_users, clients[i].name);
                strcat(active_users, ", ");
            }
            found_users++;
        }
    }

    fprintf(stderr, "Active users: %s", active_users);

    // Now send some acknowledgements...
    // Send the user an acknowledgement...
    char ACK[BUFFER_SIZE];
    char ACK_MSG[BUFFER_SIZE];

    // Compose the ACK message
    memset(ACK_MSG, '\0', BUFFER_SIZE);
    sprintf(ACK_MSG, "%s%s%s%s", "Active sessions |", active_session_names, "\nActive users |", active_users);

    createSegment(ACK, QU_ACK, ACK_MSG);
    fprintf(stderr, "%s\n", ACK);

    if (send(socket, ACK, BUFFER_SIZE, 0) == -1)
    {
        fprintf(stderr, "Failed to send query");
    }

    return;
}

//
void sendSessionMSG(int socket, struct user *sender, char *msg)
{

    // Figure out which session the user is apart of...
    int *users_in_session = sender->chatRoom->users;
    char ACK[BUFFER_SIZE];
    memset(ACK, '\0', BUFFER_SIZE);
    int size = strlen(msg);
    sprintf(ACK, "%d:%d:%s:%s", MESSAGE, size, sender->name, msg);

    for (int i = 0; i < MAX_USERS; i++)
    {
        // If there are users active in this session then forward the message
        if (users_in_session[i] != -1 && users_in_session[i] != sender->session_id)
        {
            fprintf(stderr, "Found a user! %d\n", users_in_session[i]);

            int user_id = users_in_session[i];

            send(clients[user_id].sockfd, ACK, BUFFER_SIZE, 0);
        }
    }
    return;
}