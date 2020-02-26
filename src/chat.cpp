// standard C stuff
#include <stdio.h>
#include <unistd.h>
#include <string.h>

// C++ stuff
#include <string>
#include <iostream>

// socket stuff
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>

std::string GetPortNum()
{
    std::string input{"9114"};
    do
    {
        std::cout << "Enter the port number (default: 9114) >" << std::flush;
        std::cin >> input;

        if(std::stoi(input) > 65535)
        {
            std::cout << "Error! Port number too large. Maximum is 65535. Press enter to continue." << std::flush;
            std::cin.ignore();
            std::cin.get();
        }
        else if(std::stoi(input) <= 1024)
        {
            std::cout << "Error! Port number should be larger than 1024. Press enter to continue." << std::flush;
            std::cin.ignore();
            std::cin.get();
        }

        system("clear");

    } while (std::stoi(input) > 65535 || std::stoi(input) <= 1024);
   
    system("clear");
    return input;
}

int GetNumMaxConnections()
{
    int input{};
    do
    {
        std::cout << "Enter the max number of clients >" << std::flush;
        std::cin >> input;
        system("clear");

    } while (input > SOMAXCONN || input <= 0);
   
    system("clear");
    return input;
}

void HandleNewConnection(struct pollfd* pollList, int size)
{
    // connected client's information:
    int connectionSocket{};                 // accept() returns a new socket that is then used between the server and client to communicate.
    struct sockaddr_storage clientAddr;     // connector's address information
    socklen_t clientAddrSize;               // client address size
    char clientAddrStr[INET6_ADDRSTRLEN];   // client IP address in a string format

    if(pollList[0].fd != -1 && pollList[0].revents & POLLIN)
    {
        clientAddrSize = sizeof(clientAddr);
        connectionSocket = accept(pollList[0].fd, (struct sockaddr*)&clientAddr, &clientAddrSize);
        if(connectionSocket == -1)
        {
            perror("ERROR_SERVER: accept");
        }
        else
        {
            // find the first inactive pollfd and use that for the new connection
            for(int x = 0; x < size; ++x)
            {
                if(pollList[x].fd == -1)
                {
                    pollList[x].fd = connectionSocket;
                    pollList[x].events = POLLIN;
                    break;
                }
            }

            if(((struct sockaddr*)&clientAddr)->sa_family == AF_INET)
                std::cout 
                    << inet_ntop(clientAddr.ss_family, &((struct sockaddr_in*)&clientAddr)->sin_addr, clientAddrStr, INET6_ADDRSTRLEN) 
                    << " connected on socket " 
                    << connectionSocket << "." << std::endl;
            else if(((struct sockaddr*)&clientAddr)->sa_family == AF_INET6)
                std::cout 
                    << inet_ntop(clientAddr.ss_family, &((struct sockaddr_in6*)&clientAddr)->sin6_addr, clientAddrStr, INET6_ADDRSTRLEN) 
                    << " connected on socket " 
                    << connectionSocket << "." << std::endl;
        }
    }
}

