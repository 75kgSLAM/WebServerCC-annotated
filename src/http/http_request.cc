#include "http_request.h"
#include "log/log.h"
#include "RAIIs/sql_conn_RAII.hpp"
#include "pool/sql_conn_pool.h"

#include <algorithm>
#include <regex>
#include <unordered_set>
#include <mysql/mysql.h>

const std::unordered_set<std::string> DEFAULT_HTML {
    "/", "/index", "/register", "/login",
    "/welcome", "/picture", "/video"
};

const std::unordered_map<std::string, int> LOGIN_OPTIONS {
    {"/login.html", 0}, {"/register.html", 1}
};

HttpRequest::HttpRequest() {
    // init();
}

void HttpRequest::init() {
    _method = _path = _version = _body = "";
    _state = PARSE_STATE::REQUEST_LINE;
    _headers.clear();
    _post.clear();
}

bool HttpRequest::parse(Buffer& buf) {
    const std::string CRLF = "\r\n";
    if (buf.getReadableBytes() <= 0) {
        LOG_ERROR("No request to be parsed!")
        return false;
    }
    while (buf.getReadableBytes() > 0 and _state != PARSE_STATE::FINISH) {
        // find and copy a line
        const char* line_end = std::search(buf.getReadPos(), buf.getWritePosConst(), CRLF.begin(), CRLF.end());
        std::string line(buf.getReadPos(), line_end);
        // FINISH不会进入switch
        switch (_state) {
            case PARSE_STATE::REQUEST_LINE:
                if (_parseRequestLine(line) == false) {
                    return false;
                }
                _parsePath();
                break;
            case PARSE_STATE::HEADERS:
                if (_parseHeaders(line) == false) {
                    return false;
                }
                break;
            case PARSE_STATE::BODY:
                if (_parseBody(line) == false) {
                    return false;
                }
                break;
            default:
                break;
        }
        if (line_end + 2 == buf.getWritePos()) {
            break;
        }
        buf.retrieveUntil(line_end + 2);
    }
    LOG_INFO("Request parse done: [%s], [%s], [%s]",
                _method.c_str(), _path.c_str(), _body.c_str())
    return true;
}

