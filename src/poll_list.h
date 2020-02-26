#ifndef _POLL_LIST_H_
#define _POLL_LIST_H_

#include <poll.h>

class PollList
{
public:
    PollList(int maxConnections);
    ~PollList();

    void Poll();
    void AddServerSocket(int socketfd);
    void AddClientSocket(int socketfd);
    void Print();

    inline struct pollfd* Getfds() const { return fds; }

private:
    int maxConnections;
    struct pollfd* fds;
};

#endif