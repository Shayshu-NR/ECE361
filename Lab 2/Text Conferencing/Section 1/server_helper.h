#ifndef server_helper
#define server_helper

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
#define MAX_USERS 10
#define MAX_SESSIONS 10
#define MAX_MSG 2000
#define MAX_NAME 100
#define MAX_PASS 250
#define BACKLOG 10
#define MAX_SESSION 100
#define BUFFER_SIZE 2500

struct message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_MSG];
};

struct session {
    char name[MAX_SESSION];
    int users[MAX_USERS];
    int session_id;
    int active;
};

struct user{
    char name[MAX_NAME];
    char ip_addr[INET_ADDRSTRLEN];
    struct session * chatRoom;
    int port;
    int sockfd; 
    int active;
    int session_id;
};

char usernames[MAX_USERS][MAX_NAME];
char passwords[MAX_USERS][MAX_PASS];
struct user clients[MAX_USERS];
int client_sockets[MAX_USERS];
struct session client_sessions[MAX_SESSIONS];
int active_sessions;

void *get_in_addr(struct sockaddr *sa);

char *getfield(char *line, int num);

int getUser(char * name);

int createSegment(char * PtoS, int packet_type, char * msg);

void getClientDataFromDB(char * path, int col, int password);

int login(int socket, struct sockaddr_storage * client_addr);

int logoutClient(int socket, struct user * leaver);

int verify(char * id, char *pass);

void parseBuffer(char * buffer, struct message * cmd);

int createSession(char * session_name, int socket, struct user * creator);

void initSession(struct session * room);

void closeSession(struct session * room);

int joinClientSession(char * session_name, int socket, struct user * joiner);

int leaveClientSession(char * session_name, int socket, struct user * leaver);

int oneSession(struct user * joiner);

void query(int socket);

void sendSessionMSG(int socket, struct user * sender, char * msg);
#endif