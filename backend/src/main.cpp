#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
/* 
version 1.0 @addtogroup Richard
Contact information ssrichard758@gmail.com
*/
namespace {

// 演示用的用户模型。
struct User {
  int id;
  std::string email;
  std::string password_hash;
  std::string name;
};

// 基于随机 token 的内存会话。
struct Session {
  int user_id;
  std::chrono::system_clock::time_point expires_at;
};

// 这个简易服务器使用的最小 HTTP 请求结构。
struct HttpRequest {
  std::string method;
  std::string path;
  std::unordered_map<std::string, std::string> headers;
  std::string body;
};

// 去掉字符串首尾空白字符。
std::string trim(const std::string& s) {
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

// 转小写，便于做不区分大小写的比较。
std::string to_lower(std::string s) {
  for (char& ch : s) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return s;
}

// 对字符串做 JSON 转义，避免响应 JSON 格式被破坏。
std::string json_escape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '\"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out.push_back(c);
        break;
    }
  }
  return out;
}

// 仅用于演示的密码哈希。
// 注意：生产环境应使用 bcrypt/argon2 + 每个用户独立盐值。
std::string weak_hash_password(const std::string& plain) {
  constexpr const char* kSalt = "elm-academy-salt-v1";
  std::hash<std::string> hasher;
  const auto value = hasher(std::string(kSalt) + plain);
  std::ostringstream oss;
  oss << std::hex << value;
  return oss.str();
}

// 生成十六进制随机会话 token。
std::string make_token() {
  static std::mt19937_64 rng(std::random_device{}());
  std::uniform_int_distribution<uint64_t> dist;
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (int i = 0; i < 4; ++i) {
    oss << std::setw(16) << dist(rng);
  }
  return oss.str();
}

