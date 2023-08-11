/**
 * @file http_request.h
 * @author weilai
 * @brief 
 * @version 0.1
 * @date 2023-08-09
 * 
 * @copyright Copyleft Apache 2.0
 * 
 */

#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include "log/log.h"
#include "buffer/buffer.h"

#include <string>
#include <unordered_map>

class HttpRequest {
public:
    // 违反直觉！enum中的元素属于enum所在作用域，而不属于enum对象
    // 但enum class的元素属于enum class
    // 强枚举类型，不允许隐式转换
    enum class PARSE_STATE {
        REQUEST_LINE,
        HEADERS,
        BODY, // 只对POST请求
        FINISH
    };

    enum class HTTP_CODE {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        INTERNAL_ERROR
    };

public:
    HttpRequest();
    ~HttpRequest() = default;

    void init();
    bool parse(Buffer& buf);

private:
    bool _parseRequestLine(const std::string& line);
    bool _parseHeaders(const std::string& line);
    bool _parseBody(const std::string& line);

    bool _parsePath();
    bool _parsePost();
    bool _parseEncodedUrl();

    bool _userVerify(const std::string& username, const std::string& password);
    bool _userRegister(const std::string& username, const std::string& password);
    
    PARSE_STATE _state;
    std::string _method, _path, _version, _body;
    std::unordered_map<std::string, std::string> _headers;
    std::unordered_map<std::string, std::string> _post;

    static const std::unordered_set<std::string> DEFAULT_HTML;
    static const std::unordered_map<std::string, int> LOGIN_OPTIONS;
};

#endif // HTTP_REQUEST_H