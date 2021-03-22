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

/*create a list of clients and store on server*/
struct login logins [MAX_CLIENTS]={0};

bool verbose=false;
int read_passwords ();
int check_password(uint32_t userid, char* password);
int create_listen_socket(char* server_port);
int accept_connection (int sockfd, struct sockaddr_in* their_addr, char* s);
int send_message(int client_sockfd, struct message message);
int receive_message(int client_sockfd, struct client* clients, struct session* sessions);
int decode_message(char* buf, int client_sockfd, struct client* clients, struct session* sessions);
int add_client(struct client* clients, int sockfd, char* ip_addr, int port);
int remove_client(struct client* clients, struct session* sessions, int sockfd);
int add_session(struct session* sessions, uint32_t session_id);
int join_session(struct session* sessions, struct client* clients, uint32_t session_id, uint32_t client_id);
int leave_session(struct session* sessions, struct client* clients, uint32_t session_id, uint32_t client_id);
void broadcast_msg(struct session* sessions, struct client* clients, int client_sockfd,
    struct message* message);
void query_sessions(struct session* sessions, struct client* clients, int client_sockfd, struct message* message);

/*Read from a file to update list of clients, returns 0 if successful, -1 otherwise*/
int read_passwords (){
    FILE* fptr;
    if ((fptr = fopen("password.txt", "r")) == NULL) {
        printf("Error! opening file");
        // Program exits if file pointer returns NULL.
        return -1;
    }
    for (int i=0; i<MAX_CLIENTS;i++){
        uint32_t userid;
        char password [MAX_PASSWORD];
        if (fscanf(fptr, "%d %s", &userid, password)==EOF){
            break;
        }
        logins[i].id = userid;
        strncpy(logins[i].password, password,MAX_PASSWORD);
    }
    fclose(fptr);
    return 0;
}