// 简易 JSON 字符串字段提取器，适用于如下结构：
// {"email":"a@b.com","password":"123456"}
// 该实现刻意保持最小能力，不是完整 JSON 解析器。
std::optional<std::string> extract_json_string(const std::string& json, const std::string& key) {
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

class AuthServer {
 public:
  AuthServer() {
    // 预置一个演示账号，方便前端立即联调登录流程。
    User demo_user;
    demo_user.id = 1;
    demo_user.email = "student@campus.edu";
    demo_user.password_hash = weak_hash_password("123456");
    demo_user.name = "Demo Student";
    users_.push_back(demo_user);
  }

  // 启动阻塞式、单进程 TCP 服务。
  void run(uint16_t port) {
    const int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
      std::cerr << "socket() failed\n";
      return;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
      std::cerr << "bind() failed\n";
      ::close(server_fd);
      return;
    }

    if (::listen(server_fd, 16) < 0) {
      std::cerr << "listen() failed\n";
      ::close(server_fd);
      return;
    }

    std::cout << "Auth server listening on http://127.0.0.1:" << port << "\n";
    while (true) {
      const int client_fd = ::accept(server_fd, nullptr, nullptr);
      if (client_fd < 0) {
        continue;
      }
      handle_client(client_fd);
      ::close(client_fd);
    }
  }

 private:
  // 路由请求到各个认证接口。
  void handle_client(int client_fd) {
    auto req = read_request(client_fd);
    if (!req.has_value()) {
      write_response(client_fd, "400 Bad Request", "{\"error\":\"invalid request\"}");
      return;
    }

    cleanup_expired_sessions();

    if (req->method == "OPTIONS") {
      write_response(client_fd, "204 No Content", "", "application/json");
      return;
    }
    if (req->method == "GET" && req->path == "/api/health") {
      write_response(client_fd, "200 OK", "{\"ok\":true}");
      return;
    }
    if (req->method == "POST" && req->path == "/api/login") {
      handle_login(client_fd, *req);
      return;
    }
    if (req->method == "GET" && req->path == "/api/me") {
      handle_me(client_fd, *req);
      return;
    }
    if (req->method == "POST" && req->path == "/api/logout") {
      handle_logout(client_fd, *req);
      return;
    }

    write_response(client_fd, "404 Not Found", "{\"error\":\"not found\"}");
  }

  // 从 socket 读取并解析最小可用的 HTTP/1.1 请求。
  std::optional<HttpRequest> read_request(int client_fd) {
    std::string raw;
    char buffer[4096];
    ssize_t n = 0;
    while (raw.find("\r\n\r\n") == std::string::npos) {
      n = ::recv(client_fd, buffer, sizeof(buffer), 0);
      if (n <= 0) {
        return std::nullopt;
      }
      raw.append(buffer, buffer + n);
      if (raw.size() > 65536) {
        return std::nullopt;
      }
    }

    const size_t header_end = raw.find("\r\n\r\n");
    std::string header_part = raw.substr(0, header_end);
    std::string body_part = raw.substr(header_end + 4);

    std::istringstream hs(header_part);
    std::string first_line;
    if (!std::getline(hs, first_line)) {
      return std::nullopt;
    }
    if (!first_line.empty() && first_line.back() == '\r') {
      first_line.pop_back();
    }

    std::istringstream first_line_stream(first_line);
    HttpRequest req;
    std::string http_version;
    first_line_stream >> req.method >> req.path >> http_version;
    if (req.method.empty() || req.path.empty()) {
      return std::nullopt;
    }

    std::string line;
    while (std::getline(hs, line)) {
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      const size_t p = line.find(':');
      if (p == std::string::npos) {
        continue;
      }
      const std::string key = to_lower(trim(line.substr(0, p)));
      const std::string value = trim(line.substr(p + 1));
      req.headers[key] = value;
    }

    size_t content_length = 0;
    if (req.headers.count("content-length")) {
      try {
        content_length = static_cast<size_t>(std::stoul(req.headers["content-length"]));
      } catch (...) {
        return std::nullopt;
      }
    }

    while (body_part.size() < content_length) {
      n = ::recv(client_fd, buffer, sizeof(buffer), 0);
      if (n <= 0) {
        break;
      }
      body_part.append(buffer, buffer + n);
    }
    if (body_part.size() > content_length) {
      body_part.resize(content_length);
    }
    req.body = body_part;
    return req;
  }

  // 即使 send() 发生部分发送，也确保完整写出响应。
  bool send_all(int client_fd, const std::string& payload) {
    size_t sent = 0;
    while (sent < payload.size()) {
      const ssize_t n = ::send(client_fd, payload.data() + sent, payload.size() - sent, 0);
      if (n <= 0) {
        return false;
      }
      sent += static_cast<size_t>(n);
    }
    return true;
  }

  // 写回 JSON 响应，并附带基础 CORS 头。
  void write_response(int client_fd,
                      const std::string& status,
                      const std::string& body,
                      const std::string& content_type = "application/json; charset=utf-8") {
    std::ostringstream out;
    out << "HTTP/1.1 " << status << "\r\n";
    out << "Content-Type: " << content_type << "\r\n";
    out << "Content-Length: " << body.size() << "\r\n";
    out << "Connection: close\r\n";
    out << "Access-Control-Allow-Origin: *\r\n";
    out << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    out << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    out << "\r\n";
    out << body;
    const std::string payload = out.str();
    send_all(client_fd, payload);
  }

  // 在内存 users_ 中按邮箱查用户（不区分大小写）。
  std::optional<User> find_user_by_email(const std::string& email) const {
    for (const auto& user : users_) {
      if (to_lower(user.email) == to_lower(email)) {
        return user;
      }
    }
    return std::nullopt;
  }

  // 解析请求头中的 "Authorization: Bearer <token>"。
  std::optional<std::string> extract_bearer_token(const HttpRequest& req) const {
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

  // POST /api/login
  // 校验账号密码，成功后签发短期会话 token。
  void handle_login(int client_fd, const HttpRequest& req) {
    const auto email = extract_json_string(req.body, "email");
    const auto password = extract_json_string(req.body, "password");
    if (!email.has_value() || !password.has_value()) {
      write_response(client_fd, "400 Bad Request", "{\"error\":\"email or password missing\"}");
      return;
    }

    const auto user = find_user_by_email(*email);
    if (!user.has_value() || user->password_hash != weak_hash_password(*password)) {
      write_response(client_fd, "401 Unauthorized", "{\"error\":\"invalid credentials\"}");
      return;
    }

    const std::string token = make_token();
    Session session;
    session.user_id = user->id;
    session.expires_at = std::chrono::system_clock::now() + std::chrono::hours(4);
    sessions_[token] = session;

    std::ostringstream body;
    body << "{"
         << "\"token\":\"" << json_escape(token) << "\","
         << "\"expiresIn\":14400,"
         << "\"user\":{"
         << "\"id\":" << user->id << ","
         << "\"email\":\"" << json_escape(user->email) << "\","
         << "\"name\":\"" << json_escape(user->name) << "\""
         << "}"
         << "}";
    write_response(client_fd, "200 OK", body.str());
  }

  // GET /api/me
  // 校验 token 并返回当前登录用户信息。
  void handle_me(int client_fd, const HttpRequest& req) {
    const auto token = extract_bearer_token(req);
    if (!token.has_value() || !sessions_.count(*token)) {
      write_response(client_fd, "401 Unauthorized", "{\"error\":\"unauthorized\"}");
      return;
    }
    const Session& session = sessions_.at(*token);
    const auto now = std::chrono::system_clock::now();
    if (session.expires_at <= now) {
      sessions_.erase(*token);
      write_response(client_fd, "401 Unauthorized", "{\"error\":\"session expired\"}");
      return;
    }

    for (const auto& user : users_) {
      if (user.id == session.user_id) {
        std::ostringstream body;
        body << "{"
             << "\"id\":" << user.id << ","
             << "\"email\":\"" << json_escape(user.email) << "\","
             << "\"name\":\"" << json_escape(user.name) << "\""
             << "}";
        write_response(client_fd, "200 OK", body.str());
        return;
      }
    }
    write_response(client_fd, "401 Unauthorized", "{\"error\":\"user not found\"}");
  }

  // POST /api/logout
  // 从会话存储中移除 token，完成退出登录。
  void handle_logout(int client_fd, const HttpRequest& req) {
    const auto token = extract_bearer_token(req);
    if (token.has_value()) {
      sessions_.erase(*token);
    }
    write_response(client_fd, "200 OK", "{\"ok\":true}");
  }

  // 维护逻辑：每次请求时清理过期会话。
  void cleanup_expired_sessions() {
    const auto now = std::chrono::system_clock::now();
    for (auto it = sessions_.begin(); it != sessions_.end();) {
      if (it->second.expires_at <= now) {
        it = sessions_.erase(it);
      } else {
        ++it;
      }
    }
  }

  std::vector<User> users_;
  std::unordered_map<std::string, Session> sessions_;
};

}  // namespace

int main() {
  AuthServer server;
  server.run(8080);
  return 0;
}
