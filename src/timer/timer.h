/**
 * @file timer.h
 * @author weilai
 * @brief 
 * @version 0.1
 * @date 2023-08-14
 * 
 * @copyright Copyleft Apache 2.0
 * 
 */

#ifndef TIMER_H
#define TIMER_H

#include <functional>
#include <chrono>
#include <vector>
#include <unordered_map>

typedef std::function<void()> TimeoutCallback;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds Ms;
typedef Clock::time_point TimeStamp;

struct TimerNode {
    int id;
    // expire 到期
    TimeStamp expire;
    TimeoutCallback tcb;
    bool operator<(const TimerNode& node) {
        return expire < node.expire;
    }
};

class Timer {
public:
    Timer();
    ~Timer();

    void adjustExpire(int id, int timeout);

    void add(int id, int timeout, const TimeoutCallback& tcb);

    void doWork(int id);

    int getNextTick();

private:
    void _tick();

    void _pop();

    void _delete(size_t i);

    void _siftup(size_t i);

    void _siftdown(size_t i);

    void _swap(size_t i, size_t j);

    // 定时时间小根堆
    std::vector<TimerNode> _heap;
    // 索引表：查找某个id的TimeNode在_heap中的位置
    std::unordered_map<int, size_t> _ref;
};

#endif // TIMER_H