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

/*Creates socket with a given address and ip for TCP, returns sockfd*/
int create_socket(char* server_addr, char* server_port){
    struct addrinfo hints, *servinfo, *client_ptr;
    int sockfd, rv;

    memset(&hints, 0, sizeof (hints));
    hints.ai_family = AF_INET; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(server_addr, server_port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and bind to the first we can
    for(client_ptr = servinfo; client_ptr != NULL; client_ptr = client_ptr->ai_next) {
        if ((sockfd = socket(client_ptr->ai_family, client_ptr->ai_socktype,
            client_ptr->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        if (connect(sockfd, client_ptr->ai_addr, client_ptr->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }
        break;
    }

    if (client_ptr == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        exit(2);
    }

    freeaddrinfo(servinfo);
    return sockfd;
}

/*Sends message to server, returns number of bytes sent*/
int send_message(int client_sockfd, char* message) {
    int numbytes;
    if ((numbytes = send(client_sockfd, message, strlen(message)+1, 0)) == -1) {
        perror("talker: sendto");
        exit(1);
    }
    if (verbose){
        printf("Message sent: %s", message);
    }
    return numbytes;
}

/*Receives message from server, returns number of bytes sent*/
int receive_message(int client_sockfd, char* buf){
    int numbytes;
    if ((numbytes = recv(client_sockfd, buf, BUFLEN, 0)) == -1) {
        perror("talker: recv");
        exit(1);
    }
    if (verbose){
        printf("Message received: %s", buf);
    }
    return numbytes;
}

/*Gets command from client, parses it as one of the enum*/
enum client_command get_command (char* client_keyword){
    enum client_command client_command;
    if (strcmp(client_keyword, "/login")==0){
        client_command = LOGON;
    } 
    else if (strcmp(client_keyword, "/logout")==0){
        client_command = LOGOUT;
    }
    else if (strcmp(client_keyword, "/joinsession")==0){
        client_command = JOINSESSION;
    }
    else if (strcmp(client_keyword, "/leavesession")==0){
        client_command = LEAVESESSION;
    }
    else if (strcmp(client_keyword, "/createsession")==0){
        client_command = CREATSESSION;
    }
    else if (strcmp(client_keyword, "/list")==0){
        client_command = LIST;
    }
    else if (strcmp(client_keyword, "/quit")==0){
        client_command = QUIT;
    }
    else {
        client_command = TEXT;
    }
    return client_command;
}

int main(int argc, char *argv[]){
    if (argc>1&&(strcmp(argv[1], "-v")==0)){
        verbose = true;
    }
    char client_keyword [MAX_CLIENT_INPUT];
    if (scanf("%s", client_keyword)==EOF){
        printf("Error getting input\n");
        return -1;
    }

    enum client_command client_command = get_command(client_keyword);

    int client_id, session_id, sockfd;
    switch (client_command) {
        case LOGON : ;
            char password [MAX_PASSWORD];
            char server_ip [INET_ADDRSTRLEN];
            char port [MAX_CLIENT_INPUT];
            if (scanf("%d %s %s %s", &client_id, password, server_ip, port)==EOF){
                printf("Error getting input\n");
                return -1;
            }
            sockfd = create_socket(server_ip,port);
            char buf [BUFLEN] = "Hello";
            send_message(sockfd, buf);
            receive_message(sockfd, buf);
            printf("Echo: %s\n", buf);
            break;
        case LOGOUT :
            printf("%d\n", client_command); 
            break;
        case JOINSESSION :
            if (scanf("%d", &session_id)==EOF){
                printf("Error getting input\n");
                return -1;
            }
            printf("%d\n", client_command);         
            break;
        case LEAVESESSION :
            printf("%d\n", client_command);   
            break;
        case CREATSESSION :
            if (scanf("%d", &session_id)==EOF){
                printf("Error getting input\n");
                return -1;
            }
            printf("%d\n", client_command);   
            break;
        case LIST :
            printf("%d\n", client_command);   
            break;
        case QUIT :
            printf("%d\n", client_command); 
            break;
        case TEXT :
            printf("%d\n", client_command);  
            break;
    }
    return 0;
}