#include "UserRepository.h"
#include "util/AppUtils.h"

UserRepository::UserRepository() {
    // 初始化模拟数据
    User demo_user;
    demo_user.id = 1;
    demo_user.email = "student@campus.edu";
    demo_user.password_hash = AppUtils::weak_hash_password("123456");
    demo_user.name = "Demo Student";
    users_.push_back(demo_user);
}

std::optional<User> UserRepository::findByEmail(const std::string& email) const {
    for (const auto& user : users_) {
        if (AppUtils::to_lower(user.email) == AppUtils::to_lower(email)) {
            return user;
        }
    }
    return std::nullopt;
}

std::optional<User> UserRepository::findById(int id) const {
    for (const auto& user : users_) {
        if (user.id == id) {
            return user;
        }
    }
    return std::nullopt;
}