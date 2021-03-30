#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include "structs.h"
#include <time.h>

/*create a list of clients and store on server*/
struct login logins [MAX_CLIENTS]={0};

bool verbose=false;
int read_passwords ();
int get_client_index(char* client_id, struct client* clients);
int get_session_index(char* session_id, struct session* sessions);
int check_password(char* userid, char* password);
int create_listen_socket(char* server_port);
int accept_connection (int sockfd, struct sockaddr_in* their_addr, char* s);
int send_message(int client_sockfd, struct message message);
int receive_message(int client_sockfd, struct client* clients, struct session* sessions);
int decode_message(char* buf, int client_sockfd, struct client* clients, struct session* sessions);
int add_client(struct client* clients, int sockfd, char* ip_addr, int port);
int remove_client(struct client* clients, struct session* sessions, int sockfd);
int add_session(struct session* sessions, char* session_id);
int join_session(struct session* sessions, struct client* clients, uint32_t session_index, uint32_t client_index);
int leave_session(struct session* sessions, struct client* clients, uint32_t session_index, uint32_t client_index);
void broadcast_msg(struct session* sessions, struct client* clients, int client_sockfd, 
    uint32_t session_index, struct message* message);
void query_sessions(struct session* sessions, struct client* clients, int client_sockfd, struct message* message);
void check_client_timeouts(struct client* clients, struct session* sessions);

/*Read from a file to update list of clients, returns 0 if successful, -1 otherwise*/
int read_passwords (){
    FILE* fptr;
    if ((fptr = fopen("password.txt", "r")) == NULL) {
        printf("Error! opening file");
        // Program exits if file pointer returns NULL.
        return -1;
    }
    for (int i=0; i<MAX_CLIENTS;i++){
        char userid[MAX_ID];
        char password [MAX_PASSWORD];
        if (fscanf(fptr, "%s%s", userid, password)==EOF){
            break;
        }
        strncpy(logins[i].id,userid, MAX_ID);
        strncpy(logins[i].password, password,MAX_PASSWORD);
    }
    fclose(fptr);
    return 0;
}

/*Helper function that returns index of the client id (string), returns -1 if not found.*/
int get_client_index(char* client_id, struct client* clients){
    for (int i=0;i<MAX_CLIENTS;i++){
        if (strncmp(client_id, clients[i].client_id, MAX_ID)==0){
            return i;
        }
    }
    return -1;
}

/*Helper function that returns index of the session id (string), returns -1 if not found.*/
int get_session_index(char* session_id, struct session* sessions){
    for (int i=0;i<MAX_SESSIONS;i++){
        if (strncmp(session_id, sessions[i].session_id, MAX_ID)==0){
            return i;
        }
    }
    return -1;
}

/*Checks if userid and password combination exists in database, returns 
0 if correct, -1 otherwise */
int check_password(char* userid, char* password){
    for (int i=0; i<MAX_CLIENTS; i++){
        if (strncmp(logins[i].id, userid, MAX_ID)==0){
            if (strncmp(logins[i].password, password, MAX_PASSWORD)==0){
                return 0;
            }
        } 
    }
    return -1;
}

