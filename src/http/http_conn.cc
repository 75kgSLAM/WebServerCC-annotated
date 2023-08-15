#include "http_conn.h"

#include "unistd.h"
#include <sys/uio.h>

const int MAX_EXTRA_SPACE = 65536;

HttpConn::HttpConn(): _fd(-1), _addr({0}), _is_closed(true), _iov_count(0) {}

HttpConn::~HttpConn() {
    close_conn();
}

void HttpConn::init(int socket_fd, const sockaddr_in& addr) {
    assert(socket_fd > 0); // why fd > 0 ？
    user_count++;
    _addr = addr;
    _fd = socket_fd;
    _read_buf.retrieveAll();
    _write_buf.retrieveAll();
    _is_closed = false;
    _version = "1.1";
    LOG_INFO("New client connection [%d](%s:%d), current user count: %d",
                _fd, getIP(), getPort(), user_count)
}

void HttpConn::close_conn() {
    // 如果没有init过，不需要释放？
    if (_is_closed == false) {
        _is_closed = true;
        user_count--;
        close(_fd);
        LOG_INFO("A client quit [%d](%s:%d), current user count: %d",
                    _fd, getIP(), getPort(), user_count)
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
    } while (is_ET);
    return len;
}

ssize_t HttpConn::write(int* save_errno) {
    ssize_t len = -1;
    do {
        len = writev(_fd, _iov, _iov_count);
        if (len <= 0) {
            *save_errno = errno;
            break;
        }
        // writev成功后会将iov_len置为0吗？？？ TODO
        if (_iov[0].iov_len == 0 and _iov[1].iov_len == 0) {
            break;
        }
        size_t l = static_cast<size_t>(len);
        if (l > _iov[0].iov_len) {
            _iov[1].iov_base = static_cast<char*>(_iov[1].iov_base) + (l - _iov[0].iov_len);
            _iov[1].iov_len -= (l - _iov[0].iov_len);
            // 只有iov[0]是用_write_buf中提前生成的响应
            // iov[1]对应的是请求的文件，并不是我们所维护的内容
            _write_buf.retrieveAll();
            _iov[0].iov_len = 0;
        } else {
            _iov[0].iov_base = static_cast<char*>(_iov[0].iov_base) + len;
            _iov[0].iov_len -= l;
            _write_buf.retrieve(l);
        }
    } while (is_ET or getBytesToWrite() > 10240); // why 10240?
    return len;
}

bool HttpConn::process() {
    _request.init();
    if (_read_buf.getReadableBytes() <= 0) {
        return false;
    }
    bool is_keep_alive = false;
    int code = 400;
    if (_request.parse(_read_buf) == true) {
        LOG_DEBUG("Path: %s", _request.getPath().c_str())
        is_keep_alive = _request.isKeepAlive();
        code = 200;
    }
    _response.init(src_dir, _request.getPath(), is_keep_alive, code);
    _response.makeResponse(_write_buf);
    
    // 状态栏和响应头
    _iov[0].iov_base = const_cast<char*>(_write_buf.getReadPos());
    _iov[0].iov_len = _write_buf.getReadableBytes();
    _iov_count += 1;

    // 请求的文件
    if (_response.getFileLen() > 0 and _response.getFile() != nullptr) {
        _iov[1].iov_base = static_cast<char*>(_response.getFile());
        _iov[1].iov_len = static_cast<size_t>(_response.getFileLen());
        _iov_count += 1;
    }
    LOG_DEBUG("Process done!")
    return true;
}

int HttpConn::getBytesToWrite() const {
    return _iov[0].iov_len + _iov[1].iov_len;
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

bool HttpConn::isKeepAlive() const {
    return _request.isKeepAlive();
}

// private methods
ssize_t HttpConn::_readToBuf(int fd, int* err_state) {
    // why 65536?
    char buf[MAX_EXTRA_SPACE];
    // length should <= __IOV_MAX
    iovec iov[2];
    size_t writable = _read_buf.getReadableBytes();
    // 使用readv分散读，保证数据全部读完
    // 默认总数据量不会超过buf容量+额外容量
    iov[0].iov_base = _read_buf.getWritePos();
    iov[0].iov_len = writable;
    iov[1].iov_base = buf;
    iov[1].iov_len = sizeof buf;

    ssize_t len = readv(_fd, iov, 2);
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