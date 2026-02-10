#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <optional>
#include "repository/UserRepository.h"
#include "model/Session.h"

class AuthService {
public:
    // 构造函数注入 Repository
    AuthService(std::shared_ptr<UserRepository> userRepository);

    std::optional<std::string> login(const std::string& email, const std::string& password);
    std::optional<User> getMe(const std::string& token);
    bool logout(const std::string& token);
    void cleanupExpiredSessions();

private:
    std::shared_ptr<UserRepository> userRepository_;
    std::unordered_map<std::string, Session> sessions_; // 内存 Session 存储
};