#ifndef EPOLLER_H
#define EPOLLER_H

#include <vector>

#include <sys/epoll.h>

class Epoller {
public:
    explicit Epoller(int max_event = 1024);

    ~Epoller();

    bool addFd(int fd, uint32_t events);

    bool movFd(int fd, uint32_t events);

    bool delFd(int fd);

    int wait(int timeout_ms = -1);

    int getEventFd(size_t i) const;

    uint32_t getEvents(size_t i) const;

private:
    int _epoll_fd;

    std::vector<struct epoll_event> _events;
};

#endif // EPOLLER_H