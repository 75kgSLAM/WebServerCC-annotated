/**
 * @file log.h
 * @author weilai
 * @brief 
 * @version 0.1
 * @date 2023-08-03
 * 
 * @copyright Copyleft Apache 2.0
 * 
 */

#ifndef LOG_H
#define LOG_H

#include <string>
#include <thread>
#include <mutex>

#include "block_queue.hpp"
#include "buffer.h"

class Log {
public:
    /**
     * @brief Singleton Lazy create Log object
     * 
     * @return Log& 
     */
    static Log& instance();
    
    void init(int level = 1, const char* dirname = "./log", 
                const char* filename = ".log", int maxsize = 1024);
    
    void write(int level, const char* format, ...);
    void flush();

    int getLevel() const;
    void setLevel(int level);
    bool isOpen() const;

    void flushLogThread();

private:
    Log();
    virtual ~Log();
    void _appendLogLevelTitle(int level);
    void _asyncWrite();

private:
    // 为什么用const char*而不用const string？
    // 因为const string无法允许我们在init()时才初始化
    // 但我们在init()时才能知道日志的名字
    const char* _dirname;
    const char* _filename;

    const int _MAX_LINES = 50000;
    const int _LOG_PATH_LEN = 256;

    int _line_count;
    int _today;

    bool _is_open;
    int _level;
    bool _is_async;

    Buffer _buf;
    FILE* _fp;
    std::unique_ptr<BlockQueue<std::string>> _que;
    std::unique_ptr<std::thread> _write_thread;
    std::mutex _m;
};

// 可变参数宏提供写日志接口，优先级更高的日志会被写入
#define LOG_BASE(level, format, ...) \
    do { \
        Log log = Log::instance(); \
        if (log.isOpen() and log.getLevel() <= level) { \
            log.format(level, format, ##__VA_ARGS__); \
            log.flush(); \
        } \
    } while(0); // 注意这里的分号
// 日志分级，注意结尾的分号，宏替换时是不做语法检查的
#define LOG_DEBUG(format, ...) do { LOG_BASE(0, format, ##__VA_ARGS__) } while(0);
#define LOG_INFO(format, ...) do { LOG_BASE(1, format, ##__VA_ARGS__) } while(0);
#define LOG_WARN(format, ...) do { LOG_BASE(2, format, ##__VA_ARGS__) } while(0);
#define LOG_ERROR(format, ...) do { LOG_BASE(3, format, ##__VA_ARGS__) } while(0);

#endif // LOG_H