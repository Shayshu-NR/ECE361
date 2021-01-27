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
#include "packet.h"

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

FILE* open_file (){
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
        exit(-1);
    }
    return fp;
}

void create_socket(char* server_add, char* server_port, int* sockfd, struct addrinfo* hints, struct addrinfo** servinfo, struct addrinfo** clientinfo, struct addrinfo** cp){
    memset(hints, 0, sizeof (*hints));
    int rv;
    hints->ai_family = AF_INET;
    hints->ai_socktype = SOCK_DGRAM;
    if ((rv = getaddrinfo(server_add, server_port, hints, servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    char* client_port = "52520";
    memset(hints, 0, sizeof (*hints));
    hints->ai_family = AF_INET; // set to AF_INET to force IPv4
    hints->ai_socktype = SOCK_DGRAM;
    hints->ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, client_port, hints, clientinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and make a socket
    for(*cp = *clientinfo; *cp != NULL; *cp = (*cp)->ai_next) {
        if ((*sockfd = socket((*cp)->ai_family, (*cp)->ai_socktype,
        (*cp)->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        if (bind(*sockfd, (*cp)->ai_addr, (*cp)->ai_addrlen) == -1) {
            close(*sockfd);
            perror("client: bind");
            continue;
        }
        break;
    }

    if (*cp == NULL) {
        fprintf(stderr, "client: failed to bind socket\n");
        exit(2);
    }
}

void send_message (char* server_add,int* sockfd, char* message, struct addrinfo* servinfo, struct addrinfo* cp, int* numbytes){
    if ((*numbytes = sendto(*sockfd, message, strlen(message), 0,
        servinfo->ai_addr, servinfo->ai_addrlen)) == -1) {
        perror("talker: sendto");
        exit(1);
    }
    if (verbose){
        printf("talker: sent %d bytes to %s:%hu\n", *numbytes, server_add,ntohs(((struct sockaddr_in *)(servinfo->ai_addr))->sin_port));
        printf("sent from: %hu\n", ntohs(((struct sockaddr_in *)(cp->ai_addr))->sin_port));
    }
}

void receive_message(int *sockfd, char* buf, int* numbytes){
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    if (verbose){
        printf("listener: waiting to recvfrom...\n");
    }
    addr_len = sizeof (their_addr);
    if ((*numbytes = recvfrom(*sockfd, buf, MAXBUFLEN-1 , 0,
    (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }

    buf[*numbytes] = '\0';
    if (verbose){
        printf("listener: got packet from %s:%hu\n",
        inet_ntop(their_addr.ss_family,
        get_in_addr((struct sockaddr *)&their_addr),
        s, sizeof s), ntohs(((struct sockaddr_in *)&their_addr)->sin_port));
        printf("listener: packet is %d bytes long\n", *numbytes);
        printf("listener: packet contains \"%s\"\n", buf);
    }
}

void get_file_info(FILE *fp, struct packet* next_packet){
    
}

void get_next_packet(FILE *fp, struct packet* next_packet) {

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

    FILE *fp = open_file ();
    
    //read from file
    fclose(fp);
    
    int sockfd;
    struct addrinfo hints, *servinfo, *clientinfo, *cp;  
    
    create_socket(server_add,server_port,&sockfd,&hints,&servinfo,&clientinfo,&cp);

    long timer_start, timer_end;
    struct timeval timecheck;

    gettimeofday(&timecheck, NULL);
    timer_start = (long)timecheck.tv_sec * 1000000 + (long)timecheck.tv_usec ;
    
    char* message = "ftp";
    int numbytes;
    send_message (server_add,&sockfd,message,servinfo,cp,&numbytes);

    char buf[MAXBUFLEN];
    receive_message(&sockfd,buf,&numbytes);
    gettimeofday(&timecheck, NULL);
    timer_end = (long)timecheck.tv_sec * 1000000 + (long)timecheck.tv_usec;

    printf("Roundtrip time: %ldus\n", (timer_end-timer_start));
    
    if (strcmp(buf,"yes")==0){
        printf("A file tranfer can start.\n");
    }

    struct packet next_packet;
    get_file_info(fp,&next_packet);
    get_next_packet(fp,&next_packet);

    freeaddrinfo(servinfo);
    freeaddrinfo(clientinfo);
    close(sockfd);
    return 0;
}