int main()
{
    std::string port = GetPortNum();
    int maxConnections = GetNumMaxConnections();

    // server(host) logic
    // ================================
    int listeningSocket{};              // main listening socket file descriptor
    struct addrinfo hints;              // used to setup the server data with the getaddrinfo() function
    struct addrinfo* serverInfo;        // holds the server data
    struct addrinfo* serverAddr;        // used to iterate through the results in server_info that we get from getaddrinfo()

    memset(&hints, 0, sizeof(hints));   // makes sure the hints struct is empty
    hints.ai_family = AF_UNSPEC;        // chooses automatically IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;    // use TCP stream socket
    hints.ai_flags = AI_PASSIVE;        // use my IP


    // getaddrinfo is used to set up the server_info with the specification provided in the hints struct:
    // 1st parameter: hostname / IP(we can leave it null because we used the AI_PASSIVE option in the hints struct)
    // 2nd parameter: service/port
    // 3rd parameter: hints struct containing the specification we want to use
    // 4th parameter: the struct where the results are stored
    // It gives us a list of possible addresses we can use and stores them in the serverinfo.
    int result = getaddrinfo(nullptr, port.c_str(), &hints, &serverInfo);
    if(result != 0)
    {
        std::cerr << "ERROR_SERVER: getaddrinfo: " << gai_strerror(result) << std::endl;
        return 1;
    }

    // getaddrinfo returns multiple results(for example some results may have different ai_family or protocol), this depends
    // how strictly we set up our hints struct. Here we loop through all of the results and use the first one that works.
    // The ai_next field in the struct addrinfo points to the next result in the list.

    int sockOptVal{1};  // used in the next loop to set a socket option value
    for(serverAddr = serverInfo; serverAddr != nullptr; serverAddr = serverAddr->ai_next)
    {
        // create the main listening socket
        listeningSocket = socket(serverAddr->ai_family, serverAddr->ai_socktype, serverAddr->ai_protocol);
        if(listeningSocket == -1)
        {
            // if we get an error, we ignore the rest of the loop(continue) and try again with a different address
            perror("ERROR_SERVER: socket");
            continue;
        }

        // this option allows reusing ports. It prevents the "Address already in use" error
        int sockOptStatus = setsockopt(listeningSocket, SOL_SOCKET, SO_REUSEADDR, &sockOptVal, sizeof(int));
        if(sockOptStatus == -1)
        {
            perror("ERROR_SERVER: setsockopt");
            exit(1);
        }

        // the bind function associates our socket with a port number. This is NOT needed when we are joining the chat. Only when hosting.
        int bindStatus = bind(listeningSocket, serverAddr->ai_addr, serverAddr->ai_addrlen);
        if(bindStatus == -1)
        {
            close(listeningSocket);
            perror("ERROR_SERVER: bind");
            continue;
        }

        // if none of the functions above return an error, we have either found an address that works or
        // we've looped through the entire list and found none that works. Either way we can break out of the loop.
        break;
    }

    // this is no longer needed
    freeaddrinfo(serverInfo);

    // if we failed to bind any address in the previous loop
    if(serverAddr == nullptr)
    {
        std::cerr << "ERROR_SERVER: failed to bind" << std::endl;
        exit(1);
    }

    // listen on any incoming clients trying to connect to the server port.
    int listen_status = listen(listeningSocket, maxConnections);
    if(listen_status == -1)
    {
        perror("ERROR_SERVER: listen");
        exit(1);
    }

    std::cout << "waiting for connections..." << std::endl;


    char buffer[1024];

    // pollList is used in the poll() function to check whenever a socket is ready to receive/send data
    // POLLIN means we are checking when data is ready to recv() in this socket
    struct pollfd pollList[maxConnections];
    pollList[0].fd = listeningSocket;
    pollList[0].events = POLLIN;

    // we need to initialize all the empty pollfds to -1 so that they get ignored by the poll() function
    for(int i = 1; i < maxConnections; ++i)
        pollList[i].fd = -1;

    while(true)
    {
        std::cout << "Poll list: ";
        for(auto& sock : pollList)
        {
            std::cout << sock.fd << ", ";
        }
        std::cout << std::endl;

        // check if any events have occurred
        int pollsOccurred = poll(pollList, maxConnections, -1);

        if(pollsOccurred == -1)
        {
            perror("ERROR_SERVER: poll");
            exit(1);
        }

        HandleNewConnection(pollList, maxConnections);

        // loop through all current connected client sockets
        for(int i = 1; i < maxConnections; ++i)
        {
            // if we are just a regular client:
            if(pollList[i].fd != -1 && pollList[i].revents & POLLIN)
            {
                int numBytes = recv(pollList[i].fd, buffer, sizeof(buffer), 0);
                int senderFd = pollList[i].fd;

                if(numBytes <= 0)
                {
                    // got an error or connection closed by client
                    if(numBytes == 0)
                        std::cout << "SERVER: socket " << senderFd << " hung up" << std::endl;
                    else
                        perror("ERROR_SERVER: recv");

                    // if client disconnected, we want to close the socket and inactivate the fd in the pollList
                    close(pollList[i].fd);
                    pollList[i].fd = -1;       
                }
                else
                {
                    // we got some good data from a client. we start looping at index 1 because we want to skip the listeningSocket(index 0)
                    for(int j = 1; j < maxConnections; ++j)
                    {
                        // send the data to everyone except the listener and the client that is sending and none of the inactive sockets
                        int destinationFd = pollList[j].fd;
                        if(destinationFd != senderFd && destinationFd != -1)
                        {
                            int send_status = send(destinationFd, buffer, numBytes, 0);
                            if(send_status == -1)
                                perror("ERROR_SERVER: send");
                        }
                    }
                }                   
            }               
        }
    }
    return 0;
}