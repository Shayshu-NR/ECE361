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
#define MAX_TIMES 10

bool verbose = false;

/*open file with name and return file pointer*/
FILE* open_file (char* file_name_short){
    char user_input[USER_INPUT_LENGTH];
    printf("Enter: ftp <filename>\n");
    fgets(user_input,USER_INPUT_LENGTH, stdin);
    if (strncmp("ftp",user_input,3)!=0){
        fprintf(stderr,"usage: ftp <filename>\n");
        exit(1);
    }
    strcpy(file_name_short,user_input+4);
    file_name_short[strlen(file_name_short)-1]='\0';
    FILE * fp;
    fp = fopen(file_name_short, "rb");
    if(fp == NULL) {
        perror("Error opening file");
        exit(-1);
    }
    return fp;
}

/*creates socket for udp binds to a certain port and updates servinfo and clientinfo*/
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

/*sends message on udp socket*/
void send_message (char* server_add,int* sockfd, char* message, struct addrinfo* servinfo, struct addrinfo* cp, int* numbytes, int length){
    if ((*numbytes = sendto(*sockfd, message, length, 0,
        servinfo->ai_addr, servinfo->ai_addrlen)) == -1) {
        perror("talker: sendto");
        exit(1);
    }
    if (verbose){
        printf("talker: sent %d bytes to %s:%hu\n", *numbytes, server_add,ntohs(((struct sockaddr_in *)(servinfo->ai_addr))->sin_port));
        printf("sent from: %hu\n", ntohs(((struct sockaddr_in *)(cp->ai_addr))->sin_port));
    }
}

/*receives message on udp socket*/
void receive_message(int *sockfd, char* buf, int* numbytes){
    struct sockaddr_in their_addr;
    socklen_t addr_len;
    char s[INET_ADDRSTRLEN];
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
        printf("listener: got packet from %s:%hu\n", inet_ntop(their_addr.sin_family, &their_addr.sin_addr, s, sizeof s), ntohs(their_addr.sin_port));
        printf("listener: packet is %d bytes long\n", *numbytes);
        printf("listener: packet contains \"%s\"\n", buf);
    }
}

/*gets size of file and number of fragments and writes in on packet header*/
void get_file_info(FILE *fp, struct packet* next_packet){
    fseek(fp, 0L, SEEK_END);
    int size = ftell(fp);
    rewind(fp);
    next_packet->total_frag = size/1000;
    if (size%1000>0){
        next_packet->total_frag+=1;
    }
    //printf("Size:%d\n", size);
}   

/*gets up to 1000 bytes of packet and writes in in the data section of packet*/
void get_next_data(FILE *fp, struct packet* next_packet) {
    if (next_packet->frag_no!=next_packet->total_frag){
        int bytes_read = fread(next_packet->filedata,1000,1,fp)*1000;
        //printf("Bytes read:%d\n", bytes_read);
        next_packet->size = bytes_read;
    } else {
        int start_pos = ftell(fp);
        fread(next_packet->filedata,1000,1,fp);
        int bytes_read = ftell(fp)-start_pos;
        //printf("Bytes read:%d\n", bytes_read);
        next_packet->size = bytes_read;
    }
}

/*converts the packet to a c-string for sending*/
int packet_to_string(char* packet_string, struct packet* next_packet){
    sprintf(packet_string, "%d:%d:%d:%s:", next_packet->total_frag,next_packet->frag_no,next_packet->size,next_packet->filename);
    int str_size = strlen(packet_string);
    memcpy(packet_string+strlen(packet_string),next_packet->filedata,next_packet->size);
    str_size+=next_packet->size;
    packet_string[str_size]='\0';
    return str_size;
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
    char file_name_short[USER_INPUT_LENGTH]; 
    FILE *fp = open_file(file_name_short);

    int sockfd;
    struct addrinfo hints, *servinfo, *clientinfo, *cp;  
    
    create_socket(server_add,server_port,&sockfd,&hints,&servinfo,&clientinfo,&cp);

    //used for timing roundtrip time
    long timer_start, timer_end;
    struct timeval timecheck;

    gettimeofday(&timecheck, NULL);
    timer_start = (long)timecheck.tv_sec * 1000000 + (long)timecheck.tv_usec ;
    
    char* message = "ftp";
    int numbytes;
    send_message (server_add,&sockfd,message,servinfo,cp,&numbytes, strlen(message));

    char buf[MAXBUFLEN];
    receive_message(&sockfd,buf,&numbytes);
    gettimeofday(&timecheck, NULL);
    timer_end = (long)timecheck.tv_sec * 1000000 + (long)timecheck.tv_usec;

    printf("Roundtrip time: %ldus\n", (timer_end-timer_start));
    
    if (strcmp(buf,"yes")==0){
        printf("A file tranfer can start.\n");
    }

    //creates packet for ftp
    struct packet next_packet;
    next_packet.filename = malloc((strlen(file_name_short)+1)*sizeof(char));
    strcpy(next_packet.filename,file_name_short);
    get_file_info(fp,&next_packet);

    //get fragment, convert to string, and send it
    for (int i=0; i<next_packet.total_frag; i++){
        next_packet.frag_no=i+1;
        get_next_data(fp,&next_packet);
        int str_size = 2000;
        char* packet_string = malloc(str_size);
        packet_string[0]='\0';
        str_size = packet_to_string(packet_string,&next_packet);
        if(verbose){
            printf("Str size:%d, %s, %ld\n", str_size, packet_string, strlen(packet_string));
        }
        fd_set readfds;
        struct timeval wait_time;
        int times_tried = 0;
        
        //loop until successful ACK message
        while (true){
            wait_time.tv_sec = 0;
            wait_time.tv_usec = 1000*1000; //1s
            FD_ZERO(&readfds);
            FD_SET(sockfd, &readfds);
            send_message (server_add,&sockfd,packet_string,servinfo,cp,&numbytes, str_size);
            times_tried++;
            if (numbytes!=str_size){
                fprintf(stderr, "Sent bytes different from str length\n");
                return (1);
            }
            select(sockfd+1, &readfds, NULL, NULL, &wait_time);
            if (!FD_ISSET(sockfd, &readfds)){
                fprintf(stderr,"Failed to send packet!\n");
                if (times_tried>MAX_TIMES){
                    printf("Sending failure.\n");
                    return 1;
                }
                continue;
            }
            
            receive_message(&sockfd,buf,&numbytes);
            if (strcmp(buf,"ACK")==0){
                if (verbose){
                    printf("ACK received\n");
                }
                break;
            }
        }
        free(packet_string);
    }
    if (verbose){
        printf("File transfer complete\n");
    }
    fclose(fp);
    free(next_packet.filename);
    freeaddrinfo(servinfo);
    freeaddrinfo(clientinfo);
    close(sockfd);
    return 0;
}