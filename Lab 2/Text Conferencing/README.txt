Data Structures:
    
    For this lab several data structures were used to keep track of sessions, users, etc....
    
    Starting on the client side, a structure called message was used to make handling the incoming
    and outpoing packets a bit easier. It contains the type of message being sent, the size of the data
    being sent, who the packet is coming from, and the actual data itself.
    
    struct message {
        unsigned int type;
        unsigned int size;
        unsigned char source[MAX_NAME];
        unsigned char data[MAX_MSG];
    };

    On the server side there was a message packet, the same as the client, a session structure and a user 
    structure. The session structure was used to keep track of the name of a created session, whethere it was 
    active or not, and which users were in that given session.

    struct session {
        char name[MAX_SESSION];
        int users[MAX_USERS];
        int session_id;
        int active;
    };

    The user structure contained the name of the user, their ip address, a pointer to their current session, 
    their port number, their socket number, whether they were active or not, and a session id.

    struct user{
        char name[MAX_NAME];
        char ip_addr[INET_ADDRSTRLEN];
        struct session * chatRoom;
        int port;
        int sockfd; 
        int active;
        int session_id;
    };

Server side:

    The core of this program is quite straight forward. Start by initializing all the necessarry data structures with the data
    from the data base and clearing all the client information. Create the server by binding to a socket given a port number
    then listening to incoming connections. 
    
    /login: Once a connection is made receive the incoming data and parse it to extract the 
    username and password of the user. Next verify the incoming information with the data saved in the server user database 
    (store in a csv file in this case). Once verified send back an acknowledgement. This was done by comparing the message source
    filed and the message data fields to the list of verified usernames and passwords, and sending back an acknowledgement to the 
    client socket. Once a client is connected their socket is added to the read set, and their status in their user structure is updated
    from not active to active, as well as their ip address and socket being filled in.

    When the server receives a message and it isn't a new connection then it parses the packet and executes one of the following
    commands...

    /createsession: Finds an empty session in the client_sessions array and adds the user to the list of active users, then 
    sends an acknowledgement to the user. Update the pointer in the creators user structure to point to the newly created 
    session. Set the user to active.

    /joinsession: Finds the requested session by name from the client_sessions array and adds the user to the list of active users 
    if there is enough room. If there is enough room set their user structure to point to the newly joined session and send an 
    acknowledgement, otherwise send a join error.

    /leavesession: Find the users current session and remove them from the list of active users in that session. If the session has no one
    else in it then close the session by calling closeSession(room). 

    /list: Itterate through the list of sessions and check if any are active, Itterate through a list of users. Send to the client
    a list of active session names and active users. 

    /text: Find the session that the sender is in and go through the list of active users in that session. Using their user.sockfd 
    as their socket forward the message to each user. 

Client side:

    The client will continually check to see if it was received a message from the server or if stdin has been written to. 
    If stdin was set, then parse the input and excute the correct command. All commands follow the same general frame work. Read 
    the data from the terminal, create a segment using createSegment(buffer, command) send the segment to the server, wait for a response, 
    parse the response using parseBuffer(buffer, message_structure), then relay the information to the client.

    Ex:
    stding <- /list
    createSegment(buffer, QUERY)
    send(socket, buffer, buffer_size, 0)
    recv(socket, recv_buffer, buffer_size)
    parseBuffer(recv_buffer, message_structure)
    fprintf(stderr, "%s\n", message_structure.data)

    To send a text message the client waits for stdin to be written to, and reads one word in using scanf, to read the reaming text, 
    use fgets to read the rest of the terminal. 
    To keep track of the clients sessions, etc... a few global variables were used. This also, allows for client side control meaning that 
    the client doesn't need to send unecessary packets to the server.

    Lastly, a few precautions were taken to prevent the user from disconnection improperly. This means that the user must first leaver their 
    session, then logout before they can quit the program. 


