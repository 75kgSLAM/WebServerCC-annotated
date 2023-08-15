/**
 * @file server.h
 * @author weilai
 * @brief 
 * @version 0.1
 * @date 2023-08-13
 * 
 * @copyright Copyrleft Apache 2.0
 * 
 */

#ifndef SERVER_H
#define SERVER_H

#include "log/log.h"
#include "pool/thread_pool.hpp"
#include "pool/sql_conn_pool.h"
#include "timer/timer.h"
#include "epoller.h"

#include <string>
#include <unordered_map>

class Server {
public:
    /**
     * @brief Construct a new Server object
     * 
     * @param port 端口号
     * @param trigger_mode ET/LT
     * @param timeout 单位ms
     * @param opt_linger 优雅关闭
     * @param sql_port sql端口号
     * @param sql_username sql用户名
     * @param sql_password sql密码
     * @param sql_dbname sql数据库名
     * @param use_log 是否启用日志
     * @param log_level 默认日志等级
     */
    Server(
        int port, int trigger_mode, int timeout_ms, bool opt_linger,
        int sql_port, const char* sql_username, const char* sql_password, const char* sql_dbname
    );
    ~Server();
    void start();

private:
    bool _initSocket();
    void _initEventMode(int trigger_mode);
    void _addClient(int fd, sockaddr_in addr);

    void _dealListen();
    void _dealRead(HttpConn* client);
    void _dealWrite(HttpConn* client);

    void _sendError(int fd, const char* info);
    void _extendTime(HttpConn* client);
    void _closeConn(HttpConn* client);

    void _onRead(HttpConn* client);
    void _onWrite(HttpConn* client);
    void _onProcess(HttpConn* client);

    int _setFdNonblock(int fd);
    
    static const int MAX_FD = 65536;

    int _port;
    int _timeout_ms;
    bool _opt_linger;
    bool _is_close;
    int _listen_fd;

    // 注意！_src_dir由getcwd()产生，getcwd()使用了malloc，
    // 程序结束需要free()掉_src_dir内部的指针
    std::string _src_dir;

    // 我猜这是监听socket和连接socket
    // bit_mask 32个二级制位对应不同属性
    uint32_t _listen_event;
    uint32_t _conn_event;

    std::unique_ptr<ThreadPool> _thread_pool;
    std::unordered_map<int, HttpConn> _users;
    std::unique_ptr<Epoller> _epoller;
    std::unique_ptr<Timer> _timer;
};

#endif // SERVER_H