/*Checks if userid and password combination exists in database, returns 
0 if correct, -1 otherwise */
int check_password(uint32_t userid, char* password){
    for (int i=0; i<MAX_CLIENTS; i++){
        if (logins[i].id!=0){
            if (logins[i].id==userid){
                if (strncmp(logins[i].password,password, MAX_PASSWORD)==0){
                    return 0;
                }
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
    uint32_t client_id, session_id;
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
            client_id = atoi((char*)message.source);
            if (check_password(client_id, (char*)message.data)==0){
                for (int i=0;i<MAX_CLIENTS;i++){
                    if (clients[i].client_id==client_id){
                            /*already logged in*/
                            new_message.type = LO_NAK;
                            strcpy(data_string, "Already logged in.");
                            new_message.size = strlen(data_string);
                            strncpy((char*)new_message.source, (char*)message.source, MAX_NAME);
                            strncpy((char*)new_message.data, data_string, MAX_DATA);
                            send_message(client_sockfd, new_message);
                            return 0;
                    }
                }
                /*password correct*/
                for (int i=0;i<MAX_CLIENTS;i++){
                    if (clients[i].client_id!=0){
                        if (clients[i].sockfd==client_sockfd){
                            if (clients[i].client_id==1){
                                /*login sucesss*/
                                clients[i].client_id = client_id;
                                new_message.type = LO_ACK;
                                strcpy(data_string, empty_string);
                            } else {
                                /*already logged in*/
                                new_message.type = LO_NAK;
                                strcpy(data_string, "Already logged in.");
                            }
                            new_message.size = strlen(data_string);
                            strncpy((char*)new_message.source, (char*)message.source, MAX_NAME);
                            strncpy((char*)new_message.data, data_string, MAX_DATA);
                            send_message(client_sockfd, new_message);
                            break;
                        }
                    }
                }
            } else {
                new_message.type = LO_NAK;
                strcpy(data_string, "Incorrect userid or password.");
                new_message.size = strlen(data_string);
                strncpy((char*)new_message.source, (char*)message.source, MAX_NAME);
                strncpy((char*)new_message.data, data_string, MAX_DATA);
                send_message(client_sockfd, new_message);
            }
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
            session_id = atoi((char*)message.data);
            client_id = atoi((char*)message.source);
            result = join_session(sessions, clients, session_id, client_id);
            if (result<0){
                /*join session failed*/
                new_message.type = JN_NAK;
                if (result==-1){
                    strcpy(data_string, "Client already in session. Session ID:");
                } else if (result==-2){
                    strcpy(data_string, "Client cannot join any more sessions. Session ID:");
                } else if (result==-3){
                    strcpy(data_string, "Not a valid session. Session ID:");
                } else if (result==-4){
                    strcpy(data_string, "Client does not exist. Session ID:");
                } else {
                    strcpy(data_string, "Unknown error. Session ID:");
                }
                strcat(data_string,(char*)message.data);
            } else {
                /*join session succeeded*/
                new_message.type = JN_ACK;
                strcpy(data_string, (char*)message.data);
            }
            new_message.size = strlen(data_string);
            strncpy((char*)new_message.source, (char*)message.source, MAX_NAME);
            strncpy((char*)new_message.data, data_string, MAX_DATA);
            send_message(client_sockfd, new_message);
            break;
        case LEAVE_SESS:
            //session_id = atoi((char*)message.data);
            client_id = atoi((char*)message.source);
            for (int i=0;i<MAX_CLIENTS;i++){
                if (clients[i].sockfd==client_sockfd){
                    for (int j=0;j<MAX_SESSIONS;j++){
                        if (clients[i].sessions[j]!=0){
                            session_id = clients[i].sessions[j];
                        }
                    }
                }
            }
            result = leave_session(sessions, clients, session_id, client_id);
            if (verbose){
                if (result<0){
                    switch (result){
                        case -1: printf("Client not in session.\n"); break;
                        case -2: printf("Client does not exist.\n"); break;
                        case -3: printf("Unknown error.\n"); break;
                    }
                }
            }
            break;
        case NEW_SESS:
            if (rv<4){
                printf("Error decoding message!\n");
                return -1;
            }
            session_id = atoi((char*)message.data);
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
            broadcast_msg(sessions, clients, client_sockfd, &message);
            break;
        case QUERY:
            query_sessions(sessions, clients, client_sockfd, &message);
            break;
        default:
            printf("Error decoding message!\n");
            return -1;
    }
    return 0;
}

/*Adds client to client list, assigns one to client id (unassigned id), returns 0 if sucesssful, 
-1 if client list is full*/
int add_client(struct client* clients, int sockfd, char* ip_addr, int port) {
    for (int i=0;i<MAX_CLIENTS;i++){
        if (clients[i].client_id==0){
            clients[i].client_id = 1; 
            clients[i].sockfd = sockfd;
            strncpy(clients[i].ip_addr, ip_addr, INET_ADDRSTRLEN);
            clients[i].port = port;
            memset(clients[i].sessions, 0, sizeof(uint32_t)*MAX_SESSIONS);
            return 0;
        }
    }
    /*no more clients*/
    return -1;
}

/*Removes client from client list, return 0 if removed, -1 if it does not exist*/
int remove_client(struct client* clients, struct session* sessions, int sockfd){
    for (int i=0;i<MAX_CLIENTS;i++){ //find our client, if exists
        if (clients[i].sockfd==sockfd){ 
            for (int j=0;j<MAX_SESSIONS;j++){ //find sessions client is in
                uint32_t session_id = clients[i].sessions[j];
                if ((session_id)!=0){
                    leave_session(sessions, clients, session_id, clients[i].client_id);
                }
            }
            clients[i].sockfd = 0;
            clients[i].client_id = 0; //remove client from client list
            return 0;
        }
    }
    /*client does not exist*/
    return -1;
}

/*Creates a new session, 0 if successful, -1 if session already exists, 
-2 if no more sessions can be created */
int add_session(struct session* sessions, uint32_t session_id){
    int new_session_index = -1;
    for (int i=0;i<MAX_SESSIONS;i++){
        if (sessions[i].session_id == session_id){
            /* session already exists*/
            return -1;
        } else if (sessions[i].session_id == 0){
            if (new_session_index==-1){
                /*add new session here*/
                new_session_index = i;
            }
        }
    }
    if (new_session_index!=-1){
        /*create new session*/
        sessions[new_session_index].session_id = session_id;
        memset(sessions[new_session_index].clients, 0, sizeof(uint32_t)*MAX_CLIENTS);
        return 0;
    } else {
        /*no sessions space */
        return -2;
    }
}

/*Helps client join session, 0 if successful, -1 if client is already in session, 
-2 if client cannot join any more sessions, -3 if not a valid session, -4 if client does not exist, 
-5 otherwise (session is guarenteed to have space) */
int join_session(struct session* sessions, struct client* clients, uint32_t session_id, uint32_t client_id){
    for (int i=0;i<MAX_CLIENTS;i++){ //find client in clients
        if (clients[i].client_id==client_id){ //found client
            for (int j=0;j<MAX_SESSIONS;j++){
                if (clients[i].sessions[j]==session_id){
                    /*client already in session*/
                    return -1;
                } else if (clients[i].sessions[j]!=0){
                    /*client in a session already*/
                    return -2;
                } else {
                    /*client can join session*/
                    for (int k=0;k<MAX_SESSIONS;k++){//find session in sessions
                        if (sessions[k].session_id == session_id){
                            /*guarenteed to have space, but just in case*/
                            for (int l=0;l<MAX_CLIENTS;l++){
                                if (sessions[k].clients[l]==0){
                                    sessions[k].clients[l]=client_id;
                                    clients[i].sessions[j]=session_id;
                                    return 0;
                                }
                            }
                            return -5;
                        }     
                    }
                    /*session does not exist*/
                    return -3;
                }
            }
        }
    } 
    /*client does not exist*/
    return -4;
}

/*Helps client leave session, 0 if successful, -1 if client is not in session, 
-2 if client does not exist, -3 otherwise, removes session is last one to leave*/
int leave_session(struct session* sessions, struct client* clients, uint32_t session_id, uint32_t client_id){
    for (int i=0;i<MAX_CLIENTS;i++){ //find client in clients
        if (clients[i].client_id==client_id){ //found client
            for (int j=0;j<MAX_SESSIONS;j++){
                if (clients[i].sessions[j]==session_id){
                    /*remove session from client*/
                    clients[i].sessions[j]=0;
                    for (int k=0;k<MAX_SESSIONS;k++){//find session in sessions
                        if (sessions[k].session_id == session_id){
                            /*found session*/
                            int remaining = 0;
                            for (int l=0;l<MAX_CLIENTS;l++){
                                if (sessions[k].clients[l]==client_id){
                                    /*remove client from session*/
                                    sessions[k].clients[l]=0;
                                } else if (sessions[k].clients[l]!=0){
                                    remaining++;
                                }
                            }
                            if (remaining==0){
                                /*delete session*/
                                sessions[k].session_id = 0;
                            }
                            return 0;
                        }    
                    }
                    /*unknown error*/
                    return -3;
                }
            }
            /*client is not in session*/
            return -1;
        }
    }
    /*client does not exist*/
    return -2;
}

/*Sends the from the client to everyone else on the same session*/
void broadcast_msg(struct session* sessions, struct client* clients, int client_sockfd, struct message* message){
    uint32_t client_id = atoi((char*)message->source);
    struct message new_message;
    char data_string[MAX_DATA];
    for (int i=0;i<MAX_CLIENTS;i++){ //find client in clients
        if (clients[i].client_id==client_id){
            /*found client*/
            for (int j=0;j<MAX_SESSIONS;j++){//find session in client
                if (clients[i].sessions[j]!=0){
                    /*found session*/
                    for (int k=0;k<MAX_SESSIONS;k++){//find session in sessions
                        if (sessions[k].session_id==clients[i].sessions[j]){
                            for (int l=0;l<MAX_CLIENTS;l++){ //find clients in session
                                if ((sessions[k].clients[l]!=0)&&(sessions[k].clients[l]!=client_id)){
                                    for (int m=0;m<MAX_CLIENTS;m++){//find client information
                                        if (clients[m].client_id==sessions[k].clients[l]){
                                            send_message(clients[m].sockfd, *message);
                                            break;
                                        }
                                    }
                                }
                            }
                            return;
                        }
                    }
                    strcpy(data_string, "Error finding session.");
                    goto print_client;
                }
            }
            strcpy(data_string, "Client not in any sessions.");
            goto print_client;
        }
    }
    strcpy(data_string, "Client not found.");
    print_client: 
        new_message.type = MESSAGE;
        new_message.size = strlen(data_string);
        strncpy((char*)new_message.source, (char*)message->source, MAX_NAME);
        strncpy((char*)new_message.data, data_string, MAX_DATA);
        send_message(client_sockfd, new_message);
    return;
}

/*Creates and sends a message with 
all the clients and sessions and the clients in those session*/
void query_sessions(struct session* sessions, struct client* clients, int client_sockfd, struct message* message){
    struct message new_message;
    char data_string[MAX_DATA]="";
    strcat(data_string,"Clients:");
    for (int i=0;i<MAX_CLIENTS;i++){
        if (clients[i].client_id>1){
            char temp[MAX_DATA];
            sprintf(temp," %d", clients[i].client_id);
            strcat(data_string,temp);
        }
    }
    char temp[MAX_DATA];
    sprintf(temp," \tSessions: ");
    strcat(data_string,temp);
    for (int i=0;i<MAX_SESSIONS;i++){
        if (sessions[i].session_id!=0){
            char temp[MAX_DATA];
            sprintf(temp,"%d:", sessions[i].session_id);
            strcat(data_string,temp);
            for (int j=0;j<MAX_CLIENTS;j++){
                if (sessions[i].clients[j]>0){
                    char temp[MAX_DATA];
                    sprintf(temp," %d", sessions[i].clients[j]);
                    strcat(data_string,temp);
                }
            }
            sprintf(temp,"\t");
            strcat(data_string,temp);
        }
    }
    new_message.type = QU_ACK;
    new_message.size = strlen(data_string);
    strncpy((char*)new_message.source, (char*)message->source, MAX_NAME);
    strncpy((char*)new_message.data, data_string, MAX_DATA);
    send_message(client_sockfd, new_message);
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

    int wait=0;
    while (true){
        if (verbose){
            if (wait==4){
                printf("Waiting...\n");
                wait=0;
            } else {
                wait++;
            }
        }
        struct timeval wait_time;
        wait_time.tv_sec = MAX_WAIT_TIME;
        wait_time.tv_usec = 0; 
        fd_set readfds;
        int max_fd = accept_sockfd+1;
        FD_ZERO(&readfds);
        FD_SET(accept_sockfd, &readfds);
        for (int i=0;i<MAX_CLIENTS;i++){
            if (clients[i].client_id!=0){
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
            if (clients[i].client_id!=0){
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
    } //infinite loop
    close(accept_sockfd);
    return 0;
}