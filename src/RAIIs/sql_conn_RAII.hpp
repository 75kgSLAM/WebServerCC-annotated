/**
 * @file sql_conn_RAII.hpp
 * @author weilai
 * @brief 封装MYSQL连接从池中获取和归还的操作
 * @version 0.1
 * @date 2023-08-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef SQL_CONN_RAII_HPP
#define SQL_CONN_RAII_HPP

#include "pool/sql_conn_pool.h"

#include <mysql/mysql.h>

typedef class SqlConnRAII {
public:
    SqlConnRAII(SqlConnPool* conn_pool) {
        assert(conn_pool);
        _conn_pool = conn_pool;
        _mysql = _conn_pool->getConn();
        assert(_mysql);
    }

    ~SqlConnRAII() {
        if(_mysql != nullptr) {
            _conn_pool->freeConn(_mysql);
        }
    }

    MYSQL* operator->() {
        return _mysql;
    }

    MYSQL* operator&() {
        return _mysql;
    }
    
private:
    MYSQL* _mysql;
    // why pointer?
    SqlConnPool* _conn_pool;
} SqlConn;

#endif // SQL_CONN_RAII_HPP