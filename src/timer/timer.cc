#include "timer.h"

#include <assert.h>

Timer::Timer(): _heap(64) {}

Timer::~Timer() {

}

void Timer::adjustExpire(int id, int timeout) {
    assert(_ref.find(id) != _ref.end());
    size_t i = _ref[id];
    _heap[i].expire = Clock::now() + Ms(timeout);
    _siftdown(i);
}

// 添加/覆盖一个计时器
void Timer::add(int id, int timeout, const TimeoutCallback& tcb) {
    assert(id >= 0);
    size_t i;
    // 新的id，添加到堆
    if (_ref.find(id) == _ref.end()) {
        i = _heap.size();
        _ref[id] = i;
        _heap.emplace_back(id, Clock::now() + Ms(timeout), tcb);
        _siftup(i);
        return;
    }
    // 已有id，覆盖原有计时器
    i = _ref[id];
    _heap[i].expire = Clock::now() + Ms(timeout);
    _heap[i].tcb = tcb;
}

void Timer::doWork(int id) {
    if (_ref.find(id) == _ref.end()) {
        return;
    }
    size_t i = _ref[id];
    _heap[i].tcb();
    _delete(i);
}

/**
 * @brief 
 * 
 * @return -1: _heap empty
 *          0: 当前任务需要触发
 *     其他正数: 未到定时时间
 */
int Timer::getNextTick() {
    _tick();
    size_t res = -1;
    if (!_heap.empty()) {
        res = std::chrono::duration_cast<Ms>(_heap.front().expire - Clock::now()).count();
        res = res < 0 ? 0 : res;
    }
    return res;
}

//private method
void Timer::_tick() {
    while (!_heap.empty()) {
        const TimerNode& node = _heap.front();
        if (std::chrono::duration_cast<Ms>(node.expire - Clock::now()).count() > 0) {
            break;
        }
        node.tcb();
        _pop();
    }
}

void Timer::_pop() {
    assert(!_heap.empty());
    _delete(0);
}

void Timer::_delete(size_t i) {
    assert(i >= 0 and i < _heap.size());
    _swap(i, _heap.size()-1);
    _ref.erase(_heap.back().id);
    _heap.pop_back();
    _siftup(i);
    _siftdown(i);
    
}

// 向上调整只需关注父节点
void Timer::_siftup(size_t i) {
    assert(i >= 0 and i < _heap.size());
    // j为父节点索引位置
    size_t j = (i - 1) / 2;
    while (j >= 0) {
        // 重载了TimerNode的operator<
        if (_heap[j] < _heap[i]) {
            break;
        }
        _swap(i, j);
        i = j;
        j = (i - 1) / 2;
    }
}

void Timer::_siftdown(size_t i) {
    assert(i >= 0 and i < _heap.size());
    // j为左孩子索引位置
    size_t j = i * 2 + 1;
    while (j < _heap.size()) {
        if (j + 1 < _heap.size() and _heap[j + 1] < _heap[j]) {
            ++j;
        }
        if (_heap[i] < _heap[j]) {
            break;
        }
        _swap(i, j);
        i = j;
        j = i * 2 + 1;
    }
}

void Timer::_swap(size_t i, size_t j) {
    assert(i >= 0 and i < _heap.size());
    assert(j >= 0 and j < _heap.size());
    std::swap(_heap[i], _heap[j]);
    // 恢复索引的匹配
    _ref[_heap[i].id] = i;
    _ref[_heap[j].id] = j;
}