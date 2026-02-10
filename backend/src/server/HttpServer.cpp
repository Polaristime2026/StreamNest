#include "HttpServer.h"
#include "util/AppUtils.h"
#include <iostream>
#include <sstream>
#include <cstring>

// === 核心修改开始：自动判断是 Windows 还是 Linux ===
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    // Windows 下关闭 Socket 的函数叫 closesocket
    #define CLOSE_SOCKET closesocket
    // Windows 下 Socket 类型是 SOCKET (unsigned long long)
    using SocketType = SOCKET;
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
    // Linux 下关闭 Socket 的函数叫 close
    #define CLOSE_SOCKET close
    // Linux 下 Socket 类型是 int
    using SocketType = int;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif
// === 核心修改结束 ===

void HttpServer::run(uint16_t port, RequestHandler handler) {
#ifdef _WIN32
    // Windows 必须先初始化 Winsock 库
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return;
    }
#endif

    const SocketType server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        std::cerr << "socket() failed\n";
        return;
    }

    int opt = 1;
    // Windows 下 setsockopt 第4个参数需要强转为 const char*
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "bind() failed\n";
        CLOSE_SOCKET(server_fd);
        return;
    }

    if (listen(server_fd, 16) == SOCKET_ERROR) {
        std::cerr << "listen() failed\n";
        CLOSE_SOCKET(server_fd);
        return;
    }

    std::cout << "Auth server listening on http://127.0.0.1:" << port << "\n";
    while (true) {
        SocketType client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd == INVALID_SOCKET) {
            continue;
        }
        // 强转为 int 传入 handle_client (为了保持兼容性)
        handle_client(static_cast<int>(client_fd), handler);
        CLOSE_SOCKET(client_fd);
    }

#ifdef _WIN32
    WSACleanup(); // 清理 Winsock
#endif
}

void HttpServer::handle_client(int client_fd, RequestHandler handler) {
    auto req = read_request(client_fd);
    if (!req.has_value()) {
        write_response(client_fd, "400 Bad Request", "{\"error\":\"invalid request\"}");
        return;
    }
    handler(client_fd, *req);
}

std::optional<HttpRequest> HttpServer::read_request(int client_fd) {
    std::string raw;
    char buffer[4096];
    
    // Windows recv 返回的是 int，Linux 是 ssize_t
    int n = 0; 
    while (raw.find("\r\n\r\n") == std::string::npos) {
        n = ::recv(client_fd, buffer, sizeof(buffer), 0);
        if (n <= 0) {
            return std::nullopt;
        }
        raw.append(buffer, buffer + n);
        if (raw.size() > 65536) {
            return std::nullopt;
        }
    }

    const size_t header_end = raw.find("\r\n\r\n");
    std::string header_part = raw.substr(0, header_end);
    std::string body_part = raw.substr(header_end + 4);

    std::istringstream hs(header_part);
    std::string first_line;
    if (!std::getline(hs, first_line)) {
        return std::nullopt;
    }
    if (!first_line.empty() && first_line.back() == '\r') {
        first_line.pop_back();
    }

    std::istringstream first_line_stream(first_line);
    HttpRequest req;
    std::string http_version;
    first_line_stream >> req.method >> req.path >> http_version;
    if (req.method.empty() || req.path.empty()) {
        return std::nullopt;
    }

    std::string line;
    while (std::getline(hs, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const size_t p = line.find(':');
        if (p == std::string::npos) {
            continue;
        }
        const std::string key = AppUtils::to_lower(AppUtils::trim(line.substr(0, p)));
        const std::string value = AppUtils::trim(line.substr(p + 1));
        req.headers[key] = value;
    }

    size_t content_length = 0;
    if (req.headers.count("content-length")) {
        try {
            content_length = static_cast<size_t>(std::stoul(req.headers["content-length"]));
        } catch (...) {
            return std::nullopt;
        }
    }

    while (body_part.size() < content_length) {
        n = ::recv(client_fd, buffer, sizeof(buffer), 0);
        if (n <= 0) {
            break;
        }
        body_part.append(buffer, buffer + n);
    }
    if (body_part.size() > content_length) {
        body_part.resize(content_length);
    }
    req.body = body_part;
    return req;
}

bool HttpServer::send_all(int client_fd, const std::string& payload) {
    size_t sent = 0;
    while (sent < payload.size()) {
        const int n = ::send(client_fd, payload.data() + sent, static_cast<int>(payload.size() - sent), 0);
        if (n <= 0) {
            return false;
        }
        sent += static_cast<size_t>(n);
    }
    return true;
}

void HttpServer::write_response(int client_fd,
                                const std::string& status,
                                const std::string& body,
                                const std::string& content_type) {
    std::ostringstream out;
    out << "HTTP/1.1 " << status << "\r\n";
    out << "Content-Type: " << content_type << "\r\n";
    out << "Content-Length: " << body.size() << "\r\n";
    out << "Connection: close\r\n";
    out << "Access-Control-Allow-Origin: *\r\n";
    out << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    out << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    out << "\r\n";
    out << body;
    const std::string payload = out.str();
    send_all(client_fd, payload);
}