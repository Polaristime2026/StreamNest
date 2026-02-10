#pragma once
#include <string>

struct User {
    int id;
    std::string email;
    std::string password_hash;
    std::string name;
};