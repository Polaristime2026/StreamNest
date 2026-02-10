# 📚 C++ 认证服务器代码详解

> 一份针对 C++ 初学者的完整代码解析
> 
> **文件**: `backend/src/main.cpp`  
> **功能**: 基于 TCP Socket 的迷你 HTTP 认证服务器  
> **语言**: C++17

---

## 🎯 程序概述

这是一个**从零开始用 C++ 编写的轻量级 Web 认证服务器**，无需依赖任何数据库或 Web 框架。

### 核心功能
- ✅ **用户登录** → 验证邮箱和密码，返回 Token
- ✅ **获取用户信息** → 使用 Token 查询当前用户
- ✅ **登出** → 清除会话
- ✅ **健康检查** → 验证服务器是否正常运行

### 技术栈
- **网络编程**: POSIX Socket (Linux/WSL)
- **并发模型**: 阻塞式单线程 (演示用，生产环境需改进)
- **HTTP协议**: 手写 HTTP/1.1 parser
- **会话管理**: 内存存储 (进程结束数据丢失)

---

## 📋 代码结构分析

### 第一部分：头文件引入

```cpp
#include <arpa/inet.h>        // IP 地址和网络字节序转换
#include <netinet/in.h>       // 网络协议定义（sockaddr_in）
#include <sys/socket.h>       // Socket 编程接口
#include <unistd.h>           // close() 等系统调用

#include <chrono>             // 时间库（计算会话过期时间）
#include <string>             // 字符串类
#include <unordered_map>      // HashMap，存储会话数据
#include <vector>             // 向量，存储用户列表
// ... 其他标准库
```

**初学者解释**:
- `#include` 是引入库文件
- `<arpa/inet.h>` 等是**系统库**，用于网络编程
- `<string>` 等是**标准库**，用于数据结构

---

### 第二部分：数据结构定义

#### 1️⃣ User 结构体 - 表示一个用户

```cpp
struct User {
  int id;                    // 用户 ID
  std::string email;         // 邮箱
  std::string password_hash; // 密码哈希值（不能存明文！）
  std::string name;          // 用户名
};
```

**为什么存 `password_hash` 而不是密码本身?**
- 安全考虑：即使黑客获得数据库，也拿不到明文密码
- 登录时对输入的密码进行同样的哈希运算，比较哈希值即可

#### 2️⃣ Session 结构体 - 表示一个会话（登录状态）

```cpp
struct Session {
  int user_id;         // 这个会话属于哪个用户
  std::chrono::system_clock::time_point expires_at;  // 会话何时过期
};
```

**工作原理**:
1. 用户登录成功 → 服务器创建一个 Session
2. 返回 Token（会话的唯一标识）给前端
3. 前端后续请求时，在 Header 中带上 Token
4. 服务器读取 Token → 查表找到对应的 Session → 获知是哪个用户 → 验证是否过期

#### 3️⃣ HttpRequest 结构体 - 表示一个 HTTP 请求

```cpp
struct HttpRequest {
  std::string method;                                  // "GET" 或 "POST"
  std::string path;                                    // "/api/login"
  std::unordered_map<std::string, std::string> headers; // 请求头键值对
  std::string body;                                    // 请求体（如 JSON）
};
```

**HTTP 请求例子**:
```
POST /api/login HTTP/1.1
Host: 127.0.0.1:8080
Content-Type: application/json
Content-Length: 45

{"email":"student@campus.edu","password":"123456"}
```
- **方法**：POST
- **路径**：/api/login
- **头**：Host, Content-Type, Content-Length
- **体**：{"email":"...","password":"..."}

---

### 第三部分：工具函数

#### 🔧 trim() - 去掉字符串首尾空白

```cpp
std::string trim(const std::string& s) {
  size_t begin = 0;
  // 跳过开头的空白字符（空格、tab、换行等）
  while (begin < s.size() && std::isspace(s[begin])) {
    ++begin;
  }
  
  size_t end = s.size();
  // 跳过结尾的空白字符
  while (end > begin && std::isspace(s[end - 1])) {
    --end;
  }
  
  // 返回中间的有效部分
  return s.substr(begin, end - begin);
}
```

