#ifndef _SERVER_H_
#define _SERVER_H_

#include "poll_list.h"

#include <string>

class Server
{
public:
    Server(std::string port, int maxConnections);
    ~Server();

    int CreateSocket();
    void Listen();
    void Poll();
    void Accept();
    void RelayMessagesToClients();

    void PrintPollList();
    
private:
    int fd;
    std::string ipAddress;
    std::string port;
    int maxConnections;

    PollList pollList;
};

#endif