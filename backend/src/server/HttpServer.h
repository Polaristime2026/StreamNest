#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <optional>
#include "model/HttpRequest.h"

// 定义处理请求的回调函数类型 (相当于 DispatcherServlet)
using RequestHandler = std::function<void(int, const HttpRequest&)>;

class HttpServer {
public:
    // 启动服务器
    static void run(uint16_t port, RequestHandler handler);

    // 发送响应 (静态工具方法)
    static void write_response(int client_fd,
                               const std::string& status,
                               const std::string& body,
                               const std::string& content_type = "application/json; charset=utf-8");

private:
    static void handle_client(int client_fd, RequestHandler handler);
    static std::optional<HttpRequest> read_request(int client_fd);
    static bool send_all(int client_fd, const std::string& payload);
};