/**
 * @file http_response.h
 * @author weilai
 * @brief 
 * @version 0.1
 * @date 2023-08-09
 * 
 * @copyright Copyleft Apache 2.0
 * 
 */

#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include "buffer/buffer.h"

#include <string>
#include <unordered_map>

#include <sys/stat.h>

class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    void init(const std::string& src_dir, const std::string& path, bool keep_alive = false, int code = -1);
    
    void makeResponse(Buffer& buf);

    int getFileLen() const ;
    void* getFile();

private:
    void _addStateLine(Buffer& buf);
    void _addHeader(Buffer& buf);
    void _addContent(Buffer& buf);

    void _errorHtml();
    std::string _getFileType() const;
    void HttpResponse::_errorContent(Buffer& buf, std::string message);

    void _unmapFile();

    int _code;
    bool _keep_alive;
    std::string _path;
    std::string _src_dir;

    void* _file; // mmap file address
    struct stat _file_stat;

    static const std::unordered_map<std::string, std::string> FILE_TYPE;
    static const std::unordered_map<int, std::string> ERROR_CODE;
    static const std::unordered_map<int, std::string> STATUS_CODE;
};

#endif // HTTP_RESPONSE_H