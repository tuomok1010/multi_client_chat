# multi_client_chat(Linux)

<b> A simple chat server that supports multiple connected clients. </b>
<b> Use telnet to connect. For example: 'telnet 127.0.0.1 9114' </b>

<b> This project is heavily based on a guide made by Brian “Beej Jorgensen” Hall. </b>

<b> Main changes that I made: </b>
- Abstracted the entire code into classes.
- Changed the way sockets are added into the pollfd array
- User input


Link to the original below:
https://beej.us/guide/bgnet/examples/pollserver.c
