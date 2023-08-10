/**
 * @file buffer.h
 * @author weilai
 * @brief 
 * @version 0.1
 * @date 2023-08-05
 * 
 * @copyright Copyleft Apache 2.0
 * 
 */

#ifndef BUFFER_H
#define BUFFER_H

#include <vector>
#include <atomic>
#include <string>

#include "assert.h"

class Buffer {
public:
    Buffer(int init_size = 1024);
    // use default destructor
    ~Buffer() = default;

    // buffer is a vector shown below     
    // v[begin]--prepandable--v[_read_pos]--readable--v[_write_pos]--writable--v[end]
    size_t getWritableBytes() const;
    size_t getReadableBytes() const;
    size_t getPrependableBytes() const;

    const char* getReadPos() const;
    char* getWritePos();
    const char* getWritePosConst() const;
    void hasWritten(size_t len);

    void retrieve(size_t len);
    void retrieveUntil(const char* end);
    void retrieveAll();
    std::string retrieveAllTOString();

    void append(const std::string& s);
    void append(const char* str, size_t len);
    void append(const void* data, size_t len);
    void append(const Buffer& buf);

    void ensureWritable(size_t len);
    
private:
    char* _beginCharPtr();
    const char* _beginCharPtrConst() const;
    void _extendSpace(size_t len);

    std::vector<char> _buf;
    std::atomic<size_t> _write_pos;
    std::atomic<size_t> _read_pos;
};

#endif // BUFFER_H