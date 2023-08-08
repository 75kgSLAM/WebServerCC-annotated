#include "sql_conn_pool.h"

SqlConnPool& SqlConnPool::instance() {
    static SqlConnPool scp;
    return scp;
}