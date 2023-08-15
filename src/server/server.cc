#include "server/server.h"
#include "http/http_conn.h"
#include "pool/sql_conn_pool.h"

#include <unistd.h>
#include <assert.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <string.h>

Server::Server(
    int port, int trigger_mode, int timeout_ms, bool opt_linger,
    int sql_port, const char* sql_username, const char* sql_password, const char* sql_dbname)
    : _port(port), _timeout_ms(timeout_ms), _opt_linger(opt_linger), _is_close(false), _src_dir(getcwd(nullptr, 256))
    {
    Log::instance().init();
    assert(!_src_dir.empty());
    _src_dir += "/resources/"; // 这样加载资源路径？
    HttpConn::user_count = 0;
    HttpConn::src_dir = _src_dir.c_str();
    MysqlParam mp {
        "localhost", sql_username, sql_password,
        sql_dbname,sql_port
    };
    SqlConnPool::instance().init(mp);

    _initEventMode(trigger_mode);
    if (!_initSocket()) {
        LOG_ERROR("######## Server init failed! ########")
        _is_close = true;
        assert(!_is_close);
    }
    LOG_INFO("######## Server init done! ########")
    LOG_INFO("Port: %d", _port)
    LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                _listen_event & EPOLLET ? "ET" : "LT",
                _conn_event & EPOLLET ? "ET" : "LT")
    LOG_INFO("Open Linger: %d", _opt_linger ? "true" : "false")
    LOG_INFO("Src Dir: %s", HttpConn::src_dir)
    LOG_INFO("SqlConnPool size: %d, ThreadPool size: %d",
            SqlConnPool::instance().getMaxSize(), _thread_pool->getMaxSize())
}

Server::~Server() {
    close(_listen_fd);
    _is_close = true;
    free((void*)_src_dir.c_str());
    SqlConnPool::instance().close();
}

void Server::start() {
    int time_ms = -1;
    if (!_is_close) {
        LOG_INFO("######## Server start! ########")
    }
    while (!_is_close) {
        if (_timeout_ms > 0) {
            time_ms = _timer->getNextTick();
        }
        int event_count = _epoller->wait(time_ms);
        for (int i = 0; i < event_count; ++i) {
            int fd = _epoller->getEventFd(i);
            uint32_t events = _epoller->getEvents(i);
            // 如果是监听socket，尝试建立连接
            if (fd == _listen_fd) {
                _dealListen();
                continue;
            }
            // 如果既不是监听也不是连接socket，报错
            if (_users.find(fd) == _users.end()) {
                LOG_ERROR("Bad fd when dealing events!")
                assert(_users.find(fd) != _users.end());
            }
            HttpConn* conn = &_users[fd];
            // 连接socket，处理不同事件
            // 客户端结束读 | 客户端结束读写 | 客户端错误
            if (events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                _closeConn(conn);
                continue;
            }
            if (events & EPOLLIN) {
                _dealRead(conn);
                continue;   
            }
            if (events & EPOLLOUT) {
                _dealWrite(conn);
                continue;
            }
            LOG_ERROR("Unexpected event!")
        }
    }
}

//private methods
void Server::_addClient(int fd, sockaddr_in addr) {
    assert(fd > 0);
    _users[fd].init(fd, addr);
    if (_timeout_ms > 0) {
        // bind绑定成员函数和对象以及函数参数，返回function对象供调用
        _timer->add(fd, _timeout_ms, std::bind(&Server::_closeConn, this, &_users[fd]));
    }
    _epoller->addFd(fd, EPOLLIN | _conn_event);
    _setFdNonblock(fd);
    LOG_INFO("Add new client! fd:[%s]", _users[fd].getFd());
}

void Server::_dealListen() {
    struct sockaddr_in addr;
    socklen_t len = sizeof addr;
    do {
        int fd = accept(_listen_fd, (struct sockaddr*)&addr, &len);
        if (fd < 0) {
            LOG_ERROR("Accept connection failed!")
            return;
        }
        if (HttpConn::user_count >= MAX_FD) {
            _sendError(fd, "Server busy!");
            close(fd);
            LOG_WARN("Too many clients!")
            return;
        }
        _addClient(fd, addr);
    } while (_listen_event & EPOLLET);
}

void Server::_dealRead(HttpConn* client) {
    assert(client != nullptr);
    _extendTime(client);
    _thread_pool->addTask(std::bind(&Server::_onRead, this, client));
}

void Server::_dealWrite(HttpConn* client) {
    assert(client != nullptr);
    _extendTime(client);
    _thread_pool->addTask(std::bind(&Server::_onWrite, this, client));
}

