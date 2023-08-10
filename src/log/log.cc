/**
 * @file log.cc
 * @author weilai
 * @brief Log类初始化、格式化数据、写入等功能的具体实现
 * @version 0.1
 * @date 2023-08-05
 * 
 * @copyright Copyleft Apache 2.0
 * 
 */

#include "log.h"

#include "assert.h"
#include "sys/time.h"
#include "stdarg.h" // va_list va_start va_end

Log& Log::instance() {
    static Log log;
    return log;
}

void Log::init(int level = 1, const char* dirname, const char* filename, int maxsize) {
    // 队列长度大于零，说明使用了异步写入
    if (maxsize > 0) {
        _is_async = true;
        _que = std::make_unique<BlockQueue<std::string>>(maxsize);
        _write_thread = std::make_unique<std::thread>(flushLogThread);

    }
    _is_open = true;
    _level = level;
    _dirname = dirname;
    _filename = filename;
    // C API获取当前相对时间
    time_t t = time(nullptr);
    tm* now = localtime(&t);
    auto& n = *now;
    char path[_LOG_PATH_LEN];
    snprintf(path, _LOG_PATH_LEN-1, "%s/%04d_%02d_%02d_%s",
            _dirname, n.tm_year + 1900, n.tm_mon + 1, n.tm_mday, _filename);
    _today = n.tm_mday;
    // 打开目标文件，双检锁保证多线程只init一次?
    // if (_fp == nullptr) 
    {
    // why lock? 多线程只能有一个线程操作_fp
        std::lock_guard<std::mutex> locker(_m);
        if (_fp != nullptr) {
            flush();
            fclose(_fp);
        }
        // "a" means "append"
        _fp = fopen(path, "a");
        assert(_fp != nullptr);
    }
}

void Log::write(int level, const char* format, ...) {
    // gettimeofday获取到微秒级的时间，用于具体日志信息时间记录
    timeval t = {0, 0};
    gettimeofday(&t, nullptr);
    time_t sec = t.tv_sec;
    tm* now = localtime(&sec);
    auto& n = *now;
    va_list valist;

    // 处理日志日期变动和当前日志写满的情况
    if (n.tm_mday != _today or (_line_count != 0 and (_line_count % _MAX_LINES == 0))) {
        char newpath[_LOG_PATH_LEN];
        char tail[16];
        snprintf(tail, 16, "%04d_%02d_%02d", n.tm_year + 1900, n.tm_mon + 1, n.tm_mday);
        if (n.tm_mday != _today) {
            snprintf(newpath, _LOG_PATH_LEN - 1, "%s/%s%s", _dirname, tail, _filename);
            _today = n.tm_mday;
            _line_count = 0;
        } else {
            // 加后缀 -1,-2,-3,...
            snprintf(newpath, _LOG_PATH_LEN - 1, "%s/%s%s-%d", _dirname, tail, _filename, (_line_count / _MAX_LINES));
        }

        {
            std::lock_guard<std::mutex> locker(_m);
            flush();
            fclose(_fp);
            _fp = fopen(newpath, "a");
            assert(_fp != nullptr);
        }
    }
    // 写入单条日志
    {
        std::lock_guard<std::mutex> locker(_m);
        ++_line_count;

        // 写入日期
        int slen = snprintf(_buf.getWritePos(), 48, "%04d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    n.tm_year + 1900, n.tm_mon + 1, n.tm_mday, n.tm_hour,
                    n.tm_min, n.tm_sec, t.tv_usec);
        _buf.hasWritten(slen);
        
        // 写入日志等级
        _appendLogLevelTitle(level);
        
        // 从...读出参数到valist，根据format指定的格式写入_buf
        va_start(valist, format);
        int vlen = vsnprintf(_buf.getWritePos(), _buf.getWritableBytes(), format, valist);
        va_end(valist); // 结束读出
        _buf.hasWritten(vlen);
        _buf.append("\n\0");

        // 异步：加入队列 同步：直接写入
        if (_is_async and _que != nullptr and !_que->full()) {
            _que->push(_buf.retrieveAllTOString());
        } else {
            fputs(_buf.getReadPos(), _fp);
        }
        _buf.retrieveAll();
    } 
}

void Log::flush() {
    if (_is_async) {
        _que->flush();
    }
    fflush(_fp);
}

// get set isopen等方法是否需要加锁？
int Log::getLevel() const {
    return _level;
}

void Log::setLevel(int level) {
    std::lock_guard<std::mutex> locker(_m);
    _level = level;
}

bool Log::isOpen() const {
    return _is_open;
}

void Log::flushLogThread() {
    Log::instance()._asyncWrite();
}

// private methods
Log::Log()
    : _line_count(0), _today(0), _is_open(false), 
    _level(0), _is_async(false), _que(nullptr),
    _write_thread(nullptr), _fp(nullptr) {}

Log::~Log() {
    if (_write_thread != nullptr && _write_thread->joinable()) {
        while (!_que->empty()) {
            _que->flush();
        }
        _que->close();
        _write_thread->join();
    }
    if (_fp != nullptr) {
        std::lock_guard<std::mutex> locker(_m);
        if (_fp != nullptr) {
            flush();
            fclose(_fp);
        }
    }
}

void Log::_appendLogLevelTitle(int level) {
    std::string title = [&]() {
        switch (level) {
            case 0: return "[DEBUG]: ";
            case 1: return "[INFO] : ";
            case 2: return "[WARN] : ";
            case 3: return "[ERROR]: ";
            default: return "[INFO] : ";
        }
    }();
    _buf.append(title);
}

void Log::_asyncWrite() {
    std::string str;
    while (_que->pop(str)) {
        std::lock_guard<std::mutex> locker(_m);
        fputs(str.data(), _fp);
    }
}