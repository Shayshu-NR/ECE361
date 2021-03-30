#ifndef client_helper
#define client_helper

#define LOGIN 1
#define LO_ACK 2  
#define LO_NACK 3
#define EXIT 4
#define JOIN 5
#define JN_ACK 6
#define JN_NACK 7
#define LEAVE_SESS 8
#define NEW_SESS 10
#define NS_ACK 11
#define MESSAGE 12
#define QUERY 13
#define QU_ACK 14
#define LOGOUT 15
#define NEW_INV 16
#define INV_RES 17

#define MAX_USERS 10
#define MAX_SESSIONS 10
#define MAX_MSG 2500
#define MAX_NAME 100
#define MAX_PASS 250
#define MAX_SESSION 100
#define BACKLOG 10
#define BUFFER_SIZE 2500

bool logged_in;
char client_id[MAX_NAME];
char password[MAX_PASS];
char server_ip[INET_ADDRSTRLEN];
char port[5];
char session_name[MAX_SESSION];
char join_session_name[MAX_SESSION];
char leave_session_name[MAX_SESSION];
int session_id;
char current_session[MAX_SESSIONS][MAX_SESSION];
char msg[MAX_MSG];
char invite_user[MAX_NAME];
char invite_session[MAX_SESSION];

struct message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char session[MAX_SESSION];
    unsigned char data[MAX_MSG];
};

char *concat(const char *s1, const char *s2);

void parseBuffer(char *buffer, struct message *cmd);

int createSegment(char * PtoS, int packet_type);

void login(char * PtoS, int socket);

void logout(char * PtoS, int socket);

void joinSession(char *PtoS, int socket, char * session_name);

void createSession(char *PtoS, int socket);

void leaveSession(char *PtoS, int socket, char * session_name);

void list(char *PtoS, int socket);

void sendMessage(char *PtoS, int socket, char * msg);

void inviteSession(char *PtoS, int socket, char * user_name, char * session_name);
#endif