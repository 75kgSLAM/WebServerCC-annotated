#include "server/server.h"
#include "http/http_conn.h"
#include "pool/sql_conn_pool.h"

#include <unistd.h>
#include <assert.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

Server::Server(
    int port, int trigger_mode, int timeout_ms, bool opt_linger,
    int sql_port, const char* sql_username, const char* sql_password, const char* sql_dbname)
    : _port(port), _opt_linger(opt_linger), _is_close(false), _src_dir(getcwd(nullptr, 256))
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
    if (!_is_close) {
        LOG_INFO("######## Server start! ########")
    }
    while (!_is_close) {
        if (_timeout_ms > 0) {
            
        }
    }
}

//private methods
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

int Server::_setFdNonblock(int fd) {
    assert(fd > 0);
    // 添加non-block属性 F_GETFL or F_GETFD?
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}