// private methods
bool HttpRequest::_parseRequestLine(const std::string& line) {
    std::regex pattern("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch submatch;
    if (regex_match(line, submatch, pattern)) {
        _method = submatch[1];
        _path = submatch[2];
        _version = submatch[3];
        _state = PARSE_STATE::HEADERS;
        LOG_INFO("RequestLine parse done: [%s]", submatch[0])
        return true;
    }
    LOG_ERROR("Match failed! Bad RequestLine!")
    return false;
}

bool HttpRequest::_parseHeaders(const std::string& line) {
    // 匹配到空行直接继续解析消息体
    if (line.empty()) {
        _state = PARSE_STATE::BODY;
        LOG_INFO("All RequestHeaders parse done.")
        return true;
    }
    std::regex pattern("^([^:]*): ?(.*)$");
    std::smatch submatch;
    if (regex_match(line, submatch, pattern)) {
        _headers[submatch[1]] = submatch[2];
        LOG_INFO("RequestHeader parse done: [%s]", submatch[0])
        return true;
    } else {
        LOG_ERROR("Match Failed! Bad headers!")
        return false;
    }
}

bool HttpRequest::_parseBody(const std::string& line) {
    // GET请求也可以携带body，不报错只给一个警告
    if (_method != "POST") {
        LOG_WARN("A GET request with body: [%s]", line.c_str())
        return true;
    }
    // 后面的操作可以确定这是一个POST请求
    if (line.empty()) {
        LOG_INFO("A POST request with empty body.")
        return true;
    }
    _body = line;
    if (_parsePost() == false) {
        return false;
    }
    _state = PARSE_STATE::FINISH;
    return true;
}

bool HttpRequest::_parsePath() {
    if (DEFAULT_HTML.find(_path) == DEFAULT_HTML.end()) {
        return false;
    }
    if (_path == "/") {
        _path = "/index.html";
    } else {
        _path += ".html";
    }
    return true;
}

// POST请求实现用户名和密码验证，可以隐藏具体数据
bool HttpRequest::_parsePost() {
    const std::string ct{"Content-Type"};
    if (_headers.find(ct) == _headers.end()) {
        LOG_ERROR("Find no Content-Type when parsing POST body!")
        return false;
    }
    if (_headers[ct] != "application/x-www-form-urlencoded") {
        return false;
    }
    _parseEncodedUrl();
    if (LOGIN_OPTIONS.find(_path) == LOGIN_OPTIONS.end()) {
        return true;
    }
    // const map does not support []
    // 因为map的operator[]是non-const，允许修改值以及在键不存在时插入键
    // int tag = LOGIN_OPTIONS[_path]; // error
    // 可以使用at或find取得对应的值
    // int tag = LOGIN_OPTIONS.find(_path)->second; // correct
    int tag = LOGIN_OPTIONS.at(_path);
    LOG_DEBUG("LOGIN tag: %d", tag)
    if (tag == 1) {
        if (_userRegister(_post["username"], _post["password"])) {
            return true;
        }
        return false;
    }
    if (_userVerify(_post["username"], _post["password"])) {
        _path = "/welcome.html";
    } else {
        _path = "/error.html";
    }
    return true;
    
}

bool HttpRequest::_parseEncodedUrl() {
    std::string key = "", value = "";
    // 这里用正则匹配太麻烦了 TODO：regex
    int end = 0, start = 0;
    for (; end < _body.size(); ++end) {
        char& c = _body[end];
        switch (c) {
            // 特殊地，http用'+'来编码' '，解析时转换回' '
            case '+':
                c = ' ';
                break;
            // 编码为十六进制数，需要解码
            case '%':
                // TODO
                break;
            case '=':
                key = _body.substr(start, end - start);
                start = end + 1;
                break;
            case '&':
                value = _body.substr(start, end - start);
                start = end + 1;
                if (_post.find(key) != _post.end()) {
                    LOG_WARN("Duplicate header: [%s: %s]",
                            key, _post[key])
                }
                _post[key] = value;
                LOG_DEBUG("Store a single header: [%s: %s]",
                            key.c_str(), value.c_str())
                break;
            default:
                break;
        }
    }
    // 如果结尾是一个&
    if (start > end) {
        return true;
    }
    // 处理最后一个键值对
    _post[key] = _body.substr(start, end - start);
    return true;
}

bool HttpRequest::_userVerify(const std::string& username, const std::string& password) {
    if (username == "") {
        LOG_ERROR("No username!")
        return false;
    }
    LOG_DEBUG("Verify username: %s, password: %s",
            username.c_str(), password.c_str())
    SqlConn sql(&SqlConnPool::instance());
    
    char order[256] = { 0 };
    snprintf(order, 256, "SELECT username, password FROM user WHERE username='%s' LIMIT 1", username.c_str());
    LOG_DEBUG("MySQL query: [%s]", order);

    MYSQL_RES *res = nullptr;
    if (mysql_query(&sql, order) != 0) {
        mysql_free_result(res);
        return false;
    }
    res = mysql_store_result(&sql);
    unsigned int num_fields = mysql_num_fields(res);
    MYSQL_FIELD *fields = mysql_fetch_fields(res);

    // 这里应该只fetch到一行？
    MYSQL_ROW row = mysql_fetch_row(res);
    LOG_DEBUG("Get MySQL row: %s %s", row[0], row[1]);
    std::string pwd(row[1]);
    bool f = false;
    if (pwd == password) {
        f = true;
    } else {
        LOG_INFO("Wrong password!")
    }
    // 这里可以RAII？
    mysql_free_result(res);
    return f;
}

bool HttpRequest::_userRegister(const std::string& username, const std::string& password) {
    LOG_DEBUG("New user register.")
    char order[256] = { 0 };
    snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s', '%s')", username.c_str(), password.c_str());
    LOG_DEBUG("New user register: [%s]", order);
    SqlConn sql(&SqlConnPool::instance());
    if (mysql_query(&sql, order) != 0) {
        LOG_ERROR("Register failed!")
        return false;
    }
    LOG_DEBUG("Register done!")
    return true;
}