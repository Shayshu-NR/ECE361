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
int create_socket(char* server_addr, char* server_port);
int send_message(int client_sockfd, struct message message);
int receive_message(int* client_sockfd, char* client_id, char* session_inv);
int decode_message(char* buf, char* client_id, char* session_inv, int* client_sockfd);
enum client_command get_command (char* client_keyword);

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

/*Sends message to server, returns number of bytes sent, -1 otherwise*/
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
        perror("talker: sendto");
        exit(1);
    }
    if (verbose){
        printf("Message sent: %s\n", buf);
    }
    return numbytes;
}

/*Receives message from server, returns number of bytes sent*/
int receive_message(int* client_sockfd, char* client_id, char* session_inv){
    char buf [BUFLEN];
    int numbytes;
    if ((numbytes = recv(*client_sockfd, buf, BUFLEN, 0)) == -1) {
        perror("talker: recv");
        exit(1);
    }
    if (numbytes==0){
        /*Server timed out*/
        if (verbose){
            printf("Server timed out.\n");
        }
        close(*client_sockfd);
        exit(-1);
    }
    if (verbose){
        printf("Message received: %s\n", buf);
    }
    decode_message(buf, client_id, session_inv, client_sockfd);
    return numbytes;
}

/*Decodes message that server sent*/
int decode_message(char* buf, char* client_id, char* session_inv, int* client_sockfd){
    struct message message;
    int rv = sscanf(buf, "%u%u%s%*c%[^\n]", &message.type, &message.size, message.source,
        message.data);
    if (rv<3){
        printf("Error decoding message!\n");
        return -1;
    }
    char* session_id;
    char* text_message;
    switch (message.type){
        case LO_ACK : 
            printf("Successfully logged in.\n");
            strncpy(client_id, (char*)message.source, MAX_ID);
            break;
        case LO_NAK:
            if (rv<4){
                printf("Error decoding message!\n");
                return -1;
            }
            printf("Failed to log in: %s\n", message.data);
            break;
        case JN_ACK:
            if (rv<4){
                printf("Error decoding message!\n");
                return -1;
            }
            printf("Sucessfully joined session: %s\n", message.data);
            break;
        case JN_NAK:
            if (rv<4){
                printf("Error decoding message!\n");
                return -1;
            }
            printf("Failed to joined session: %s\n", message.data);
            break;
        case NS_ACK:
            if (rv<4){
                printf("Error decoding message!\n");
                return -1;
            }
            printf("Created new session: %s\n", message.data);
            break;
        case MESSAGE:
            if (rv<4){
                printf("Error decoding message!\n");
                return -1;
            }
            session_id = malloc(sizeof(char)*MAX_ID);
            text_message = malloc(sizeof(char)*MAX_DATA);
            rv = sscanf((char*)message.data, "%s%*c%[^\n]", session_id, text_message);
            if (rv<2){
                printf("Error decoding message!\n");
                return -1;
            }
            printf("In Session: %s. Message from %s: %s\n", session_id, message.source, text_message);
            free(session_id);
            free(text_message);
            break;
        case QU_ACK:
            printf("%s\n",message.data);
            break;
        case INV_RECV:
            session_id = malloc(sizeof(char)*MAX_ID);
            text_message = malloc(sizeof(char)*MAX_DATA);
            rv = sscanf((char*)message.data,"%s%s", text_message, session_id);
            if (rv<2){
                printf("Error decoding message!\n");
                return -1;
            }
            strncpy(session_inv, session_id, MAX_ID);
            printf("Invite from %s: join server '%s'. Type /accept to accept.\n", message.source, session_id);
            free(session_id);
            free(text_message);
            break;
        case INV_ACK:
            printf("Invite sent succesfully.\n");
            break;
        case INV_NAK:
            printf("Invite failed: %s\n", message.data);
            break;
        case TIMEOUT:
            printf("Client timeout from server. Please login again to continue.\n");
            *client_sockfd = 0;
            break;
        default:
            printf("Error decoding message!\n");
            return -1;
    }
    return 0;
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
    else if (strcmp(client_keyword, "/inv")==0){
        client_command = INV;
    }
    else if (strcmp(client_keyword, "/accept")==0){
        client_command = ACCEPT;
    }
    else if (strcmp(client_keyword, "/reject")==0){
        client_command = REJECT;
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
    int sockfd=0;
    char client_id [MAX_ID]={0};
    char session_inv [MAX_ID] = {0};

    while (true){
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN, &readfds);
        if (sockfd){
            FD_SET(sockfd, &readfds);
            select(sockfd+1, &readfds, NULL, NULL, NULL);
        } else {
            select(STDIN+1, &readfds, NULL, NULL, NULL);
        }
        /*select returns*/
        if (sockfd){
            if (FD_ISSET(sockfd, &readfds)){
                receive_message(&sockfd, client_id, session_inv);
            }
        }
        if (FD_ISSET(STDIN, &readfds)){
            char client_keyword [MAX_CLIENT_INPUT];
            if (scanf("%s", client_keyword)==EOF){
                printf("Error getting input\n");
                return -1;
            }

            enum client_command client_command = get_command(client_keyword);

            char client_input [MAX_CLIENT_INPUT]={0};
            char client_input2 [MAX_CLIENT_INPUT]={0};
            struct message message={0};
            char empty_string[] = "";
            switch (client_command) {
                case LOGIN : ;
                    char password [MAX_PASSWORD];
                    char server_ip [INET_ADDRSTRLEN];
                    char port [MAX_CLIENT_INPUT];
                    if (scanf("%s %s %s %s", client_input, password, server_ip, port)==EOF){
                        printf("Error getting input\n");
                        return -1;
                    }
                    if (!sockfd){
                        sockfd = create_socket(server_ip,port);
                    }
                    message.type = LOGON;
                    message.size = strlen(password);
                    strncpy((char*)message.source, client_input, MAX_NAME);
                    strncpy((char*)message.data, password, MAX_DATA);
                    send_message(sockfd, message);
                    break;
                case LOGOUT :
                    if (!sockfd){
                        printf("Error: No connection established\n");
                        continue;
                    }
                    message.type = EXIT;
                    message.size = 0;
                    strncpy((char*)message.source, client_id, MAX_NAME);
                    strncpy((char*)message.data, empty_string, MAX_DATA);
                    send_message(sockfd, message);
                    close (sockfd);
                    sockfd = 0;
                    break;
                case JOINSESSION :
                    if (!sockfd){
                        printf("Error: No connection established\n");
                        continue;
                    }
                    if (scanf("%s", client_input)==EOF){
                        printf("Error getting input\n");
                        return -1;
                    }
                    message.type = JOIN;
                    message.size = strlen(client_input);
                    strncpy((char*)message.source, client_id, MAX_NAME);
                    strncpy((char*)message.data, client_input, MAX_DATA);
                    send_message(sockfd, message);         
                    break;
                case LEAVESESSION :
                    if (!sockfd){
                        printf("Error: No connection established\n");
                        continue;
                    }
                    if (scanf("%s", client_input)==EOF){
                        printf("Error getting input\n");
                        return -1;
                    }
                    message.type = LEAVE_SESS;
                    message.size = strlen(client_input);
                    strncpy((char*)message.source, client_id, MAX_NAME);
                    strncpy((char*)message.data, client_input, MAX_DATA);
                    send_message(sockfd, message);
                    break;
                case CREATSESSION :
                    if (!sockfd){
                        printf("Error: No connection established\n");
                        continue;
                    }
                    if (scanf("%s", client_input)==EOF){
                        printf("Error getting input\n");
                        return -1;
                    }
                    message.type = NEW_SESS;
                    message.size = strlen(client_input);
                    strncpy((char*)message.source, client_id, MAX_NAME);
                    strncpy((char*)message.data, client_input, MAX_DATA);
                    send_message(sockfd, message);  
                    message.type = JOIN;
                    send_message(sockfd, message);
                    break;
                case LIST :
                    if (!sockfd){
                        printf("Error: No connection established\n");
                        continue;
                    }
                    message.type = QUERY;
                    message.size = 0;
                    strncpy((char*)message.source, client_id, MAX_NAME);
                    strncpy((char*)message.data, empty_string, MAX_DATA);
                    send_message(sockfd, message);
                    break;
                case QUIT :
                    if (!sockfd){
                        return 0;
                    }
                    close(sockfd);
                    return 0;
                case TEXT :
                    if (!sockfd){
                        printf("Error: No connection established\n");
                        continue;
                    }
                    if (scanf("%*c%[^\n]", client_input)==EOF){
                        printf("Error getting input\n");
                        return -1;
                    }
                    //client keyword is session ID
                    strcat(client_input2, client_keyword);
                    strcat(client_input2, " ");
                    strcat(client_input2, client_input);
                    message.type = MESSAGE;
                    message.size = strlen(client_input2);
                    strncpy((char*)message.source, client_id, MAX_NAME);
                    strncpy((char*)message.data, client_input2, MAX_DATA);
                    send_message(sockfd, message);
                    break;
                case INV:
                    if (scanf("%s%s", client_input, client_input2)==EOF){
                        printf("Error getting input\n");
                        return -1;
                    }
                    message.type = INV_SEND;
                    strcat(client_input, " ");
                    strcat(client_input, client_input2);
                    message.size = strlen(client_input);
                    strncpy((char*)message.source, client_id, MAX_NAME);
                    strncpy((char*)message.data, client_input, MAX_DATA);
                    send_message(sockfd, message);
                    break;
                case ACCEPT:
                    if (strncmp(session_inv, "", MAX_ID)!=0){
                        message.type = JOIN;
                        message.size = strlen(session_inv);
                        strncpy((char*)message.source, client_id, MAX_NAME);
                        strncpy((char*)message.data, session_inv, MAX_DATA);
                        send_message(sockfd, message);   
                    } else {
                        printf("No valid session invite.\n");
                    }
                    break;
                case REJECT:
                    printf("Invite rejected.\n");
                    strcpy(session_inv,"");
                    break;
                default :
                    break;
            } //Switch case
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF);
        } //FDISSET(STDIN)
    } //While LOOP
    return 0;
}