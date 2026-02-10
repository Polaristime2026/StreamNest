#include "AuthController.h"
#include "util/AppUtils.h"
#include <sstream>

AuthController::AuthController(std::shared_ptr<AuthService> authService)
    : authService_(authService) {}

void AuthController::handleRequest(int client_fd, const HttpRequest& req) {
    // 每次请求前简单清理过期会话
    authService_->cleanupExpiredSessions();

    if (req.method == "OPTIONS") {
        HttpServer::write_response(client_fd, "204 No Content", "", "application/json");
        return;
    }

    // 路由匹配
    if (req.method == "GET" && req.path == "/api/health") {
        HttpServer::write_response(client_fd, "200 OK", "{\"ok\":true}");
        return;
    }
    if (req.method == "POST" && req.path == "/api/login") {
        handleLogin(client_fd, req);
        return;
    }
    if (req.method == "GET" && req.path == "/api/me") {
        handleMe(client_fd, req);
        return;
    }
    if (req.method == "POST" && req.path == "/api/logout") {
        handleLogout(client_fd, req);
        return;
    }

    HttpServer::write_response(client_fd, "404 Not Found", "{\"error\":\"not found\"}");
}

void AuthController::handleLogin(int client_fd, const HttpRequest& req) {
    const auto email = AppUtils::extract_json_string(req.body, "email");
    const auto password = AppUtils::extract_json_string(req.body, "password");

    if (!email.has_value() || !password.has_value()) {
        HttpServer::write_response(client_fd, "400 Bad Request", "{\"error\":\"email or password missing\"}");
        return;
    }

    auto result = authService_->login(*email, *password);
    if (result.has_value()) {
        HttpServer::write_response(client_fd, "200 OK", *result);
    } else {
        HttpServer::write_response(client_fd, "401 Unauthorized", "{\"error\":\"invalid credentials\"}");
    }
}

void AuthController::handleMe(int client_fd, const HttpRequest& req) {
    const auto token = extractBearerToken(req);
    if (!token.has_value()) {
        HttpServer::write_response(client_fd, "401 Unauthorized", "{\"error\":\"unauthorized\"}");
        return;
    }

    auto user = authService_->getMe(*token);
    if (!user.has_value()) {
        HttpServer::write_response(client_fd, "401 Unauthorized", "{\"error\":\"session expired or invalid\"}");
        return;
    }

    std::ostringstream body;
    body << "{"
         << "\"id\":" << user->id << ","
         << "\"email\":\"" << AppUtils::json_escape(user->email) << "\","
         << "\"name\":\"" << AppUtils::json_escape(user->name) << "\""
         << "}";
    HttpServer::write_response(client_fd, "200 OK", body.str());
}

void AuthController::handleLogout(int client_fd, const HttpRequest& req) {
    const auto token = extractBearerToken(req);
    if (token.has_value()) {
        authService_->logout(*token);
    }
    HttpServer::write_response(client_fd, "200 OK", "{\"ok\":true}");
}

std::optional<std::string> AuthController::extractBearerToken(const HttpRequest& req) const {
    const auto it = req.headers.find("authorization");
    if (it == req.headers.end()) {
        return std::nullopt;
    }
    const std::string prefix = "Bearer ";
    if (it->second.rfind(prefix, 0) != 0) {
        return std::nullopt;
    }
    return it->second.substr(prefix.size());
}