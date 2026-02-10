#pragma once
#include <memory>
#include "server/HttpServer.h"
#include "service/AuthService.h"

class AuthController {
public:
    AuthController(std::shared_ptr<AuthService> authService);

    // 核心路由分发器
    void handleRequest(int client_fd, const HttpRequest& req);

private:
    std::shared_ptr<AuthService> authService_;

    void handleLogin(int client_fd, const HttpRequest& req);
    void handleMe(int client_fd, const HttpRequest& req);
    void handleLogout(int client_fd, const HttpRequest& req);
    std::optional<std::string> extractBearerToken(const HttpRequest& req) const;
};