/*Creates socket with a given address and ip for listening to TCP, returns listening sockfd*/
int create_listen_socket(char* server_port){
    struct addrinfo hints, *servinfo, *serv_ptr;
    int sockfd, rv;
    int yes=1;
    memset(&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, server_port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }
    // loop through all the results and bind to the first we can
    for(serv_ptr = servinfo; serv_ptr != NULL; serv_ptr = serv_ptr->ai_next) {
        if ((sockfd = socket(serv_ptr->ai_family, serv_ptr->ai_socktype,
            serv_ptr->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        if (bind(sockfd, serv_ptr->ai_addr, serv_ptr->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }
        break;
    }
    if (serv_ptr == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        exit(2);
    }
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
    freeaddrinfo(servinfo);
    return sockfd;
}

/*Accepts a connection request from a client, returns client_fd, or -1 if unsucessful*/
int accept_connection (int sockfd, struct sockaddr_in* their_addr, char* s){
    socklen_t addr_len = sizeof (struct sockaddr_in);
    int new_fd = accept(sockfd, (struct sockaddr *)their_addr, &addr_len);
    if (new_fd == -1) {
        perror("accept");
        return new_fd;
    }
    inet_ntop(their_addr->sin_family,&their_addr->sin_addr,s, INET_ADDRSTRLEN);
    if (verbose){
        printf("listener: got connection from %s:%hu\n",s, ntohs(their_addr->sin_port));
    }
    return new_fd;
}

/*Sends message to client, returns number of bytes sent, -1 otherwise*/
int send_message(int client_sockfd, struct message message) {
    char buf [BUFLEN];
    int rv = sprintf(buf, "%u %u %s %s", message.type, message.size,
        message.source, message.data);
    if (rv<0){
        printf("Failed to create buf message!\n");
        return -1;
    }

    int numbytes;
    if ((numbytes = send(client_sockfd, buf, rv+1, 0)) == -1) {
        perror("listener: sendto");
        exit(1);
    }
    if (verbose){
        printf("Message sent: %s\n", buf);
    }
    return numbytes;
}

/*Receives message from client socket, returns number of bytes sent*/
int receive_message(int client_sockfd, struct client* clients, struct session* sessions){
    char buf [BUFLEN];
    int numbytes;
    if ((numbytes = recv(client_sockfd, buf, BUFLEN, 0)) == -1) {
        perror("listener: recv");
        exit(1);
    }
    if (verbose){
        printf("Message received: %s\n", buf);
    }
    for (int i=0;i<MAX_CLIENTS;i++){
        /*Update most recent cmd time*/
        if (clients[i].sockfd == client_sockfd){
            clients[i].most_recent_cmd = time(NULL);
            break;
        }
    }
    if (numbytes>0){
        decode_message(buf, client_sockfd, clients, sessions);
    }
    return numbytes;
}

/*Decodes message that server sent*/
int decode_message(char* buf, int client_sockfd, struct client* clients, struct session* sessions){
    struct message message;
    int rv = sscanf(buf, "%u%u%s%*c%[^\n]", &message.type, &message.size, message.source,
        message.data);
    if (rv<3){
        printf("Error decoding message!\n");
        return -1;
    }
    char* client_id, *session_id; 
    char extra_arg[MAX_DATA];
    uint32_t client_index, session_index;
    struct message new_message;
    char empty_string[] = "";
    char data_string[MAX_DATA];
    int result;
    switch (message.type){
        case LOGIN : 
            if (rv<4){
                printf("Error decoding message!\n");
                return -1;
            }
            client_id = (char*)message.source;
            if (check_password(client_id, (char*)message.data)==0){
                /*password correct*/
                if ((client_index=get_client_index(client_id, clients))==-1){//Is client already logged in
                    /*Log client in*/
                    for (int i=0;i<MAX_CLIENTS;i++){//find unregistered client
                        if (strcmp(clients[i].client_id,"USER")==0&&clients[i].sockfd==client_sockfd){
                                /*login sucesss*/
                                strncpy(clients[i].client_id,client_id,MAX_ID);
                                new_message.type = LO_ACK;
                                strcpy(data_string, empty_string);
                                new_message.size = strlen(data_string);
                                strncpy((char*)new_message.source, (char*)message.source, MAX_NAME);
                                strncpy((char*)new_message.data, data_string, MAX_DATA);
                                send_message(client_sockfd, new_message);
                                return 0;
                        }
                    }
                    /*already logged in on this socket*/
                    new_message.type = LO_NAK;
                    strcpy(data_string, "An account is already logged in.");
                } else {
                    /*this client id is already logged in*/
                    new_message.type = LO_NAK;
                    strcpy(data_string, "Already logged in.");
                }
            } else {
                new_message.type = LO_NAK;
                strcpy(data_string, "Incorrect userid or password.");
            }
            new_message.size = strlen(data_string);
            strncpy((char*)new_message.source, (char*)message.source, MAX_NAME);
            strncpy((char*)new_message.data, data_string, MAX_DATA);
            send_message(client_sockfd, new_message);
            break;
        case EXIT:
            /*remove client and close socket*/
            remove_client(clients, sessions,client_sockfd);
            close(client_sockfd);
            break;
        case JOIN:
            if (rv<4){
                printf("Error decoding message!\n");
                return -1;
            }
            session_id = (char*)message.data;
            client_id = (char*)message.source;
            client_index = get_client_index(client_id, clients);
            session_index = get_session_index(session_id, sessions);
            if (client_index==-1||session_index==-1){
                new_message.type = JN_NAK;
                strcpy(data_string, "Invalid Client ID or Session ID");
            } else {
                result = join_session(sessions, clients, session_index, client_index);
                if (result<0){
                    /*join session failed*/
                    new_message.type = JN_NAK;
                    if (result==-1){
                        strcpy(data_string, "Client already in session. Session ID:");
                    } else if (result==-2){
                        strcpy(data_string, "Session has no more spaces. Session ID:");
                    } else {
                        strcpy(data_string, "Client cannot join any more sessions. Session ID:");
                    }
                    strcat(data_string,(char*)message.data);
                } else {
                    /*join session succeeded*/
                    new_message.type = JN_ACK;
                    strcpy(data_string, (char*)message.data);
                }
            }
            new_message.size = strlen(data_string);
            strncpy((char*)new_message.source, (char*)message.source, MAX_NAME);
            strncpy((char*)new_message.data, data_string, MAX_DATA);
            send_message(client_sockfd, new_message);
            break;
        case LEAVE_SESS:
            if (rv<4){
                printf("Error decoding message!\n");
                return -1;
            }
            session_id = (char*)message.data;
            client_id = (char*)message.source;
            client_index = get_client_index(client_id, clients);
            session_index = get_session_index(session_id, sessions);
            if (client_index==-1||session_index==-1){
                //do nothing because we dont send a message back
            } else {
                result = leave_session(sessions, clients, session_index, client_index);
                if (result==-1){
                    if (verbose){
                        printf("Client not in session.\n");
                    }
                }
            }
            break;
        case NEW_SESS:
            if (rv<4){
                printf("Error decoding message!\n");
                return -1;
            }
            session_id = (char*)message.data;
            result = add_session(sessions, session_id);
            if (verbose){
                if (result<0){
                    switch (result){
                        case -1: printf("Session already exists.\n"); break;
                        case -2: printf("No more sessions can be created.\n"); break;
                    }
                }
            }
            if (result==0){
                /*add session succeeded*/
                new_message.type = NS_ACK;
                strcpy(data_string, (char*)message.data);
                new_message.size = strlen(data_string);
                strncpy((char*)new_message.source, (char*)message.source, MAX_NAME);
                strncpy((char*)new_message.data, data_string, MAX_DATA);
                send_message(client_sockfd, new_message);
            }
            break;
        case MESSAGE:
            if (rv<4){
                printf("Error decoding message!\n");
                return -1;
            }
            session_id = malloc(sizeof(char)*MAX_ID);
            sscanf((char*)message.data, "%s", session_id);
            if ((session_index = get_session_index(session_id, sessions))!=-1){
                broadcast_msg(sessions, clients, client_sockfd, session_index, &message);
            }
            free(session_id);
            break;
        case QUERY:
            query_sessions(sessions, clients, client_sockfd, &message);
            break;
        case INV_SEND:
            if (rv<4){
                printf("Error decoding message!\n");
                return -1;
            }
            session_id = malloc(sizeof(char)*MAX_ID);
            rv = sscanf((char*)message.data, "%s%s", extra_arg,session_id);
            if (rv<2){
                printf("Error decoding message!\n");
                return -1;
            }
            if (((client_index=get_client_index(extra_arg, clients))!=-1)&&(get_session_index(session_id, sessions)!=-1)){
                /*Invite recipient and session both exist*/
                new_message.type = INV_RECV;
                strcpy(data_string, (char*)message.data);
                new_message.size = strlen(data_string);
                strncpy((char*)new_message.source, (char*)message.source, MAX_NAME);
                strncpy((char*)new_message.data, data_string, MAX_DATA);
                send_message(clients[client_index].sockfd, new_message);

                new_message.type = INV_ACK;
                send_message(client_sockfd, new_message);
            } else {
                /*Failed*/
                new_message.type = INV_NAK;
                strcpy(data_string, "Client or session does not exist.");
                new_message.size = strlen(data_string);
                strncpy((char*)new_message.source, (char*)message.source, MAX_NAME);
                strncpy((char*)new_message.data, data_string, MAX_DATA);
                send_message(client_sockfd, new_message);
            }
            free(session_id);
            break;
        default:
            printf("Error decoding message!\n");
            return -1;
    }
    return 0;
}

/*Adds client to client list, assigns one to client id (unassigned id), returns 0 if sucesssful, 
-1 if client list is full, EDITED*/
int add_client(struct client* clients, int sockfd, char* ip_addr, int port) {
    for (int i=0;i<MAX_CLIENTS;i++){
        if (strcmp(clients[i].client_id,"")==0){
            strcpy(clients[i].client_id, "USER");
            clients[i].sockfd = sockfd;
            strncpy(clients[i].ip_addr, ip_addr, INET_ADDRSTRLEN);
            clients[i].port = port;
            memset(clients[i].sessions, -1, sizeof(uint32_t)*MAX_SESSIONS);
            clients[i].most_recent_cmd = time(NULL);
            return 0;
        }
    }
    /*no more clients*/
    return -1;
}

/*Removes client from client list, return 0 if removed, -1 if it does not exist, EDITED*/
int remove_client(struct client* clients, struct session* sessions, int sockfd){
    for (int i=0;i<MAX_CLIENTS;i++){ //find our client, if exists
        if (clients[i].sockfd==sockfd){ 
            for (int j=0;j<MAX_SESSIONS;j++){ //find sessions client is in
                uint32_t session_index = clients[i].sessions[j];
                if (session_index!=-1){
                    leave_session(sessions, clients, session_index, i);
                }
            }
            clients[i].sockfd = 0;
            strcpy(clients[i].client_id,""); //remove client from client list
            return 0;
        }
    }
    /*client does not exist*/
    return -1;
}

/*Creates a new session, 0 if successful, -1 if session already exists, 
-2 if no more sessions can be created, EDITED */
int add_session(struct session* sessions, char* session_id){
    if (get_session_index(session_id, sessions)!=-1){
        /*session already exists*/
        return -1;
    }
    for (int i=0;i<MAX_SESSIONS;i++){
        if (strcmp(sessions[i].session_id, "")==0){
            /*Create new session*/
            strncpy(sessions[i].session_id, session_id, MAX_ID);
            memset(sessions[i].clients, -1, sizeof(uint32_t)*MAX_CLIENTS);
            return 0;
        }
    }
    /*No session space*/
    return -2;
}

/*Helps client join session, 0 if successful, -1 if client is already in session, 
-2 if session has no more spaces, -3 if client cannot join anymore sessions, EDITED*/
int join_session(struct session* sessions, struct client* clients, uint32_t session_index, uint32_t client_index){
    for (int i=0;i<MAX_SESSIONS;i++){
        if (clients[client_index].sessions[i]==session_index){
            /*client already in session*/
            return -1;
        }
    }
    for (int i=0;i<MAX_SESSIONS;i++){//find client space
        if (clients[client_index].sessions[i]==-1){
            for (int j=0;j<MAX_CLIENTS;j++){//find session space
                if (sessions[session_index].clients[j]==-1){
                    sessions[session_index].clients[j]=client_index;
                    clients[client_index].sessions[i]=session_index;
                    return 0;
                }
            }
            /*Session has no more spaces*/
            return -2;
        }
    }
    /*Client cannot join anymore sessions*/
    return -3;
}

/*Helps client leave session, 0 if successful, -1 if client is not in session, EDITED*/
int leave_session(struct session* sessions, struct client* clients, uint32_t session_index, uint32_t client_index){
    bool flag=false;
    for (int i=0;i<MAX_SESSIONS;i++){//find session in client
        if (clients[client_index].sessions[i]==session_index){
            /*remove session from client*/
            clients[client_index].sessions[i]=-1;
            flag=true;
            break;
        }
    }
    if (!flag){
        /*Client not in session*/
        return -1;
    }
    int remaining = 0;
    for (int i=0;i<MAX_CLIENTS;i++){//find client in session
        if (sessions[session_index].clients[i]==client_index){
            /*remove client from session*/
            sessions[session_index].clients[i]=-1;
        } else if (sessions[session_index].clients[i]!=-1){
            remaining++;
        }
    }
    if (remaining==0){
        /*delete session*/
        strcpy(sessions[session_index].session_id,"");
    }
    return 0;
}

/*Sends the from the client to everyone else on the same session, EDITED*/
void broadcast_msg(struct session* sessions, struct client* clients, int client_sockfd, uint32_t session_index, struct message* message){
    char* client_id = (char*)message->source;
    struct message new_message;
    char data_string[MAX_DATA];
    int client_index = get_client_index(client_id, clients);
    if (client_index==-1){
        strcpy(data_string, "Client does not exist.");
    } else {
        for (int i=0;i<MAX_SESSIONS;i++){//find session in client
            if (clients[client_index].sessions[i]==session_index){
                /*client is in session*/
                for (int j=0;j<MAX_CLIENTS;j++){
                    /*find all client of sessions*/
                    int recipient_index = sessions[session_index].clients[j];
                    if (recipient_index!=-1&&recipient_index!=client_index){
                        /*Sent message to recipient*/
                        send_message(clients[recipient_index].sockfd, *message);
                    }
                }
                return;
            }
        }
        /*Client isn't in session*/
        strcpy(data_string, "Client not in session.");
    }
    new_message.type = MESSAGE;
    new_message.size = strlen(data_string);
    strncpy((char*)new_message.source, "SERVER", MAX_NAME);
    strncpy((char*)new_message.data, data_string, MAX_DATA);
    send_message(client_sockfd, new_message);
    return;
}

void check_client_timeouts(struct client* clients, struct session* sessions){
    for (int i=0;i<MAX_CLIENTS;i++){
        if (strncmp(clients[i].client_id,"", MAX_ID)!=0){
            if (time(NULL)-clients[i].most_recent_cmd>TIMEOUT_SEC){
                if (verbose){
                    printf("User: %s timed out\n", clients[i].client_id);
                }
                /*Send client timeout message*/
                struct message message;
                message.type = TIMEOUT;
                strcpy((char*)message.data, "");
                message.size = strlen((char*)message.data);
                strncpy((char*)message.source, "SERVER", MAX_NAME);
                send_message(clients[i].sockfd, message);
                /*Remove client*/
                remove_client(clients, sessions, clients[i].sockfd);
                close(clients[i].sockfd);
            }
        }
    }
}


/*Creates and sends a message with 
all the clients and sessions and the clients in those session, EDITED*/
void query_sessions(struct session* sessions, struct client* clients, int client_sockfd, struct message* message){
    struct message new_message;
    char data_string[MAX_DATA]="";
    strcat(data_string,"Clients:");
    for (int i=0;i<MAX_CLIENTS;i++){//print all clients
        if (strcmp(clients[i].client_id,"")!=0){
            strcat(data_string, " ");
            strcat(data_string,clients[i].client_id);
        }
    }
    strcat(data_string,"\tSessions: ");
    for (int i=0;i<MAX_SESSIONS;i++){//print all sessions and clients in those sessions
        if (strcmp(sessions[i].session_id,"")!=0){
            strcat(data_string, sessions[i].session_id);
            strcat(data_string,":");
            for (int j=0;j<MAX_CLIENTS;j++){
                uint32_t client_index = sessions[i].clients[j];
                if (client_index!=-1){
                    strcat(data_string, " ");
                    strcat(data_string,clients[client_index].client_id);
                }
            }
            strcat(data_string,"\t");
        }
    }
    new_message.type = QU_ACK;
    new_message.size = strlen(data_string);
    strncpy((char*)new_message.source, (char*)message->source, MAX_NAME);
    strncpy((char*)new_message.data, data_string, MAX_DATA);
    send_message(client_sockfd, new_message);
    return;
}

int main(int argc, char *argv[]){
    if (argc < 2){
        fprintf(stderr,"usage: server <TCP listen port>\n");
        exit(1);
    }
    if (argc>2&&(strcmp(argv[2], "-v")==0)){
        verbose = true;
    }

    char* server_port = argv[1];
    if (read_passwords()){
        printf("Failed to read passwords!");
        return 0;
    }

    /*Create a list of sessions and clients*/
    struct session sessions [MAX_SESSIONS] = {0};
    struct client clients [MAX_CLIENTS] = {0};
    int accept_sockfd = create_listen_socket(server_port);
    if (fcntl(accept_sockfd, F_SETFL, O_NONBLOCK)){
        printf("Failed to set fnctl!\n");
        return -1;
    }

    while (true){
        struct timeval wait_time;
        wait_time.tv_sec = MAX_WAIT_TIME;
        wait_time.tv_usec = 0; 
        fd_set readfds;
        int max_fd = accept_sockfd+1;
        FD_ZERO(&readfds);
        FD_SET(accept_sockfd, &readfds);
        for (int i=0;i<MAX_CLIENTS;i++){
            if (strcmp(clients[i].client_id,"")!=0){
                FD_SET(clients[i].sockfd, &readfds);
                if (clients[i].sockfd>=max_fd){
                    max_fd = clients[i].sockfd+1;
                }
            }
        }
        select(max_fd, &readfds, NULL, NULL, &wait_time);
        /*Check if new connection, if so, accept it*/
        if (FD_ISSET(accept_sockfd, &readfds)){
            struct sockaddr_in their_addr;
            char s[INET_ADDRSTRLEN];
            int new_fd = accept_connection(accept_sockfd,&their_addr,s);
            if (new_fd!=-1){ //accept new connection
                if (add_client(clients, new_fd, s, ntohs(their_addr.sin_port))){
                    printf("No more client space!\n");
                }
                if (fcntl(new_fd, F_SETFL, O_NONBLOCK)){
                    printf("Failed to set fnctl!\n");
                    return -1;
                }
                continue;
            }
        }
        /*Check if any messages received*/
        for (int i=0;i<MAX_CLIENTS;i++){
            if (strcmp(clients[i].client_id, "")!=0){
                if (FD_ISSET(clients[i].sockfd, &readfds)){
                    int rv = receive_message(clients[i].sockfd, clients, sessions);
                    if (rv==0){
                        /*Client closed connection*/
                        if (verbose){
                            printf("Client timed out\n");
                        }
                        remove_client(clients, sessions, clients[i].sockfd);
                        close(clients[i].sockfd);
                    }
                } //check if sockfd received messages
            } 
        }//check all clients
        /*client timeouts*/
        check_client_timeouts(clients, sessions);
    } //infinite loop
    close(accept_sockfd);
    return 0;
}