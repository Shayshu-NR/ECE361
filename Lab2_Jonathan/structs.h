#ifndef STRUCTS_HEADER
#define STRUCTS_HEADER
#include <inttypes.h>

#define MAX_NAME 10
#define MAX_DATA 256
#define MAX_PASSWORD 32
#define MAX_CLIENTS 10
#define MAX_CLIENT_INPUT 256
#define MAX_SESSIONS 10
#define BACKLOG 10
#define BUFLEN 1000
#define MAX_WAIT_TIME 1
#define MAX_ID 20
#define STDIN 0
#define TIMEOUT_SEC 30

struct message {
    uint32_t type;
    uint32_t size;
    unsigned char source [MAX_NAME];
    unsigned char data [MAX_DATA];
};

struct login {
    char id[MAX_ID];
    char password [MAX_PASSWORD];
};

enum control_types {
    LOGON, LO_ACK, LO_NAK, EXIT, JOIN, JN_ACK, JN_NAK, LEAVE_SESS, NEW_SESS, NS_ACK, MESSAGE, QUERY, QU_ACK, INV_SEND, INV_ACK, INV_NAK, INV_RECV, TIMEOUT
};

enum client_command {
    LOGIN, LOGOUT, JOINSESSION, LEAVESESSION, CREATSESSION, LIST, QUIT, TEXT, INV, ACCEPT, REJECT
};

/*an instance of a session, has a list of clients_id belonging to the session
session_id is a string and clients is indexes 0 to MAX_CLIENTS -1, 
clients -1 means not a client */
struct session {
    char session_id[MAX_ID];
    uint32_t clients[MAX_CLIENTS];
};

/*a connected client on the server and a list of sessions it is in 
client_id is a string and sessions is indexes 0 to MAX_SESSIONS -1,
client_id is 'USER' if connection is established but not logged in
sessions -1 means means not a session*/
struct client {
    char client_id[MAX_ID];
    int sockfd;
    char ip_addr [INET_ADDRSTRLEN];
    int port;
    uint32_t sessions [MAX_SESSIONS];
    time_t most_recent_cmd;
};

#endif