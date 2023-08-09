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

#include "../RAIIs/semRAII.hpp"

#include <mysql/mysql.h>
#include <mutex>
#include <queue>
/**
 * @todo C++20
 */
// #include <semaphore>

#include <semaphore.h>

// 数据库连接初始化参数封装
typedef struct MysqlConnInitParam {
    const char* host;
    const char* user;
    const char* password;
    const char* db_name;
    int port;
} MysqlParam;

class SqlConnPool {
public:
    // 单例传参，用于初始化列初始化const成员
    static SqlConnPool& instance(int size = 12);

    void init(const MysqlParam& mp);

    void close();

    MYSQL* getConn();

    void freeConn(MYSQL* conn);

    int getFreeConnCount();

private:
    SqlConnPool(int size);
    ~SqlConnPool();

    // 是否需要将这些拷贝复制操作全部删除呢？
    SqlConnPool(const SqlConnPool&) = delete;
    SqlConnPool(const SqlConnPool&&) = delete;
    SqlConnPool& operator=(const SqlConnPool&) = delete;

private:
    const int _MAX_CONN;
    int _used_conn;
    int _free_conn;

    std::queue<MYSQL*> _conn_que;
    std::mutex _m;
    sem _s;
};

#endif // SQL_CONN_POOL_H