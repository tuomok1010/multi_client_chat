#include "server.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <string>
#include <iostream>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

Server::Server(std::string port, int maxConnections)
    : 
    fd(-1), 
    ipAddress(""), 
    port(port), 
    maxConnections(maxConnections), 
    pollList(maxConnections)
{

}

Server::~Server()
{

}

int Server::CreateSocket()
{
    // server info
    // ================================
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
        return -1;
    }

    // getaddrinfo returns multiple results(for example some results may have different ai_family or protocol), this depends
    // how strictly we set up our hints struct. Here we loop through all of the results and use the first one that works.
    // The ai_next field in the struct addrinfo points to the next result in the list.

    int sockOptVal{1};  // used in the next loop to set a socket option value
    for(serverAddr = serverInfo; serverAddr != nullptr; serverAddr = serverAddr->ai_next)
    {
        // create the main listening socket
        fd = socket(serverAddr->ai_family, serverAddr->ai_socktype, serverAddr->ai_protocol);
        if(fd == -1)
        {
            // if we get an error, we ignore the rest of the loop(continue) and try again with a different address
            perror("ERROR_SERVER: socket");
            continue;
        }

        // this option allows reusing ports. It prevents the "Address already in use" error
        int sockOptStatus = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &sockOptVal, sizeof(int));
        if(sockOptStatus == -1)
        {
            perror("ERROR_SERVER: setsockopt");
            exit(1);
        }

        // the bind function associates our socket with a port number.
        int bindStatus = bind(fd, serverAddr->ai_addr, serverAddr->ai_addrlen);
        if(bindStatus == -1)
        {
            close(fd);
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

    pollList.AddServerSocket(fd);

    return fd;
}

void Server::Listen()
{
    // listen on any incoming clients trying to connect to the server port.
    int listen_status = listen(fd, maxConnections);
    if(listen_status == -1)
    {
        perror("ERROR_SERVER: listen");
        exit(1);
    }
}

void Server::Poll()
{
    // checks if any events have occured on any of the sockets in the pollList
    pollList.Poll();
}

void Server::Accept()
{
    const auto& fds = pollList.Getfds();

    if(fds[0].revents & POLLIN)
    {
        // connected client's information:
        int connectionSocket{};                 // accept() returns a new socket that is then used between the server and client to communicate.
        struct sockaddr_storage clientAddr;     // connector's address information
        socklen_t clientAddrSize;               // client address size
        char clientAddrStr[INET6_ADDRSTRLEN];   // client IP address in a string format

        clientAddrSize = sizeof(clientAddr);
        connectionSocket = accept(fd, (struct sockaddr*)&clientAddr, &clientAddrSize);

        if(connectionSocket == -1)
        {
            perror("ERROR_SERVER: accept");
        }
        else
        {
            // add client to PollList
            pollList.AddClientSocket(connectionSocket);

            // this horrendous monster just prints the ip of the connected client on the terminal. Prints either IPv4 or IPv6 depending on which one is used
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

void Server::RelayMessagesToClients()
{
    const auto& fds = pollList.Getfds();

    char buffer[1024];

    // note that we start from index 1 because we want to skip the listening socket(which is at index 0)
    for(int i = 1; i < maxConnections; ++i)
    {
        // make sure the socket is not inactive(-1) and check if an event has occurred
        if(fds[i].fd != -1 && fds[i].revents & POLLIN)
        {
            int numBytes = recv(fds[i].fd, buffer, sizeof(buffer), 0);
            int senderFd = fds[i].fd;

            if(numBytes <= 0)
            {
                // got an error or connection closed by client
                if(numBytes == 0)
                    std::cout << "SERVER: socket " << senderFd << " disconnected." << std::endl;
                else
                    perror("ERROR_SERVER: recv");

                // if client disconnected, we want to close the socket and inactivate the fd in the pollList
                close(fds[i].fd);
                fds[i].fd = -1;       
            }
            else
            {
                // we got some good data from a client. we start looping at index 1 because we want to skip the listeningSocket(index 0)
                for(int j = 0; j < maxConnections; ++j)
                {
                    // send the data to everyone except the client that is sending, listener,  and any of the inactive sockets
                    int destinationFd = fds[j].fd;
                    if(destinationFd != senderFd && destinationFd != -1 && destinationFd != fd)
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

void Server::PrintPollList()
{
    pollList.Print();
}