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
     * @param conn_pool_size 数据库连接池最大容量
     * @param thread_pool_size 线程池最大容量
     * @param use_log 是否启用日志
     * @param log_level 默认日志等级
     */
    Server(
        int port, int trigger_mode, int timeout, bool opt_linger,
        int sql_port, const char* sql_username, const char* sql_password, const char* sql_dbname,
        int conn_pool_size, int thread_pool_size, bool use_log, int log_level
    );
    ~Server();
    void start();

private:
    
};

#endif // SERVER_H