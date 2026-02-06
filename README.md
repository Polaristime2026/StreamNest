# C++ 登录后端 + 前端示例

最小可运行的认证后端，用标准 C++17 手写 TCP/HTTP 服务器，提供登录 / 退出 / 获取当前用户等接口；`views/` 里附带一个 Vue 3 登录页示例，方便联调。

## 目录结构
- `backend/src/main.cpp` — 单文件实现的阻塞式 TCP 服务器与认证逻辑
- `backend/CMakeLists.txt` — CMake 构建配置
- `views/login.html` / `views/login.css` — 纯前端登录页（Vue 3 CDN 版）

## 运行环境
- CMake ≥ 3.16
- 任意支持 C++17 的编译器（g++、clang++ 均可）
- macOS / Linux（使用 POSIX socket API）

## 构建与启动
```bash
cd backend
cmake -S . -B build
cmake --build build
./build/edu_auth_server
```
默认监听：`http://127.0.0.1:8080`

## 内置测试账号
- 邮箱：`student@campus.edu`
- 密码：`123456`
- 会话时长：4 小时（在 `sessions_` 中维护，过期即失效）

## API 一览
| 方法 | 路径 | 说明 | 备注 |
| --- | --- | --- | --- |
| GET  | `/api/health` | 健康检查 | 返回 `{"ok":true}` |
| POST | `/api/login`  | 邮箱+密码登录 | 成功返回 token 与用户信息 |
| GET  | `/api/me`     | 获取当前用户 | 需 `Authorization: Bearer <token>` |
| POST | `/api/logout` | 退出登录 | 删除内存中的会话 |

### 请求/响应示例
```bash
# 登录
curl -X POST http://127.0.0.1:8080/api/login \
  -H "Content-Type: application/json" \
  -d '{"email":"student@campus.edu","password":"123456"}'

# 带 token 获取当前用户
curl http://127.0.0.1:8080/api/me \
  -H "Authorization: Bearer <token>"

# 退出登录
curl -X POST http://127.0.0.1:8080/api/logout \
  -H "Authorization: Bearer <token>"
```

### `/api/login` 返回体
```json
{
  "token": "<hex_token>",
  "expiresIn": 14400,
  "user": {
    "id": 1,
    "email": "student@campus.edu",
    "name": "Demo Student"
  }
}
```

## 前端示例联调
1) 启动后端后，直接在浏览器打开 `views/login.html`（本地文件即可）。
2) 在表单里填入测试账号，使用浏览器控制台 `fetch` 或替换脚本来调用后端接口。
3) 若跨域：后端默认返回 `Access-Control-Allow-Origin: *`，无需额外配置。

## 开发提示
- 服务器为单进程阻塞模型，适合教学/演示，不适合生产。
- 密码哈希使用演示版 `weak_hash_password`，实际项目请替换为 bcrypt/argon2 + 唯一盐。
- 会话存储在内存 `sessions_` 中，服务重启即失效。

## 常见问题
- “启动报错：Address already in use” → 端口 8080 被占用，修改 `main.cpp` 中的端口或释放占用。
- “跨域失败” → 确认请求是 HTTP 而非 `file://` 受限环境；必要时改用本地静态服务器打开 `views/`。

## 许可证
未声明许可证；在合规前请仅用于内部演示或学习。
