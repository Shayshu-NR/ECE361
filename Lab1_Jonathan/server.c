//Usage: server <UDP listen port>
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
    if (argc < 2){
        fprintf(stderr,"usage: server <UDP listen port>\n");
        exit(1);
    }
    if ((argc>2)&&(strcmp((argv[2]), "-v")==0)){
        verbose = true;
    }
    char* server_port = argv[1];
    int sockfd;
    struct addrinfo hints, *servinfo, *clientinfo, *p, *sp;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, server_port, &hints, &servinfo)) != 0) {
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
    if (verbose){
        printf("listener: waiting to recvfrom...\n");
    }
    addr_len = sizeof (their_addr);
    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1 , 0,
    (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }  
    buf[numbytes] = '\0';
    inet_ntop(their_addr.ss_family,get_in_addr((struct sockaddr *)&their_addr),
        s, sizeof s);
    if (verbose){
        printf("listener: got packet from %s:%hu\n",
        s, ntohs(((struct sockaddr_in *)&their_addr)->sin_port));
        printf("listener: packet is %d bytes long\n", numbytes);
        printf("listener: packet contains \"%s\"\n", buf);
    }

    char message[100];
    if (strcmp(buf,"ftp")==0){
        strcpy(message,"yes");
    } else {
        strcpy(message,"no");
    }

    int client_sockfd;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    char* client_add = s;
    char client_port [100];
    sprintf(client_port,"%hu",ntohs(((struct sockaddr_in *)&their_addr)->sin_port));
    if ((rv = getaddrinfo(client_add, client_port, &hints, &clientinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    // loop through all the results and make a socket
    for(sp = clientinfo; sp != NULL; sp = sp->ai_next) {
        if ((client_sockfd = socket(sp->ai_family, sp->ai_socktype,
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
    if ((numbytes = sendto(client_sockfd, message, strlen(message), 0,
        sp->ai_addr, sp->ai_addrlen)) == -1) {
        perror("talker: sendto");
        exit(1);
    }
    if (verbose){
        printf("talker: sent %d bytes to %s:%hu\n", numbytes, client_add,ntohs(((struct sockaddr_in *)(sp->ai_addr))->sin_port));
    }

    freeaddrinfo(servinfo);
    freeaddrinfo(clientinfo);
    close(client_sockfd);
    close(sockfd);
    return 0;
}