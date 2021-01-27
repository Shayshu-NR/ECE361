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
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if ((rv = getaddrinfo(server_add, server_port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
        p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        return 2;
    }
    if ((numbytes = sendto(sockfd, message, strlen(message), 0,
        p->ai_addr, p->ai_addrlen)) == -1) {
        perror("talker: sendto");
        exit(1);
    }
    freeaddrinfo(servinfo);
    printf("talker: sent %d bytes to %s:%s\n", numbytes, server_add,server_port);
    close(sockfd);

    char* client_port = "10000";
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, client_port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
            p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }
    freeaddrinfo(servinfo);

    printf("listener: waiting to recvfrom...\n");
    addr_len = sizeof (their_addr);
    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
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
    close(sockfd);
    return 0;
}