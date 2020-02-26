#include "poll_list.h"

#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <string>


PollList::PollList(int maxConnections)
    : maxConnections(maxConnections)
{
    fds = new pollfd[maxConnections];

    // we need to initialize all the empty pollfds to -1 so that they get ignored by the poll() function if they arent being used
    for(int i = 0; i < maxConnections; ++i)
        fds[i].fd = -1;
}

PollList::~PollList()
{
    delete[] fds;
}

void PollList::Poll()
{
    int pollsOccurred = poll(fds, maxConnections, -1);
    if(pollsOccurred == -1)
    {
        perror("ERROR_SERVER: poll");
        exit(1);
    }
}

void PollList::AddServerSocket(int socketfd)
{
    fds[0].fd = socketfd;
    fds[0].events = POLLIN;
}

void PollList::AddClientSocket(int socketfd)
{
    // find the first inactive slot in the PollList and use that for the new connection. Starts from index 1 because 0 is reserved for the server
    for(int slot = 1; slot < maxConnections; ++slot)
    {
        if(fds[slot].fd == -1)
        {
            fds[slot].fd = socketfd;
            fds[slot].events = POLLIN;
            return;
        }
    }
}

void PollList::Print()
{
    std::cout 
        << "======\n"
        << "PollList\n"
        << "server socket: [" << fds[0].fd << "] " << "client sockets: ";

    for(int i = 1; i < maxConnections; ++i)
        std::cout << "[" << fds[i].fd << "] ";
    std::cout << "\n======" << std::endl;
}