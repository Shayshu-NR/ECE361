//Usage: deliver <server address><server port number>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define USER_INPUT_LENGTH 100
#define MAXBUFLEN 100

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main (int argc, char *argv[]) {
    if (argc != 3){
        fprintf(stderr,"usage: deliver <server address><server port number>\n");
        exit(1);
    }
    char* server_add = argv[1];
    char* server_port = argv[2];

    char user_input[USER_INPUT_LENGTH];
    printf("Enter: ftp <filename>\n");
    fgets(user_input,USER_INPUT_LENGTH, stdin);
    if (strncmp("ftp",user_input,3)!=0){
        fprintf(stderr,"usage: ftp <filename>\n");
        exit(1);
    }
    char* file_name_short = user_input+4;
    file_name_short[strlen(file_name_short)-1]='\0';
    FILE * fp;
    fp = fopen(file_name_short, "r");
    if(fp == NULL) {
        perror("Error opening file");
        return(-1);
    }
    //read from file
    fclose(fp);
    
    char* message = "ftp";
    int send_sockfd, receive_sockfd;
    struct addrinfo hints, *servinfo, *clientinfo, *sp, *cp;
    int rv;
    int numbytes;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if ((rv = getaddrinfo(server_add, server_port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    char* client_port = "10000";
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, client_port, &hints, &clientinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and make a socket
    for(sp = servinfo; sp != NULL; sp = sp->ai_next) {
        if ((send_sockfd = socket(sp->ai_family, sp->ai_socktype,
            sp->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }
            break;
    }
    if (sp == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }

    // loop through all the results and make a socket
    for(cp = clientinfo; cp != NULL; cp = cp->ai_next) {
        if ((receive_sockfd = socket(cp->ai_family, cp->ai_socktype,
        cp->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }
        if (bind(receive_sockfd, cp->ai_addr, cp->ai_addrlen) == -1) {
            close(receive_sockfd);
            perror("listener: bind");
            continue;
        }
        break;
    }

    if (cp == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }
    
    if ((numbytes = sendto(send_sockfd, message, strlen(message), 0,
        sp->ai_addr, sp->ai_addrlen)) == -1) {
        perror("talker: sendto");
        exit(1);
    }
    printf("talker: sent %d bytes to %s:%hu\n", numbytes, server_add,ntohs(((struct sockaddr_in *)(sp->ai_addr))->sin_port));

    printf("sent from: %hu\n", ntohs(((struct sockaddr_in *)(cp->ai_addr))->sin_port));
    
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];

    printf("listener: waiting to recvfrom...\n");
    addr_len = sizeof (their_addr);
    if ((numbytes = recvfrom(receive_sockfd, buf, MAXBUFLEN-1 , 0,
    (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }  
    printf("listener: got packet from %s\n",
    inet_ntop(their_addr.ss_family,
    get_in_addr((struct sockaddr *)&their_addr),
    s, sizeof s));
    printf("listener: packet is %d bytes long\n", numbytes);
    buf[numbytes] = '\0';
    printf("listener: packet contains \"%s\"\n", buf);
    close(send_sockfd);
    close(receive_sockfd);
    return 0;
}