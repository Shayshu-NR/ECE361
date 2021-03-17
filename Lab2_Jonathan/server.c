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
#include "structs.h"

bool verbose=false;

/*Read from a file to update list of clients, returns 0 if successful, -1 otherwise*/
int read_passwords (struct login* logins){
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
userid if correct, -1 otherwise */
int check_password(uint32_t userid, char* password, struct login* logins){
    for (int i=0; i<MAX_CLIENTS; i++){
        if (logins[i].id!=0){
            if (logins[i].id==userid){
                if (strncmp(logins[i].password,password, MAX_PASSWORD)==0){
                    if (verbose){
                        printf("User %d successfully logged on.\n", logins[i].id);
                    }
                    return logins[i].id;
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

/*Sends message to client socket, returns number of bytes sent*/
int send_message(int client_sockfd, char* message) {
    int numbytes;
    if ((numbytes = send(client_sockfd, message, strlen(message)+1, 0)) == -1) {
        perror("talker: sendto");
        exit(1);
    }
    return numbytes;
}

/*Receives message from client socket, returns number of bytes sent*/
int receive_message(int client_sockfd, char* buf){
    int numbytes;
    if ((numbytes = recv(client_sockfd, buf, BUFLEN, 0)) == -1) {
        perror("talker: recv");
        exit(1);
    }
    return numbytes;
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
    return -1;
}

/*Removes client from client list, return 0 if removed, -1 if it does not exist*/
int remove_client(struct client* clients, uint32_t client_id){
    for (int i=0;i<MAX_CLIENTS;i++){
        if (clients[i].client_id==client_id){
            clients[i].client_id = 0; 
            return 0;
        }
    }
    return -1;
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
    /*create a list of clients and store on server*/
    struct login logins [MAX_CLIENTS]={0};
    if (read_passwords (logins)){
        printf("Failed to read passwords!");
        return 0;
    }

    /*Create a list of sessions and clients*/
    struct session sessions [MAX_SESSIONS] = {0};
    struct client clients [MAX_CLIENTS] = {0};
    int accept_sockfd = create_listen_socket(server_port);
    int i=0;
    /*Loop for listening to connection requests and client messages*/
    while (true){
        printf("Loop: %d\n", i);
        struct timeval wait_time;
        wait_time.tv_sec = MAX_WAIT_TIME;
        wait_time.tv_usec = 0; //1s
        fd_set readfds;
        int max_fd = accept_sockfd+1;
        FD_ZERO(&readfds);
        FD_SET(accept_sockfd, &readfds);
        for (int i=0;i<MAX_CLIENTS;i++){
            if (clients[i].client_id!=0){
                FD_SET(clients[i].sockfd, &readfds);
                if (clients[i].sockfd>max_fd){
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
            if (new_fd!=-1){
                if (add_client(clients, new_fd, s, ntohs(their_addr.sin_port))){
                    printf("No more client space!\n");
                }
            }
            ;
            i++;
            continue;
        }
        /*Check if any messages received*/
        for (int i=0;i<MAX_CLIENTS;i++){
            if (clients[i].client_id!=0){
                if (FD_ISSET(clients[i].sockfd, &readfds)){
                    char buf[BUFLEN];
                    if (receive_message(clients[i].sockfd, buf)!=-1){
                        if (verbose) {
                            printf("New message: %s", buf);
                        }
                    }
                    /*echo the message*/
                    send_message(clients[i].sockfd, buf);
                }
            }
        }
        i++;
    }


    close(accept_sockfd);
    return 0;
}