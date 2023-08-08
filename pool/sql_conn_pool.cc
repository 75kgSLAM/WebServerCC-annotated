#include "sql_conn_pool.h"
#include "../log/log.h"

#include "assert.h"

SqlConnPool& SqlConnPool::instance(int size) {
    assert(size > 10);
    static SqlConnPool scp(size);
    return scp;
}

void SqlConnPool::init(const MysqlParam& mp) {
    for (int i = 0; i < _MAX_CONN; ++i) {
        MYSQL* mysql = nullptr;
        // mysql_init可以直接传入nullptr来初始化
        // 但此时mysql_init自己创建的对象将在第一次
        // 调用mysql_close时被释放，后续调用mysql_close
        // 会访问到野指针；手动创建的指针则没有这个问题
        mysql = mysql_init(mysql);
        if(mysql == nullptr) {
            // 宏风格的调用
            LOG_ERROR("MySQL init failed!")
            assert(mysql != nullptr);
        }
        mysql = mysql_real_connect(mysql, mp.host, mp.user, mp.password,
                            mp.db_name, mp.port, nullptr, 0);
        // 这里是否可以重连？
        if (mysql == nullptr) {
            LOG_ERROR("MySQL connect failed!")
        }
        _conn_que.push(mysql);
    }
}

void SqlConnPool::close() {
    std::lock_guard<std::mutex> locker(_m);
    while (!_conn_que.empty()) {
        auto conn = std::move(_conn_que.front());
        _conn_que.pop();
        // TODO: mysql连接可以用RAII封装吗？
        mysql_close(conn);
    }
}

MYSQL* SqlConnPool::getConn() {
    // 这里需要外部工作线程自己控制是否继续连接！
    // 如果出现大量连接池为空说明连接池开小了
    if (_conn_que.empty()) {
        LOG_WARN("Get SqlConn failed! Empty SqlConnPool!")
        return nullptr;
    }
    bool f = _s.wait();
    if (f == false) {
        LOG_ERROR("Function sem_wait() failed!")
        assert(f);
    }
    MYSQL* conn = nullptr;
    {
        std::lock_guard<std::mutex> locker(_m);
        conn = _conn_que.front();
        _conn_que.pop();
    }
    return conn;
}

void SqlConnPool::freeConn(MYSQL* conn) {
    assert(conn != nullptr);
    std::lock_guard<std::mutex> locker(_m);
    _conn_que.push(conn);
    _s.post();
}

int SqlConnPool::getFreeConnCount() {
    std::lock_guard<std::mutex> locker(_m);
    return _conn_que.size();
}

// private methods
SqlConnPool::SqlConnPool(int size): _used_conn(0), _free_conn(0), _MAX_CONN(size), _s(_MAX_CONN) {}

SqlConnPool::~SqlConnPool() {
    close();
}