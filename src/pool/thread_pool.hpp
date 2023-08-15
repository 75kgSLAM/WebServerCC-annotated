/**
 * @file thread_pool.hpp
 * @author weilai
 * @brief 
 * @version 0.1
 * @date 2023-08-07
 * 
 * @copyright Copyleft Apache 2.0
 * 
 */

#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>

#include "assert.h"

class ThreadPool {
public:
    // 常规单参数构造explicit避免语义混乱
    explicit ThreadPool(size_t thread_count = 8): _MAX_SIZE(thread_count), _pool(std::make_shared<Pool>()) {
        assert(thread_count > 0);
        for (size_t i = 0; i < thread_count; ++i) {
            // 创建一个线程，执行lambda指定的函数，然后detach
            std::thread(
                [pool = _pool] {
                    std::unique_lock<std::mutex> locker(pool->m);
                    // 无限循环，竞争任务队列中的具体任务
                    while(true) {
                        if(!pool->task_que.empty()) {
                            // 这里有一个点需要注意，如果使用赋值来保存front返回的对象，不仅要承担拷贝的消耗
                            // 而且pop之后，原front对象仍然存在于内存中，需要手动释放
                            // 使用move直接将front对象的生命绑定在右值引用task中
                            // 而task本身是一个局部变量，生命周期直到当前作用域结束
                            // 随着作用域结束，task生命结束，原front对象生命随之结束
                            auto task = std::move(pool->task_que.front());
                            pool->task_que.pop();
                            locker.unlock();
                            // 其他线程在等待任务队列的锁，先解锁再执行任务
                            task();
                            locker.lock();
                        } else if (pool->is_closed) break;
                        else pool->cond.wait(locker);
                    }
                }
            ).detach();
        }
    }

    ThreadPool(ThreadPool&&) = default;

    ~ThreadPool() {
        if (_pool != nullptr) {
            {
                std::lock_guard<std::mutex> locker(_pool->m);
                _pool->is_closed = true;
            }
            _pool->cond.notify_all();
        }
    }

    template<class T>
    void addTask(T&& task) {
        {
            std::lock_guard<std::mutex> locker(_pool->m);
            // forward实现完美转发（为什么要完美转发？因为task应该是一个
            // 一路被传递到工作线程的临时函数对象，如果不用完美转发，
            // 在emplace时其会被解释为一个左值，从而占用内存）
            _pool->task_que.emplace(std::forward<T>(task));
        }
        _pool->cond.notify_one();
    }

    int getMaxSize() const {
        return _MAX_SIZE;
    }

private:
    const int _MAX_SIZE;

    struct Pool {
        std::mutex m;
        std::condition_variable cond;
        bool is_closed;
        // 直接使用queue没有任何限制是否会出现队列无限加长的问题呢？
        std::queue<std::function<void()>> task_que; 
    };
    std::shared_ptr<Pool> _pool;
};

#endif // THREAD_POOL_HPP
