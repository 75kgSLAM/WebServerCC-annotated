#include "epoller.h"

#include <assert.h>
#include <sys/epoll.h>
#include <unistd.h>

Epoller::Epoller(int max_event = 1024)
    : _epoll_fd(epoll_create1(EPOLL_CLOEXEC)), _events(max_event) {}

Epoller::~Epoller() {
    close(_epoll_fd);
}

bool Epoller::addFd(int fd, uint32_t events) {
    if (fd < 0) {
        return false;
    }
    struct epoll_event st_ev = { 0 };
    st_ev.data.fd = fd;
    st_ev.events = events;
    return 0 == epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &st_ev);
}

bool Epoller::movFd(int fd, uint32_t events) {
    if (fd < 0) {
        return false;
    }
    struct epoll_event st_ev = { 0 };
    st_ev.data.fd = fd;
    st_ev.events = events;
    return 0 == epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &st_ev);
}

bool Epoller::delFd(int fd) {
    if (fd < 0) {
        return false;
    }
    struct epoll_event st_ev = { 0 };
    return 0 == epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, &st_ev);
}

int Epoller::wait(int timeout_ms) {
    // 注意_events是vector，首元素地址为&_events[0]
    return epoll_wait(_epoll_fd, &_events[0], static_cast<int>(_events.size()), timeout_ms);
}

int Epoller::getEventFd(size_t i) const {
    assert(i >= 0 and i < _events.size());
    return _events[i].data.fd;
}

uint32_t Epoller::getEvents(size_t i) const {
    assert(i >= 0 and i < _events.size());
    return _events[i].events;
}