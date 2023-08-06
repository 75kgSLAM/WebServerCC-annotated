/**
 * @file buffer.cc
 * @author weilai
 * @brief 
 * @version 0.1
 * @date 2023-08-05
 * 
 * @copyright Copyleft Apache 2.0
 * 
 */

#include <cstring>

#include "buffer.h"

Buffer::Buffer(int init_size)
    : _buf(init_size), _write_pos(0), _read_pos(0) {};

size_t Buffer::getWritableBytes() const {
    return _buf.size() - _write_pos;
}

size_t Buffer::getReadableBytes() const {
    return _write_pos - _read_pos; 
}

size_t Buffer::getPrependableBytes() const {
    return _read_pos;
}

const char* Buffer::getReadPos() const {
    return _beginCharPtr() + _read_pos;
}

char* Buffer::getWritePos() {
    return _beginCharPtr() + _write_pos;
}

void Buffer::hasWritten(size_t len) {
    _write_pos += len;
}

void Buffer::retrieve(size_t len) {
    assert(len <= getReadableBytes());
    _read_pos += len;
}

void Buffer::retrieveAll() {
    memset(&_buf, 0, _buf.size());
}
std::string Buffer::retrieveAllTOString() {
    std::string str(getReadPos(), getReadableBytes());
    retrieveAll();
    return str;
}

void Buffer::append(const std::string& s) {
    append(s.data(), s.length());
}

void Buffer::append(const char* str, size_t len) {
    assert(str != nullptr);
    ensureWritable(len);
    std::copy(str, str + len, getWritePos());
    hasWritten(len);
}

void Buffer::ensureWritable(size_t len) {
    while (getWritableBytes() < len) {
        _extendSpace(len);
    }
    // assert(getWritableBytes() >= len);
}

char* Buffer::_beginCharPtr() {
    // 取vector中第一个字符的地址
    assert(!_buf.empty());
    return &_buf[0];
}

// 重载const版本
const char* Buffer::_beginCharPtr() const {
    // 取vector中第一个字符的地址
    assert(!_buf.empty());
    return &_buf[0];
}

void Buffer::_extendSpace(size_t len) {
    if (getWritableBytes() + getPrependableBytes() < len) {
        _buf.resize(_write_pos + len + 1);
    } else {
        size_t readable_len = getReadableBytes();
        std::copy(_beginCharPtr() + _read_pos, _beginCharPtr() + _write_pos, _beginCharPtr());
        _read_pos = 0;
        _write_pos = _read_pos + readable_len;
        // assert(readable_len == getReadableBytes());
    }
}