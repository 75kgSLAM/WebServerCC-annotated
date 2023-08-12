#include "http_conn.h"

#include "unistd.h"
#include <sys/uio.h>

const int MAX_EXTRA_SPACE = 65536;

HttpConn::HttpConn(): _fd(-1), _addr({0}), _is_closed(true) {}

HttpConn::~HttpConn() {
    close_conn();
}

void HttpConn::init(int socket_fd, const sockaddr_in& addr) {
    assert(socket_fd > 0); // why fd > 0 ？
    _user_count++;
    _addr = addr;
    _fd = socket_fd;
    _read_buf.retrieveAll();
    _write_buf.retrieveAll();
    _is_closed = false;
    _version = "1.1";
    LOG_INFO("New client connection [%d](%s:%d), current user count: %d",
                _fd, getIP(), getPort(), _user_count)
}

void HttpConn::close_conn() {
    // 如果没有init过，不需要释放？
    if (_is_closed == false) {
        _is_closed = true;
        _user_count--;
        close(_fd);
        LOG_INFO("A client quit [%d](%s:%d), current user count: %d",
                    _fd, getIP(), getPort(), _user_count)
    }
}

ssize_t HttpConn::read(int* save_errno) {
    // 这里以非正值来表征读取结束
    ssize_t len = -1;
    do {
        len = _readToBuf(_fd, save_errno);
        if (len <= 0) {
            break;
        }
    } while (_is_ET);
    return len;
}

ssize_t HttpConn::write(int* save_errno) {
    ssize_t len = -1;
    do {
        len = writev(_fd, _iov, _iov_count);
    } while (_is_ET);
    return len;
}

bool HttpConn::process() {
    _request.init();
    if (_read_buf.getReadableBytes() <= 0) {
        return false;
    }
    if (_request.parse(_read_buf) == true) {
        LOG_DEBUG("Path: %s", _request.getPath().c_str())

    }
}

int HttpConn::getFd() const {
    return _fd;
}

const char* HttpConn::getIP() const {
    return inet_ntoa(_addr.sin_addr);
}

int HttpConn::getPort() const {
    return _addr.sin_port;
}

std::string HttpConn::getHttpVersion() const {
    return _version;
}

// private methods
ssize_t HttpConn::_readToBuf(int fd, int* err_state) {
    // why 65536?
    char buf[MAX_EXTRA_SPACE];
    // length should <= __IOV_MAX
    iovec iov[2];
    const size_t writable = _read_buf.getReadableBytes();
    // 使用readv分散读，保证数据全部读完
    // 默认总数据量不会超过buf容量+额外容量
    iov[0].iov_base = _read_buf.getWritePos();
    iov[0].iov_len = writable;
    iov[1].iov_base = buf;
    iov[1].iov_len = sizeof buf;

    const ssize_t len = readv(_fd, iov, 2);
    if (len < 0) {
        *err_state = errno;
    } else if ((size_t)len <= writable) {
        _read_buf.hasWritten((size_t)len);
    } else {
        _read_buf.hasWritten(writable);
        _read_buf.append(buf, len - writable);
    }
    return len;
}

ssize_t HttpConn::_writeToBuf() {

}