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
bool login(int socket)
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
        client_sockets[verified] = socket;
        createSegment(ACK, LO_ACK, "Login successful");
        send(socket, ACK, BUFFER_SIZE, 0);
        return true;
    }
}

// Create a control segment to send to the client
int createSegment(char *PtoS, int packet_type, char *msg)
{
    memset(PtoS, '\0', BUFFER_SIZE);
    int bytes_read;

    int size = strlen(msg);
    bytes_read = sprintf(PtoS, "%d:%d:%s:%s", packet_type, size, "Server", msg);

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
    return;
}

int createSession(char *session_name, int socket)
{

    if (active_sessions >= MAX_SESSIONS)
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
            if (client_sessions[i].session_id == -1 )
            {
                client_sessions[i].session_id = i;
                strcpy(client_sessions[i].name, session_name);
                active_sessions++;

                // Now send a response to the client...
                char ACK[BUFFER_SIZE];
                memset(ACK, '\0', BUFFER_SIZE);

                char ses_id[MAX_SESSIONS];
                memset(ses_id, '\0', MAX_SESSIONS);
                sprintf(ses_id, "%d", client_sessions[i].session_id);

                createSegment(ACK, NS_ACK, ses_id);
                fprintf(stderr, "%s\n", ACK);

                if(send(socket, ACK, BUFFER_SIZE, 0) < 0){
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