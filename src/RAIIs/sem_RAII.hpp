#ifndef SEMRAII_HPP
#define SEMRAII_HPP

#include "log/log.h"

#include "semaphore.h"

typedef class SemRAII {
public:
    explicit SemRAII(int value) {
        // 0表示仅在当前进程中共享，value为初值
        if (sem_init(&_s, 0, value) != 0) {
            LOG_ERROR("Semaphore init failed!");
            throw std::exception(); // ??
        }
    }

    ~SemRAII() {
        sem_destroy(&_s);
    }

    // 类外无法访问实际的信号计数量，对外开放PV操作的接口
    // P操作
    bool wait() {
        return sem_wait(&_s) == 0;
    }
    // V操作
    bool post() {
        return sem_post(&_s) == 0;
    }
private:
    sem_t _s;
} sem;

#endif // SEMRAII_HPP