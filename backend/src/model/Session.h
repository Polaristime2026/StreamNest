#pragma once
#include <chrono>

struct Session {
    int user_id;
    std::chrono::system_clock::time_point expires_at;
};