**示例**:
- 输入：`"   hello world   "`
- 输出：`"hello world"`

#### 🔧 to_lower() - 转小写（用于邮箱比对）

```cpp
std::string to_lower(std::string s) {
  for (char& ch : s) {
    ch = std::tolower(ch);  // 逐个字符转小写
  }
  return s;
}
```

**为什么需要？** 邮箱 `Student@Campus.Edu` 和 `student@campus.edu` 应该视为同一个账号

#### 🔧 json_escape() - JSON 转义

```cpp
std::string json_escape(const std::string& s) {
  std::string out;
  for (char c : s) {
    switch (c) {
      case '\\': out += "\\\\"; break;  // \ 变成 \\
      case '\"': out += "\\\""; break;  // " 变成 \"
      case '\n': out += "\\n"; break;   // 换行符变成 \n
      // ..
      default: out.push_back(c);
    }
  }
  return out;
}
```

**为什么需要？** 如果用户名包含 `"` 或 `\`，直接放入 JSON 会破坏格式：
- ❌ 不转义：`{"name":"John"s house"}` (JSON 格式错误)
- ✅ 转义：`{"name":"John\"s house"}` (JSON 格式正确)

#### 🔧 weak_hash_password() - 密码哈希

```cpp
std::string weak_hash_password(const std::string& plain) {
  constexpr const char* kSalt = "elm-academy-salt-v1";  // 盐值
  std::hash<std::string> hasher;
  
  // 密码 + 盐值 一起哈希，防止彩虹表攻击
  const auto value = hasher(std::string(kSalt) + plain);
  
  // 转十六进制字符串
  std::ostringstream oss;
  oss << std::hex << value;
  return oss.str();
}
```

**流程**:
1. `plain = "123456"`
2. `salt + plain = "elm-academy-salt-v1123456"`
3. 哈希运算 → `0x1a2b3c4d5e6f...`
4. 转换为字符串返回

#### 🔧 make_token() - 生成随机会话 Token

```cpp
std::string make_token() {
  static std::mt19937_64 rng(std::random_device{}());  // 随机数生成器
  std::uniform_int_distribution<uint64_t> dist;
  
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (int i = 0; i < 4; ++i) {
    oss << std::setw(16) << dist(rng);  // 生成 4 个 64 位随机数
  }
  return oss.str();
}
```

**结果例子**: `a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6`

#### 🔧 extract_json_string() - JSON 字段提取器

```cpp
std::optional<std::string> extract_json_string(const std::string& json, 
                                               const std::string& key) {
  // 查找 "key": 的位置
  const std::string pattern = "\"" + key + "\"";
  size_t key_pos = json.find(pattern);
  if (key_pos == std::string::npos) {
    return std::nullopt;  // 未找到，返回空
  }
  
  // 找到冒号后的引号
  size_t colon_pos = json.find(':', key_pos);
  colon_pos++;
  while (json[colon_pos] == ' ' || json[colon_pos] == '\t') {
    colon_pos++;  // 跳过空格
  }
  
  // 读取引号内的值
  colon_pos++;  // 跳过开启的 "
  std::string value;
  for (size_t i = colon_pos; i < json.size(); ++i) {
    if (json[i] == '\"') {
      return value;  // 遇到闭合的 "，返回
    }
    value.push_back(json[i]);
  }
  return std::nullopt;
}
```

**示例**:
```
json = {"email":"abc@def.com","password":"123456"}
extract_json_string(json, "email") → "abc@def.com"
extract_json_string(json, "password") → "123456"
```

---

### 第四部分：AuthServer 类 - 核心服务器逻辑

#### 📡 构造函数 - 初始化演示用户

```cpp
AuthServer() {
  User demo_user;
  demo_user.id = 1;
  demo_user.email = "student@campus.edu";
  demo_user.password_hash = weak_hash_password("123456");
  demo_user.name = "Demo Student";
  users_.push_back(demo_user);
}
```

这样做的目的是**不用数据库，直接在代码里预置一个测试账号**。

可以用以下凭据登录：
- 📧 邮箱：`student@campus.edu`
- 🔑 密码：`123456`

#### 🚀 run() - 启动服务器

```cpp
void run(uint16_t port) {
  // 1️⃣ 创建 Socket
  const int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  // AF_INET: IPv4    SOCK_STREAM: TCP
  
  // 2️⃣ 允许地址即时重用（防止 "Address already in use" 错误）
  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  
  // 3️⃣ 设置监听地址和端口
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;  // 监听所有网卡
  addr.sin_port = htons(port);        // 转换为网络字节序
  
  // 4️⃣ 绑定地址
  if (::bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
    std::cerr << "bind() failed\n";
    return;
  }
  
  // 5️⃣ 开始监听，最多同时接待 16 个排队连接
  if (::listen(server_fd, 16) < 0) {
    std::cerr << "listen() failed\n";
    return;
  }
  
  // 6️⃣ 无限循环接受客户端连接
  std::cout << "Auth server listening on http://127.0.0.1:" << port << "\n";
  while (true) {
    const int client_fd = ::accept(server_fd, nullptr, nullptr);
    if (client_fd < 0) continue;
    
    handle_client(client_fd);     // 处理这个客户端的请求
    ::close(client_fd);           // 断开连接
  }
}
```

**工作流程图**:
```
[客户端 1] ──┐
[客户端 2] ──┤──→ accept() ──→ handle_client() ──→ 回复响应
[客户端 3] ──┘
```

#### 🛣️ handle_client() - 路由请求

```cpp
void handle_client(int client_fd) {
  // 1️⃣ 从 Socket 读取 HTTP 请求
  auto req = read_request(client_fd);
  if (!req.has_value()) {
    write_response(client_fd, "400 Bad Request", "{\"error\":\"invalid request\"}");
    return;
  }
  
  cleanup_expired_sessions();  // 清理过期会话
  
  // 2️⃣ 根据 HTTP 方法和路径路由到不同处理函数
  if (req->method == "OPTIONS") {
    // CORS 预检请求
    write_response(client_fd, "204 No Content", "");
  } 
  else if (req->method == "GET" && req->path == "/api/health") {
    write_response(client_fd, "200 OK", "{\"ok\":true}");
  } 
  else if (req->method == "POST" && req->path == "/api/login") {
    handle_login(client_fd, *req);
  } 
  else if (req->method == "GET" && req->path == "/api/me") {
    handle_me(client_fd, *req);
  } 
  else if (req->method == "POST" && req->path == "/api/logout") {
    handle_logout(client_fd, *req);
  } 
  else {
    write_response(client_fd, "404 Not Found", "{\"error\":\"not found\"}");
  }
}
```

**路由表**:
| 方法 | 路径 | 功能 |
|------|------|------|
| GET | /api/health | 健康检查 |
| POST | /api/login | 用户登录 |
| GET | /api/me | 获取当前用户 |
| POST | /api/logout | 用户登出 |

#### 📖 read_request() - 解析 HTTP 请求

```cpp
std::optional<HttpRequest> read_request(int client_fd) {
  std::string raw;
  char buffer[4096];
  
  // 不断从 Socket 读数据，直到看到 "\r\n\r\n"（HTTP 头和体的分隔符）
  while (raw.find("\r\n\r\n") == std::string::npos) {
    ssize_t n = ::recv(client_fd, buffer, sizeof(buffer), 0);
    if (n <= 0) return std::nullopt;  // 连接断开或出错
    raw.append(buffer, buffer + n);
    if (raw.size() > 65536) return std::nullopt;  // 防止缓冲区溢出
  }
  
  // 分割头部和体
  size_t header_end = raw.find("\r\n\r\n");
  std::string header_part = raw.substr(0, header_end);
  std::string body_part = raw.substr(header_end + 4);
  
  // 解析请求行（第一行）
  std::istringstream hs(header_part);
  std::string first_line;
  std::getline(hs, first_line);
  
  HttpRequest req;
  std::string http_version;
  std::istringstream fls(first_line);
  fls >> req.method >> req.path >> http_version;
  // first_line = "POST /api/login HTTP/1.1"
  // → method = "POST", path = "/api/login", http_version = "HTTP/1.1"
  
  // 解析请求头（后续行）
  std::string line;
  while (std::getline(hs, line)) {
    size_t p = line.find(':');
    if (p == std::string::npos) continue;
    std::string key = to_lower(trim(line.substr(0, p)));
    std::string value = trim(line.substr(p + 1));
    req.headers[key] = value;
    // "Content-Type: application/json" → headers["content-type"] = "application/json"
  }
  
  // 读取 Content-Length 指定的请求体
  size_t content_length = 0;
  if (req.headers.count("content-length")) {
    content_length = std::stoul(req.headers["content-length"]);
  }
  while (body_part.size() < content_length) {
    // 继续读数据直到够够
    ssize_t n = ::recv(client_fd, buffer, sizeof(buffer), 0);
    if (n <= 0) break;
    body_part.append(buffer, buffer + n);
  }
  
  req.body = body_part;
  return req;
}
```

**解析例子**:
```
原始 HTTP：
POST /api/login HTTP/1.1\r\n
Host: 127.0.0.1:8080\r\n
Content-Length: 45\r\n
\r\n
{"email":"student@campus.edu","password":"123456"}

解析结果：
req.method = "POST"
req.path = "/api/login"
req.headers["host"] = "127.0.0.1:8080"
req.headers["content-length"] = "45"
req.body = {"email":"student@campus.edu","password":"123456"}
```

#### ✍️ write_response() - 发送 HTTP 响应

```cpp
void write_response(int client_fd,
                    const std::string& status,
                    const std::string& body,
                    const std::string& content_type = "application/json; charset=utf-8") {
  std::ostringstream out;
  out << "HTTP/1.1 " << status << "\r\n";
  out << "Content-Type: " << content_type << "\r\n";
  out << "Content-Length: " << body.size() << "\r\n";
  out << "Connection: close\r\n";
  out << "Access-Control-Allow-Origin: *\r\n";  // CORS
  out << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
  out << "\r\n";
  out << body;
  
  send_all(client_fd, out.str());
}
```

**生成的响应例子**:
```
HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8
Content-Length: 123
Connection: close
Access-Control-Allow-Origin: *

{"token":"a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6","expiresIn":14400,...}
```

#### 🔐 handle_login() - 处理登录请求

```cpp
void handle_login(int client_fd, const HttpRequest& req) {
  // 1️⃣ 从请求体 JSON 中提取邮箱和密码
  const auto email = extract_json_string(req.body, "email");
  const auto password = extract_json_string(req.body, "password");
  if (!email.has_value() || !password.has_value()) {
    write_response(client_fd, "400 Bad Request", 
                   "{\"error\":\"email or password missing\"}");
    return;
  }
  
  // 2️⃣ 在内存中查找该邮箱的用户
  const auto user = find_user_by_email(*email);
  if (!user.has_value()) {
    write_response(client_fd, "401 Unauthorized", 
                   "{\"error\":\"invalid credentials\"}");
    return;
  }
  
  // 3️⃣ 验证密码（对输入的密码进行哈希，比较是否相等）
  if (user->password_hash != weak_hash_password(*password)) {
    write_response(client_fd, "401 Unauthorized", 
                   "{\"error\":\"invalid credentials\"}");
    return;
  }
  
  // 4️⃣ 密码正确，生成 Token 并创建会话
  const std::string token = make_token();
  Session session;
  session.user_id = user->id;
  session.expires_at = std::chrono::system_clock::now() + std::chrono::hours(4);
  // 会话有效期为 4 小时
  
  sessions_[token] = session;  // 存入 sessions_ map
  
  // 5️⃣ 返回 Token 和用户信息
  std::ostringstream body;
  body << "{"
       << "\"token\":\"" << json_escape(token) << "\","
       << "\"expiresIn\":14400,"  // 14400 秒 = 4 小时
       << "\"user\":{"
       << "\"id\":" << user->id << ","
       << "\"email\":\"" << json_escape(user->email) << "\","
       << "\"name\":\"" << json_escape(user->name) << "\""
       << "}"
       << "}";
  write_response(client_fd, "200 OK", body.str());
}
```

**登录流程图**:
```
前端请求:
{
  "email": "student@campus.edu",
  "password": "123456"
}
        ↓
服务器验证:
  1. 找到邮箱对应用户?
  2. 密码 hash 后是否匹配?
        ↓
生成响应:
{
  "token": "a1b2c3d4...",
  "expiresIn": 14400,
  "user": {
    "id": 1,
    "email": "student@campus.edu",
    "name": "Demo Student"
  }
}
```

#### 👤 handle_me() - 获取当前用户信息

```cpp
void handle_me(int client_fd, const HttpRequest& req) {
  // 1️⃣ 从 Authorization 头提取 Token
  const auto token = extract_bearer_token(req);
  // 格式: "Authorization: Bearer a1b2c3d4e5f6..."
  
  if (!token.has_value() || !sessions_.count(*token)) {
    write_response(client_fd, "401 Unauthorized", 
                   "{\"error\":\"unauthorized\"}");
    return;
  }
  
  // 2️⃣ 检查会话是否过期
  const Session& session = sessions_.at(*token);
  const auto now = std::chrono::system_clock::now();
  if (session.expires_at <= now) {
    sessions_.erase(*token);  // 删除过期的会话
    write_response(client_fd, "401 Unauthorized", 
                   "{\"error\":\"session expired\"}");
    return;
  }
  
  // 3️⃣ 根据 user_id 找用户信息
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
  
  write_response(client_fd, "401 Unauthorized", 
                 "{\"error\":\"user not found\"}");
}
```

**使用流程**:
```
前端请求头:
Authorization: Bearer a1b2c3d4e5f6...

服务器:
  1. 从 token 找到对应 session
  2. 检查 session 是否过期
  3. 从 session 的 user_id 找用户
  4. 返回用户信息

响应:
{
  "id": 1,
  "email": "student@campus.edu",
  "name": "Demo Student"
}
```

#### 🚪 handle_logout() - 登出

```cpp
void handle_logout(int client_fd, const HttpRequest& req) {
  // 1️⃣ 从请求头提取 Token
  const auto token = extract_bearer_token(req);
  
  // 2️⃣ 如果 Token 存在，从会话表中删除
  if (token.has_value()) {
    sessions_.erase(*token);
  }
  
  // 3️⃣ 总是返回成功（即使 Token 无效）
  write_response(client_fd, "200 OK", "{\"ok\":true}");
}
```

**为什么总是返回成功？** 用户点击"退出登录"后，即使 Token 已经过期或不存在，也应该返回成功，这是一个前端友好的设计。

---

### 第五部分：main() 函数

```cpp
int main() {
  AuthServer server;      // 创建服务器对象
  server.run(8080);       // 在端口 8080 开始监听
  return 0;
}
```

就这么简单！当你运行这个程序，它会一直阻塞在 `server.run(8080)`，不断接受和处理客户端请求。

---

## 🔄 完整请求流程示例

### 场景：用户登录 → 查询个人信息 → 登出

#### ① 登录请求

```bash
curl -X POST http://127.0.0.1:8080/api/login \
  -H "Content-Type: application/json" \
  -d '{"email":"student@campus.edu","password":"123456"}'
```

**服务器处理**:
1. Socket 接收请求 → `read_request()`
2. 解析出 method="POST", path="/api/login", body=`{"email":"...","password":"..."}`
3. 路由到 `handle_login()`
4. 提取邮箱和密码 → 在 `users_` 向量中查找
5. 验证密码哈希 → 生成随机 token
6. 创建 Session 对象，存入 `sessions_` map
7. 组装 JSON 响应，发送回客户端

**响应**:
```json
{
  "token": "3a5b7c9d1e2f4g6h8i0j2k4l6m8n0o2p",
  "expiresIn": 14400,
  "user": {
    "id": 1,
    "email": "student@campus.edu",
    "name": "Demo Student"
  }
}
```

#### ② 获取用户信息请求

```bash
curl http://127.0.0.1:8080/api/me \
  -H "Authorization: Bearer 3a5b7c9d1e2f4g6h8i0j2k4l6m8n0o2p"
```

**服务器处理**:
1. Socket 接收请求 → `read_request()`
2. 解析出 method="GET", path="/api/me", headers 中有 "authorization"
3. 路由到 `handle_me()`
4. 从 Authorization 头提取 token
5. 在 `sessions_` 中查找该 token
6. 检查会话是否过期
7. 根据 session 中的 user_id 在 `users_` 中找用户
8. 返回用户信息

**响应**:
```json
{
  "id": 1,
  "email": "student@campus.edu",
  "name": "Demo Student"
}
```

#### ③ 登出请求

```bash
curl -X POST http://127.0.0.1:8080/api/logout \
  -H "Authorization: Bearer 3a5b7c9d1e2f4g6h8i0j2k4l6m8n0o2p"
```

**服务器处理**:
1. Socket 接收请求 → `read_request()`
2. 路由到 `handle_logout()`
3. 从 Authorization 头提取 token
4. 从 `sessions_` 删除该 token
5. 返回成功响应

**响应**:
```json
{"ok": true}
```

---

## 📍 关键数据结构内存模型

```cpp
AuthServer 对象内存布局：

┌──────────────────────────────────────┐
│  std::vector<User> users_            │
│  ├─ [0]: id=1, email="student@...", │
│  │        password_hash="0x123abc"   │
│  │        name="Demo Student"        │
│  └─ ...                              │
│                                      │
│  std::unordered_map<            │
│    std::string,    /* token */  │
│    Session         /* session */     │
│  > sessions_                        │
│  ├─ "3a5b7c9d..." → {user_id: 1,  │
│  │                   expires_at:   │
│  │                   2026/2/8     │
│  │                   16:00}        │
│  └─ "9x8y7z6w..." → {...}         │
└──────────────────────────────────────┘
```

---

## ⚠️ 初学者常见问题

### Q1: 为什么要用 `std::optional`?

```cpp
std::optional<std::string> email = extract_json_string(req.body, "email");
if (!email.has_value()) {  // 检查是否存在
  // 字段不存在
}
```

**答**: `std::optional` 表示"可能有值，也可能没有"。比传统的"返回 nullptr"更安全，更能表达意图。

### Q2: 什么是 `unordered_map`?

```cpp
std::unordered_map<std::string, Session> sessions_;
sessions_["token123"] = session;  // 存储
Session s = sessions_["token123"];  // 查询
```

**答**: 它是一个哈希表（字典），支持 **O(1)** 的查找速度。当你有大量 Token 要查询时，比 Vector 遍历快得多。

### Q3: 为什么要 `std::chrono`?

```cpp
auto expires_at = std::chrono::system_clock::now() + std::chrono::hours(4);
```

**答**: 这是 C++ 标准库提供的**类型安全的时间库**。避免了手动计算秒数而出错。

### Q4: 这个服务器能用于生产吗?

**答**: **不能**。原因：
- ❌ 单线程：一个慢客户端会阻塞全部请求
- ❌ 内存存储：重启后数据丢失
- ❌ 弱密钥哈希：使用 bcrypt/argon2
- ❌ 无日志：无法调试问题
- ❌ 无 HTTPS：明文传输

### Q5: 怎样扩展这个服务器?

**可以尝试**:
1. 添加新的 API 端点（如修改密码）
2. 集成真实数据库（SQLite/MySQL）
3. 实现多线程处理（用 `std::thread`）
4. 添加日志记录
5. 加入数据验证（邮箱格式、密码强度）

---

## 📚 推荐学习路径

| 话题 | 需要理解 | 推荐资源 |
|------|---------|---------|
| Socket 网络编程 | `socket()`, `bind()`, `listen()`, `accept()` | Linux Man Pages |
| HTTP 协议 | 请求/响应格式 | MDN Web Docs |
| 哈希表 | `unordered_map` 的性能特性 | C++ Reference |
| 字符串处理 | `substr()`, `find()`, `append()` | C++ String Tutorial |
| 异常处理 | try-catch (未在本项目使用) | C++ Exception Handling |

---

## 🎓 总结

这个 C++ 认证服务器展示了：
- ✅ Socket 编程基础
- ✅ HTTP 协议的手工解析
- ✅ 会话管理的设计思想
- ✅ JSON 处理（最小化实现）
- ✅ 数据结构的实际应用

虽然代码很短，但它涵盖了**真实 Web 服务的核心概念**！🚀
