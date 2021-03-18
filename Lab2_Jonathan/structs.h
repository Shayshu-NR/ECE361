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
#define STDIN 0

struct message {
    uint32_t type;
    uint32_t size;
    unsigned char source [MAX_NAME];
    unsigned char data [MAX_DATA];
};

struct login {
    uint32_t id;
    char password [MAX_PASSWORD];
};

enum control_types {
    LOGON, LO_ACK, LO_NAK, EXIT, JOIN, JN_ACK, JN_NAK, LEAVE_SESS, NEW_SESS, NS_ACK, MESSAGE, QUERY, QU_ACK
};

enum client_command {
    LOGIN, LOGOUT, JOINSESSION, LEAVESESSION, CREATSESSION, LIST, QUIT, TEXT, NONE
};

/*an instance of a session, has a list of clients_id belonging to the session
session_id > 0 and clients_id > 0 */
struct session {
    uint32_t session_id;
    uint32_t clients[MAX_CLIENTS];
};

/*a connected client on the server and a list of sessions it is in */
struct client {
    uint32_t client_id;
    int sockfd;
    char ip_addr [INET_ADDRSTRLEN];
    int port;
    uint32_t sessions [MAX_SESSIONS];
};

#endif