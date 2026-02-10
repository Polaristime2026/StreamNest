#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <random>
#include <optional>
#include <iostream>

class AppUtils {
public:
    // 去掉字符串首尾空白字符
    static std::string trim(const std::string& s) {
        size_t begin = 0;
        while (begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin]))) {
            ++begin;
        }
        size_t end = s.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
            --end;
        }
        return s.substr(begin, end - begin);
    }

    // 转小写
    static std::string to_lower(std::string s) {
        for (char& ch : s) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        return s;
    }

    // JSON 转义
    static std::string json_escape(const std::string& s) {
        std::string out;
        out.reserve(s.size() + 8);
        for (char c : s) {
            switch (c) {
                case '\\': out += "\\\\"; break;
                case '\"': out += "\\\""; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out.push_back(c); break;
            }
        }
        return out;
    }

    // 弱哈希 (仅用于演示)
    static std::string weak_hash_password(const std::string& plain) {
        constexpr const char* kSalt = "elm-academy-salt-v1";
        std::hash<std::string> hasher;
        const auto value = hasher(std::string(kSalt) + plain);
        std::ostringstream oss;
        oss << std::hex << value;
        return oss.str();
    }

    // 生成随机 Token
    static std::string make_token() {
        static std::mt19937_64 rng(std::random_device{}());
        std::uniform_int_distribution<uint64_t> dist;
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (int i = 0; i < 4; ++i) {
            oss << std::setw(16) << dist(rng);
        }
        return oss.str();
    }

    // 提取 JSON 字符串字段
    static std::optional<std::string> extract_json_string(const std::string& json, const std::string& key) {
        const std::string pattern = "\"" + key + "\"";
        const size_t key_pos = json.find(pattern);
        if (key_pos == std::string::npos) {
            return std::nullopt;
        }
        size_t colon_pos = json.find(':', key_pos + pattern.size());
        if (colon_pos == std::string::npos) {
            return std::nullopt;
        }
        colon_pos++;
        while (colon_pos < json.size() && std::isspace(static_cast<unsigned char>(json[colon_pos]))) {
            colon_pos++;
        }
        if (colon_pos >= json.size() || json[colon_pos] != '"') {
            return std::nullopt;
        }
        colon_pos++;

        std::string value;
        bool escaped = false;
        for (size_t i = colon_pos; i < json.size(); ++i) {
            const char ch = json[i];
            if (escaped) {
                value.push_back(ch);
                escaped = false;
                continue;
            }
            if (ch == '\\') {
                escaped = true;
                continue;
            }
            if (ch == '"') {
                return value;
            }
            value.push_back(ch);
        }
        return std::nullopt;
    }
};