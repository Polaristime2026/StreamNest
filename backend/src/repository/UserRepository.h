#pragma once
#include <vector>
#include <optional>
#include "model/User.h"
class UserRepository {
public:
    UserRepository();
    std::optional<User> findByEmail(const std::string& email) const;
    std::optional<User> findById(int id) const;

private:
    std::vector<User> users_;
};