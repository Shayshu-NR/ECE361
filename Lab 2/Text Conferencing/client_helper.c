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

//---------- Server helper functions ----------
char *concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1) + strlen(s2) + 2); // +1 for the null-terminator
    // in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, ";");
    strcat(result, s2);
    return result;
}

// Create a buffer segment depending on what we want to send
int createSegment(char *PtoS, int packet_type)
{
    memset(PtoS, '\0', MAX_MSG);
    int bytes_read = -1;

    // Login packet
    if (packet_type == LOGIN)
    {
        int size = strlen(password);
        bytes_read = sprintf(PtoS, "%d:%d:%s:%s", LOGIN, size, client_id, password);
    }

    // Join session packet
    else if (packet_type == JOIN)
    {
        int size = strlen(session_name);
    }

    else if (packet_type == NEW_SESS)
    {
        int size = strlen(session_name);
        bytes_read = sprintf(PtoS, "%d:%d:%s:%s", NEW_SESS, size, client_id, session_name);
    }

    //
    return bytes_read;
}

// Upon receiving a data buffer, parse it and fill the cmd structure
// with all the necessary info....
void parseBuffer(char *buffer, struct message *cmd)
{

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

// Log the user into the server
void login(char *PtoS, int socket)
{
    // Create a segment to send to the server...
    createSegment(PtoS, LOGIN);

    // Now send the segment
    if (send(socket, PtoS, MAX_MSG, 0) < 0)
    {
        fprintf(stderr, "Send login error\n");
        return;
    }

    // Wait for the acknowledgement
    char ACK[BUFFER_SIZE];
    memset(ACK, '\0', BUFFER_SIZE);

    if (recv(socket, ACK, BUFFER_SIZE, 0) < 0)
    {
        fprintf(stderr, "Receiver login ACK error\n");
        return;
    }

    // Parse the ACK and see if the user was successfully
    // logged in....
    struct message server_res;
    parseBuffer(ACK, &server_res);
    if (server_res.type == LO_ACK)
    {
        logged_in = true;
    }
    fprintf(stderr, "%s\n", server_res.data);

    return;
}

// Log the user out of the server
void logout(char *PtoS, int socket)
{
    return;
}

//
void joinSession(char *PtoS, int socket)
{
    // Create a segment to join a session
    createSegment(PtoS, JOIN);

    // Now send the segment
    if (send(socket, PtoS, MAX_MSG, 0) < 0)
    {
        fprintf(stderr, "Send login error\n");
        return;
    }

    return;
}

void createSession(char *PtoS, int socket)
{
    // Create a segment to create a new session
    createSegment(PtoS, NEW_SESS);

    // Now send the segment
    if (send(socket, PtoS, MAX_MSG, 0) < 0)
    {
        fprintf(stderr, "Send login error\n");
        return;
    }

    // Wait for the acknowledgement
    char ACK[BUFFER_SIZE];
    memset(ACK, '\0', BUFFER_SIZE);
    
    if (recv(socket, ACK, BUFFER_SIZE, 0) < 0)
    {
        fprintf(stderr, "Receiver login ACK error\n");
        return;
    }
    fprintf(stderr, "%s\n", ACK);

    // Parse the ACK and see if the session was
    // successfully created
    struct message server_res;
    parseBuffer(ACK, &server_res);
    if (atoi(server_res.data) >= 0)
    {
        fprintf(stderr, "Created session %s\n", session_name);
        session_id = atoi(server_res.data);
    }
    else
    {
        fprintf(stderr, "Failed to create session, try again\n");
        session_id = -1;
    }
    return;
}
//---------------------------------------------