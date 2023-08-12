#include "http_response.h"

#include <sys/stat.h>

const std::unordered_map<std::string, std::string> HttpResponse::FILE_TYPE {
    { ".html",  "text/html" },
    { ".xml",   "text/xml" },
    { ".xhtml", "application/xhtml+xml" },
    { ".txt",   "text/plain" },
    { ".rtf",   "application/rtf" },
    { ".pdf",   "application/pdf" },
    { ".word",  "application/nsword" },
    { ".png",   "image/png" },
    { ".gif",   "image/gif" },
    { ".jpg",   "image/jpeg" },
    { ".jpeg",  "image/jpeg" },
    { ".au",    "audio/basic" },
    { ".mpeg",  "video/mpeg" },
    { ".mpg",   "video/mpeg" },
    { ".avi",   "video/x-msvideo" },
    { ".gz",    "application/x-gzip" },
    { ".tar",   "application/x-tar" },
    { ".css",   "text/css "},
    { ".js",    "text/javascript "}
};

const std::unordered_map<int, std::string> HttpResponse::STATUS_CODE {
    { 200, "OK" },
    { 400, "Bad Request" },
    { 403, "Forbidden" },
    { 404, "Not Found" }
};

const std::unordered_map<int, std::string> HttpResponse::ERROR_CODE {
    { 400, "/400.html" },
    { 403, "/403.html" },
    { 404, "/404.html" }
};

HttpResponse::HttpResponse()
    : _code(-1), _keep_alive(false), _path(""),
    _src_dir(""), _file_stat({0}) {}

HttpResponse::~HttpResponse() {

}

void HttpResponse::init(const std::string& src_dir, const std::string& path, bool keep_alive, int code) {
    _src_dir = src_dir;
    _path = path;
    _keep_alive = keep_alive;
    _code = code;
}

void HttpResponse::makeResponse(Buffer& buf) {
    if (stat((_src_dir + _path).c_str(), &_file_stat) < 0 or S_ISDIR(_file_stat.st_mode)) {
        _code = 404;
    } else if (!(_file_stat.st_mode & S_IROTH)) {
        _code = 403;
    } else {
        _code = 200;
    }
    _errorHtml();
    _addStateLine(buf);
    _addHeader(buf);
    _addContent(buf);
}

// private methods
void HttpResponse::_addStateLine(Buffer& buf) {
    if (STATUS_CODE.find(_code) == STATUS_CODE.end()) {
        _code = 400;
    }
    std::string status = STATUS_CODE.at(_code);
    buf.append("HTTP/1.1 " + std::to_string(_code) + " " + status + "\r\n");
}

void HttpResponse::_addHeader(Buffer& buf) {
    buf.append("Connection: ");
    if (_keep_alive) {
        buf.append("keep-alive\r\n");
        buf.append("keep-alive: max=10, timeout=120\r\n");
    } else {
        buf.append("close\r\n");
    }
    buf.append("Content-type: " + _getFileType() + "\r\n");
}

void HttpResponse::_addContent(Buffer& buf) {
    
}

void HttpResponse::_errorHtml() {
    if (ERROR_CODE.find(_code) == ERROR_CODE.end()) {
        return;
    }
    _path = ERROR_CODE.at(_code);
    stat((_src_dir + _path).c_str(), &_file_stat);
}

std::string HttpResponse::_getFileType() const {
    std::string default_type = "text/plain";
    // auto pos = _path.rfind(".");
    auto pos = _path.find_last_of('.');
    if (pos == std::string::npos) {
        return default_type;
    }
    std::string type = _path.substr(pos);
    if (FILE_TYPE.find(type) == FILE_TYPE.end()) {
        return default_type;
    }
    return FILE_TYPE.at(type);
}