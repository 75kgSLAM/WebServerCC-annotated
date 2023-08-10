#include "http_request.h"
#include "log/log.h"

#include <algorithm>
#include <regex>

HttpRequest::HttpRequest() {
    init();
}

void HttpRequest::init() {
    _method = _path = _version = _body = "";
    _state = PARSE_STATE::REQUEST_LINE;
    _header.clear();
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
                if (!_parseRequestLine(line)) {
                    return false;
                }
                _parsePath();
                break;
            case PARSE_STATE::HEADERS:
                _parseHeaders(line);
                break;
            case PARSE_STATE::BODY:
                _parseBody(line);
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

bool HttpRequest::_parseRequestLine(const std::string& line) {
    std::regex pattern("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch submatch;
    if (regex_match(line, submatch, pattern)) {
        _method = submatch[1];
        _path = submatch[2];
        _version = submatch[3];
        _state = PARSE_STATE::HEADERS;
        return true;
    }
    LOG_ERROR("Match failed! Bad RequestLine!")
    return false;
}

bool HttpRequest::_parseHeaders(const std::string& line) {
    // 匹配到空行直接继续解析消息体
    if (line.empty()) {
        _state = PARSE_STATE::BODY;
        return true;
    }
    std::regex pattern("^([^:]*): ?(.*)$");
    std::smatch submatch;
    if (regex_match(line, submatch, pattern)) {
        _header[submatch[1]] = submatch[2];
        return true;
    } else {
        LOG_ERROR("Match Failed! Bad headers!")
        return false;
    }
}