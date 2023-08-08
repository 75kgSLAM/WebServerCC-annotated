/**
 * @file sql_conn_pool.h
 * @author weilai
 * @brief 
 * @version 0.1
 * @date 2023-08-08
 * 
 * @copyright Copyleft Apache 2.0
 * 
 */

#ifndef SQL_CONN_POOL_H
#define SQL_CONN_POOL_H

#include <mysql/mysql.h>
#include <mutex>
#include <queue>

class SqlConnPool {
public:
    static SqlConnPool& instance();

    void init();

    void close();

private:
    SqlConnPool();
    ~SqlConnPool();
    SqlConnPool(const SqlConnPool&) = delete;
    SqlConnPool& operator=(const SqlConnPool&) = delete;

private:
    const int _MAX_CONN;
    int _used_conn;
    int _free_conn;

    std::queue<MYSQL*> _conn_que;
    std::mutex _m;
};

#endif // SQL_CONN_POOL_H