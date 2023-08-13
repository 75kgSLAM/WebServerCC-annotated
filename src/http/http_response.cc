#include "http_response.h"
#include "log/log.h"

#include <unordered_map>

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

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
    _src_dir(""), _file(nullptr), _file_stat({0}) {}

HttpResponse::~HttpResponse() {
    _unmapFile();
}

void HttpResponse::init(const std::string& src_dir, const std::string& path, bool keep_alive, int code) {
    if (_file != nullptr) {
        _unmapFile();
    }
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

int HttpResponse::getFileLen() const {
    return _file_stat.st_size;
}

void* HttpResponse::getFile() {
    return _file;
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
    int fd = open((_src_dir + _path).c_str(), O_RDONLY);
    if (fd < 0) {
        LOG_ERROR("Open file failed!")
        _errorContent(buf, "File NotFound!");
        return;
    }
    LOG_DEBUG("mmap file path: %s", (_src_dir + _path).c_str())
    // mmap将文件映射到内存提高访问速度，PROT_READ只读，MAP_PRIVATE建立私有写时拷贝映射
    void* ret = mmap(0, _file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    // #define MAP_FAILED ((void*)-1)
    // ((void*)-1) convert -1 to a pointer 0xFFFFFFFF
    // why -1?因为mmap是映射到虚拟内存空间的，可能会映射到0x0
    // 但永远不会映射到-1，-1永远是一个invalid返回值，所以用于表示错误 
    if (ret == MAP_FAILED) {
        LOG_ERROR("mmap failed!")
        _errorContent(buf, "File NotFound!");
        return;
    }
    _file = ret;
    close(fd);
    buf.append("Content-length: " + std::to_string(_file_stat.st_size) + "\r\n\r\n");
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

void HttpResponse::_errorContent(Buffer& buf, std::string message) {
    std::string body;
    std::string status = "Bad Request";
    if (ERROR_CODE.find(_code) != ERROR_CODE.end()) {
        status = ERROR_CODE.at(_code);
    }
    body += "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    body += std::to_string(_code) + " : " + status + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>FutrueWebServer</em></body></html>";

    buf.append("Content-length: " + std::to_string(body.size()) + "\r\n\r\n");
    buf.append(body);
}

void HttpResponse::_unmapFile() {
    if (_file != nullptr) {
        munmap(_file, _file_stat.st_size);
        _file = nullptr;
    }
}