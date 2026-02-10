#include "AuthService.h"
#include "util/AppUtils.h"
#include <sstream>

AuthService::AuthService(std::shared_ptr<UserRepository> userRepository)
    : userRepository_(userRepository) {}

std::optional<std::string> AuthService::login(const std::string& email, const std::string& password) {
    const auto user = userRepository_->findByEmail(email);
    if (!user.has_value() || user->password_hash != AppUtils::weak_hash_password(password)) {
        return std::nullopt;
    }

    const std::string token = AppUtils::make_token();
    Session session;
    session.user_id = user->id;
    session.expires_at = std::chrono::system_clock::now() + std::chrono::hours(4);
    sessions_[token] = session;

    // 构建返回的 JSON 字符串
    std::ostringstream body;
    body << "{"
         << "\"token\":\"" << AppUtils::json_escape(token) << "\","
         << "\"expiresIn\":14400,"
         << "\"user\":{"
         << "\"id\":" << user->id << ","
         << "\"email\":\"" << AppUtils::json_escape(user->email) << "\","
         << "\"name\":\"" << AppUtils::json_escape(user->name) << "\""
         << "}"
         << "}";
    return body.str();
}

std::optional<User> AuthService::getMe(const std::string& token) {
    if (!sessions_.count(token)) {
        return std::nullopt;
    }
    
    const Session& session = sessions_.at(token);
    const auto now = std::chrono::system_clock::now();
    
    if (session.expires_at <= now) {
        sessions_.erase(token);
        return std::nullopt;
    }

    return userRepository_->findById(session.user_id);
}

bool AuthService::logout(const std::string& token) {
    if (sessions_.count(token)) {
        sessions_.erase(token);
        return true;
    }
    return false;
}

void AuthService::cleanupExpiredSessions() {
    const auto now = std::chrono::system_clock::now();
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (it->second.expires_at <= now) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}