bool Server::_initSocket() {
    struct sockaddr_in addr;
    if (_port > 65535 or _port < 1024) {
        LOG_ERROR("Port error! port:[%s]", _port);
        return false;
    }
    addr.sin_family = AF_INET;
    // INADDR_ANY means "0.0.0.0"，该地址代表全部网卡地址
    // 监听该地址，即监听外部到达服务器全部网卡的请求
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(_port);
    struct linger opt_linger = { 0 };
    if (_opt_linger) {
        // 优雅关闭：启用SO_LINGER，超时时间为1s
        opt_linger.l_onoff = 1;
        opt_linger.l_linger = 1;
    }

    _listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listen_fd < 0) {
        LOG_ERROR("Create socket failed! port:[%s]", _port)
        return false;
    }

    // SOL_SOCKET 套接字层 SO_LINGER 延迟关闭
    int ret = setsockopt(_listen_fd, SOL_SOCKET, SO_LINGER, (const void*)&opt_linger, sizeof opt_linger);
    if (ret < 0) {
        close(_listen_fd);
        LOG_ERROR("Set SO_LINGER failed! port:[%s]", _port)
        return false;
    }

    // 端口复用
    int opt_val = 1;
    ret = setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, (const void*)&opt_val, sizeof opt_val);
    if (ret < 0) {
        LOG_ERROR("Set SO_REUSEADDR failed! port:[%s]", _port)
        close(_listen_fd);
        return false;
    }

    // 绑定监听socket和相应地址
    ret = bind(_listen_fd, (const sockaddr*)&addr, sizeof addr);
    if (ret < 0) {
        LOG_ERROR("Bind addr failed! port:[%s]", _port)
        close(_listen_fd);
        return false;
    }

    // 5是三次握手后established连接数，
    ret = listen(_listen_fd, 5);
    if (ret < 0) {
        LOG_ERROR("Listen failed! port:[%s]", _port)
        close(_listen_fd);
        return false;
    }

    ret = (int)_epoller->addFd(_listen_fd, _listen_event | EPOLLIN);
    if (ret == 0) {
        LOG_ERROR("Add listen epoll failed!");
        close(_listen_fd);
        return false;
    }

    ret = _setFdNonblock(_listen_fd);
    
    LOG_INFO("Socket init done!")
    return true;
}

void Server::_initEventMode(int trigger_mode) {
    // EPOLLRDHUP 对端描述符产生一个挂断事件（读结束标志，对方不会继续写入，而此时已经存在于缓冲区的数据仍然可读）
    _listen_event = EPOLLRDHUP;
    // EPOLLONESHOT 单个socket只触发一种事件且只触发一次
    _conn_event = EPOLLONESHOT | EPOLLRDHUP;
    switch (trigger_mode) {
    case 0:
        break;
    case 1:
        _conn_event |= EPOLLET;
        break;
    case 2:
        _listen_event |= EPOLLET;
        break;
    case 3:
        _conn_event |= EPOLLET;
        _listen_event |= EPOLLET;
        break;
    default:
        _conn_event |= EPOLLET;
        _listen_event |= EPOLLET;
        break;
    }
    HttpConn::is_ET = (_conn_event & EPOLLET);
}

void Server::_sendError(int fd, const char* info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        LOG_ERROR("Send error info to client failed! fd:[%s]", fd)
    } 
}

void Server::_closeConn(HttpConn* client) {
    assert(client != nullptr);
    LOG_INFO("A client quit! fd:[%s]", client->getFd())
    _epoller->delFd(client->getFd());
    client->close_conn();
}

void Server::_onRead(HttpConn* client) {
    assert(client != nullptr);
    int read_errno = 0;
    int ret = client->read(&read_errno);
    // 这里表示读取结束但实际未到达结尾？
    if (ret <= 0 and read_errno != EAGAIN) {
        LOG_ERROR("Unknown read error!")
        _closeConn(client);
        return;
    }
    _onProcess(client);
}

void Server::_onWrite(HttpConn* client) {
    assert(client != nullptr);
    int write_errno = 0;
    int ret = client->write(&write_errno);
    if (client->getBytesToWrite() == 0) {
        // 传输成功！
        // 这里为什么要继续process？
        if (client->isKeepAlive()) {
            _onProcess(client);
            return;
        }
    }
    else if (ret < 0) {
        if (write_errno == EAGAIN) {
            _epoller->modFd(client->getFd(), _conn_event | EPOLLOUT);
            return;
        }
    }
    _closeConn(client);
}

void Server::_onProcess(HttpConn* client) {
    assert(client != nullptr);
    if (client->process() == true) {
        _epoller->modFd(client->getFd(), _conn_event | EPOLLOUT);
    } else {
        _epoller->modFd(client->getFd(), _conn_event | EPOLLIN);
    }
}

int Server::_setFdNonblock(int fd) {
    assert(fd > 0);
    // 添加non-block属性 F_GETFL or F_GETFD?
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}