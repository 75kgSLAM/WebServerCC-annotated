/**
 * @file http_conn.h
 * @author weilai
 * @brief 
 * @version 0.1
 * @date 2023-08-09
 * 
 * @copyright Copyleft Apache 2.0
 * 
 */

#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include "log/log.h"
#include "buffer/buffer.h"
#include "http/http_request.h"
#include "http/http_response.h"

#include <arpa/inet.h> // sockaddr_in

class HttpConn {
public:
    HttpConn();

    ~HttpConn();

    void init(int sock_fd, const sockaddr_in& addr);

    // 命名需要注意，这里用到了unistd.h中的close来关闭fd
    void close_conn();

    // ssize_t means signed size type
    ssize_t read(int* save_errno);

    ssize_t write(int* save_errno);

    bool process();

    int getBytesToWrite() const;

    int getFd() const;

    const char* getIP() const;

    int getPort() const;

    std::string getHttpVersion() const;

    static bool is_ET;
    static const char* src_dir;
    static std::atomic<int> user_count;

private:
    ssize_t _readToBuf(int fd, int* err_state);
    ssize_t _writeToBuf();

private:
    int _fd;
    sockaddr_in _addr;

    bool _is_closed;

    // _iov_count不初始化，因为不一定要用到2个iov，值不确定
    int _iov_count;
    // 为什么是两个？
    iovec _iov[2];

    Buffer _read_buf;
    Buffer _write_buf;

    HttpRequest _request;
    HttpResponse _response;

    std::string _version;
};

#endif // HTTP_CONN_H