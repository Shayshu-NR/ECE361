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
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>

#define USER_INPUT_LENGTH 100
#define MAXBUFLEN 100

bool verbose = false;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main (int argc, char *argv[]) {
    if (argc < 3){
        fprintf(stderr,"usage: deliver <server address><server port number>\n");
        exit(1);
    }
    if ((argc>3)&&(strcmp((argv[3]), "-v")==0)){
        verbose = true;
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
    struct addrinfo hints, *servinfo, *clientinfo, *cp;
    int rv;
    int numbytes;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if ((rv = getaddrinfo(server_add, server_port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    char* client_port = "52520";
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, client_port, &hints, &clientinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and make a socket
    for(cp = clientinfo; cp != NULL; cp = cp->ai_next) {
        if ((sockfd = socket(cp->ai_family, cp->ai_socktype,
        cp->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        if (bind(sockfd, cp->ai_addr, cp->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: bind");
            continue;
        }
        break;
    }

    if (cp == NULL) {
        fprintf(stderr, "client: failed to bind socket\n");
        return 2;
    }
    
    long timer_start, timer_end;
    struct timeval timecheck;

    gettimeofday(&timecheck, NULL);
    timer_start = (long)timecheck.tv_sec * 1000000 + (long)timecheck.tv_usec ;


    if ((numbytes = sendto(sockfd, message, strlen(message), 0,
        servinfo->ai_addr, servinfo->ai_addrlen)) == -1) {
        perror("talker: sendto");
        exit(1);
    }
    if (verbose){
        printf("talker: sent %d bytes to %s:%hu\n", numbytes, server_add,ntohs(((struct sockaddr_in *)(servinfo->ai_addr))->sin_port));
        printf("sent from: %hu\n", ntohs(((struct sockaddr_in *)(cp->ai_addr))->sin_port));
    }
    
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    if (verbose){
        printf("listener: waiting to recvfrom...\n");
    }
    addr_len = sizeof (their_addr);
    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
    (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }

    gettimeofday(&timecheck, NULL);
    timer_end = (long)timecheck.tv_sec * 1000000 + (long)timecheck.tv_usec;

    printf("Roundtrip time: %ldus\n", (timer_end-timer_start));

    buf[numbytes] = '\0';
    if (verbose){
        printf("listener: got packet from %s:%hu\n",
        inet_ntop(their_addr.ss_family,
        get_in_addr((struct sockaddr *)&their_addr),
        s, sizeof s), ntohs(((struct sockaddr_in *)&their_addr)->sin_port));
        printf("listener: packet is %d bytes long\n", numbytes);
        printf("listener: packet contains \"%s\"\n", buf);
    }
    
    if (strcmp(buf,"yes")==0){
        printf("A file tranfer can start.\n");
    }
    freeaddrinfo(servinfo);
    freeaddrinfo(clientinfo);
    close(sockfd);
    return 0;
}