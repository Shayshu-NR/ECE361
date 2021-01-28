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
#include "packet.h"

#define MAXBUFLEN 2000

bool verbose = false;

void create_listen_socket(char* server_port, int* sockfd, struct addrinfo* hints, struct addrinfo** servinfo, struct addrinfo** p){
    int rv;
    memset(hints, 0, sizeof (*hints));
    hints->ai_family = AF_INET; // set to AF_INET to force IPv4
    hints->ai_socktype = SOCK_DGRAM;
    hints->ai_flags = AI_PASSIVE; // use my IP
    if ((rv = getaddrinfo(NULL, server_port, hints, servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }
    // loop through all the results and bind to the first we can
    for(p = servinfo; *p != NULL; *p = (*p)->ai_next) {
        if ((*sockfd = socket((*p)->ai_family, (*p)->ai_socktype,
            (*p)->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }
        if (bind(*sockfd, (*p)->ai_addr, (*p)->ai_addrlen) == -1) {
            close(*sockfd);
            perror("listener: bind");
            continue;
        }
        break;
    }
    if (*p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        exit(2);
    }
}

void create_talking_socket(char* client_add, int* client_sockfd, struct addrinfo* hints, struct sockaddr_in* their_addr, struct addrinfo** clientinfo, struct addrinfo** sp){
    memset(hints, 0, sizeof (*hints));
    hints->ai_family = AF_INET;
    hints->ai_socktype = SOCK_DGRAM;
    char client_port [100];
    int rv;
    sprintf(client_port,"%hu",ntohs(their_addr->sin_port));
    if ((rv = getaddrinfo(client_add, client_port, hints, clientinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }
    // loop through all the results and make a socket
    for(*sp = *clientinfo; *sp != NULL; *sp = (*sp)->ai_next) {
        if ((*client_sockfd = socket((*sp)->ai_family, (*sp)->ai_socktype,
        (*sp)->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }
        break;
    }
    if (*sp == NULL) {
        fprintf(stderr, "talker: failed to create socket\n");
        exit(2);
    }
}

void receive_message(int *sockfd, char* buf, int* numbytes, struct sockaddr_in* their_addr, socklen_t* addr_len, char* s){
    printf("listener: waiting to recvfrom...\n");
    *addr_len = sizeof (struct sockaddr_in);
    if ((*numbytes = recvfrom(*sockfd, buf, MAXBUFLEN-1 , 0,
    (struct sockaddr *)their_addr, addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }  
    buf[*numbytes] = '\0';
    inet_ntop(their_addr->sin_family,&their_addr->sin_addr,s, INET_ADDRSTRLEN);
    if (verbose){
        printf("listener: got packet from %s:%hu\n",s, ntohs(their_addr->sin_port));
        printf("listener: packet is %d bytes long\n", *numbytes);
        //printf("listener: packet contains \"%s\"\n", buf);
    }
}

void send_message(int* client_sockfd, char* message, struct addrinfo* sp, int* numbytes, char*s) {
    if ((*numbytes = sendto(*client_sockfd, message, strlen(message), 0,
        sp->ai_addr, sp->ai_addrlen)) == -1) {
        perror("talker: sendto");
        exit(1);
    }
    if (verbose){
        printf("talker: sent %d bytes to %s:%hu\n", *numbytes, s,ntohs(((struct sockaddr_in *)(sp->ai_addr))->sin_port));
    }
}

int decode_string(const char* string, char** next_seg){
    char* delim_pos = strpbrk(string,":");
    char num[100];
    strncpy(num,string,delim_pos-string);
    num[delim_pos-string]='\0';
    *next_seg = delim_pos+1;
    return atoi(num);
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
    int sockfd,client_sockfd;
    struct addrinfo hints, *servinfo, *clientinfo, *p, *sp;
    
    create_listen_socket(server_port,&sockfd,&hints,&servinfo,&p);

    int numbytes;
    struct sockaddr_in their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
    
    receive_message(&sockfd,buf,&numbytes,&their_addr,&addr_len, s);

    char message[100];
    if (strcmp(buf,"ftp")==0){
        strcpy(message,"yes");
    } else {
        strcpy(message,"no");
    }

    create_talking_socket(s,&client_sockfd,&hints,&their_addr,&clientinfo,&sp);

    send_message(&client_sockfd,message,sp,&numbytes,s);
    
    int total_frags, current_frag=0;
    FILE* new_file;
    while (true){
        receive_message(&sockfd,buf,&numbytes,&their_addr,&addr_len, s);
        //printf("Message received:%ld\n", strlen(buf));
        char* next_seg;
        total_frags = decode_string(buf, &next_seg);
        int new_frag_no = decode_string(next_seg, &next_seg);
        int frag_size = decode_string(next_seg, &next_seg);
        char* delim_pos = strpbrk(next_seg,":");
        char filename[100];
        strncpy(filename,next_seg,delim_pos-next_seg);
        filename[delim_pos-next_seg]='\0';
        next_seg = delim_pos+1;
        if (new_frag_no==current_frag+1){
            current_frag = new_frag_no;
            strcpy(message, "ACK");
            if (current_frag==1){
                new_file = fopen(filename, "w+b");
                if(new_file == NULL) {
                    perror("Error opening file");
                    exit(-1);
                }
                if (verbose){
                    printf("File created: %s\n", filename);
                }
            }
        } else {
            strcpy(message, "NACK");
        }
        fwrite(next_seg,frag_size, 1, new_file);
        send_message(&client_sockfd,message,sp,&numbytes,s);
        if (current_frag==total_frags){
            break;
        }
    }
    fclose(new_file);
    freeaddrinfo(servinfo);
    freeaddrinfo(clientinfo);
    close(client_sockfd);
    close(sockfd);
    return 0;
}