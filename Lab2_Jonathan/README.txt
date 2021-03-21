This folder contains code that starts a SERVER and a CLIENT that communicates to the server.
The purpose of the server is to allow clients to log in, start/join a session, and communicate with 
other users in the sessions via text. 

Common data structures: both server and client utilize a common structure "struct message". Struct message
is a standardized message structure that includes a message header which is a control field containing a value
in "enum control type", which defines the type of message it is. It also contains the sender in *source* and the 
ASCII data in the *data* field. 

SERVER: The server is responsible for storing the active sessions in struct session* sessions, and the users in each of the sessions.
It also stores users in the struct client* clients, which stores all active users and their user information, and which sessions they are in.
Sessions and client data structures are inherently somewhat redundant as sessions stores clients of each session and 
clients store the session their clients are in.

SERVER starts off by initializing the global data structure logins with the userid and password of every user in the database, stored in plaintext,
by parsing the file password.txt in the same directory as the program. Next, it starts a listening socket to wait for TCP connections.
The server then starts an infinite loop where it goes in a blocking mode using select(), until one of its socket receives information, that is, either
a new user is trying to connect, or an existing user sends a message. Since there are no existing users in the beginning, it waits for the first user to connect.
Every time a user tries to connect, the server tries to accept the connection and create a new socket for that particular user, it then stores the client(user) 
information in an empty client space, if it exists. 
Next, the client can try to log on in the server (it must if it wants to do anything), by sending a logon message. Every message that the client sends,
it is read by receive_message, which sends it to decode_message which decodes the message and calls any auxilary functions in a switch case to deal with that message type.
In this case, it will call check_passwords with the decoded client and data fields, and check password will return if the password was correct. For certain messages,
the server will then create a message response, and then call send_message. Send message will then flatten the message into a byte stream to be sent to the
destination client.  

Other messages that the server response to include:
1. NEW_SESS/JOIN_SESS/LEAVE_SESS: Server calls add_session/join_session/leave_session. This deals with the data structures of sessions and clients by
iterating through them to get the correct session id and client id and changing the fields, such as adding a new client or removing a client. When the last client
is removed from a session, the session is deleted. Deletion of session is indicated by a session id of 0, as a normal session
will never have an id of 0.

2. EXIT: Client leaves server, server calls leave session on all their existing sessions, and their client information is erased.  

3. MESSAGE: Client send a message to the server. The server calls broadcast_msg, which broadcasts the message to all clients of the session, with the exception of the client
who sent the message. It does this by creating a new message, and calls send_message to the different client sockets.

4. QUERY: Calls query_sessions, which creates a data_string containing all the clients, sessions, and the clients in those sections. Sends a response message to the client
with the data_string.

CLIENT: The client program runs a while loop in main, and runs the blocking function select(), until a socket receives information, that being either the socket to the server,
or the standard input stdin from the terminal. Since it starts off without a socket to the client, the first message the user must type into the terminal is the /login command, followed
by the login credential and the server information. When select returns (because stdin has input), the main function scans the first word of the input, and calls get_command to check if
the first word is a valid command. If it is (i.e. /login), then it returns a enum client_command to the main function, which deals with the various client commands using a switch case. The 
case for LOGIN is calling the create_socket function if a socket hasn't been created already to establish a connection with the server. After the connection is established, it creates a message
with the login credential and calls send_message, which is identical to the server's send_message function, and flattens the message struct in into a byte stream and sents it over TCP. 

The client program also awaits for server messages by listening to the tcp socket. When a server message is received, the client runs decode message, which decodes the message in a similar manner 
to the server decode message and checks for the control type and running it through the switch case. The message can either be a response from the server about a recent client command or a text message
from another user from the a session that the client is in. Either way, the decode_message function parses the data field and prints a formatted response to the terminal for the user to see. 

Other commands that the client can send to the server include:
/join, /leave, /createsession: This creates the message that includes the session id that the user inputted to send the command to the server.
/list: Sends the query command to the server.
/logout: Sends the logout command to the server, but does not close the socket (i.e. you can log back in as the same client or a different client).
/quit: Closes the socket, but does not tell the server it is logging out. This is not a problem because the server will detect the connection is lost and 
automatically logout the client anyways, but its better to logout first and then quit.
TEXT: Anything after TEXT will be sent to the server as ASCII data which is then relayed to another on the session that the user is in. If the user is not in any session,
or by themselves, nothing will happen.

NOTE: all the above commands, with the exception of /quit and /login, requires a active connection with the server, or an error message will print 
and nothing will be done. Furthermore, with the added exception of /logout, all the commands required a accepted login. It will not produce an error message,
but the server will do nothing.  
