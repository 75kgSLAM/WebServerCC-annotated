/**
 * @file block_queue.h
 * @author weilai
 * @brief 阻塞队列封装了一个生产者-消费者模型，使用std:queue作为
 *        共享缓冲区，队列为空时，消费者被挂起；队列满时，生产者被挂起。
 *        阻塞队列可复用，声明为模板类。
 * @version 0.1
 * @date 2023-08-03
 * 
 * @copyleft Apache 2.0
 * 
 */

#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <mutex>
#include <queue>
#include <condition_variable>
#include <chrono>

#include "assert.h"

template<class T>
class BlockQueue {
public:
    /**
     * @brief Construct a new Block Queue object
     * @param capacity max capacity of the buffer queue
     */
    explicit BlockQueue(size_t capacity);
    
    ~BlockQueue();

    bool push(const T& item);

    bool pop(T& item);

    size_t size();

    size_t capacity();

    void clear();

    bool full();

    bool empty();

    void close();
    
    void flush(); // why flush?

    /**
     * @brief 
     * 
     * @param item output
     * @param timeout unit: ms
     * @details not used
     */
    bool popTimeout(T& item, int timeout);

private:
    std::queue<T> _q;
    std::mutex _m;
    std::condition_variable _cond_consumer;
    std::condition_variable _cond_producer;
    size_t _capacity;
    bool _is_closed;
};

template<class T>
BlockQueue<T>::BlockQueue(size_t capacity): _capacity(capacity) {
    assert(capacity > 0);
    _is_closed = false;
}

template<class T>
BlockQueue<T>::~BlockQueue() {
    
}

template<class T>
void BlockQueue<T>::close() {
    // lock_guard应用RAII，作用域结束析构解锁
    {
        std::lock_guard<std::mutex> locker(_m);
        _is_closed = true;
    }
    // 唤醒所有等待线程的最后机会！
    _cond_consumer.notify_all();
    _cond_producer.notify_all();
}

template<class T>
bool BlockQueue<T>::push(const T& item) {
    // lock_guard内部delete掉了拷贝和赋值构造函数，无法作为函数值传递参数传入
    // 这里使用unique_lock的真正原因是：wait函数的参数类型是unique_lock。。。
    std::unique_lock<std::mutex> locker(_m);
    while (_q.size() >= _capacity) {
        _cond_producer.wait(locker);
        if (_is_closed) {
            return false;
        }
    }
    _q.push(item);
    _cond_consumer.notify_one();
    return true;
}

template<class T>
bool BlockQueue<T>::pop(T& item) {
    std::unique_lock<std::mutex> locker(_m);
    while (_q.empty()) {
        _cond_consumer.wait(locker);
        if (_is_closed) {
            return false;
        }
    }
    item = _q.top();
    _q.pop();
    _cond_producer.notify_one();
    return true;
}

template<class T>
size_t BlockQueue<T>::size() {
    std::lock_guard<std::mutex> locker(_m);
    return _q.size();
}

template<class T>
size_t BlockQueue<T>::capacity() {
    std::lock_guard<std::mutex> locker(_m);
    return _capacity;
}

template<class T>
void BlockQueue<T>::clear() {
    std::lock_guard<std::mutex> locker(_m);
    return _q.clear();
}

template<class T>
bool BlockQueue<T>::full() {
    std::lock_guard<std::mutex> locker(_m);
    return _q.size() >= _capacity;
}

template<class T>
bool BlockQueue<T>::empty() {
    std::lock_guard<std::mutex> locker(_m);
    return _q.empty();
}

template<class T>
void BlockQueue<T>::flush() {
    _cond_consumer.notify_one();
}

template<class T>
bool BlockQueue<T>::popTimeout(T& item, int timeout) {
    std::unique_lock<std::mutex> locker(_m);
    while (_q.empty()) {
        if (_cond_consumer.wait_for(locker, std::chrono::milliseconds(timeout))
                == std::cv_status::timeout) {
            return false;
        }
        if (_is_closed) {
            return false;
        }
    }
    item = _q.top();
    _q.pop();
    _cond_producer.notify_one();
    return true;
}
#endif // BLOCK_QUEUE_H