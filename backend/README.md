# C++ 登录后端（最小可运行版）

## 功能

- `POST /api/login`：邮箱+密码登录，返回 token
- `GET /api/me`：携带 `Authorization: Bearer <token>` 获取当前用户
- `POST /api/logout`：退出登录，清除会话
- `GET /api/health`：健康检查

内置测试账号：

- 邮箱：`student@campus.edu`
- 密码：`123456`

## 构建和运行

```bash
cd backend
cmake -S . -B build
cmake --build build
./build/edu_auth_server
```

服务默认监听：`http://127.0.0.1:8080`

## 接口测试

```bash
curl -X POST http://127.0.0.1:8080/api/login \
  -H "Content-Type: application/json" \
  -d '{"email":"student@campus.edu","password":"123456"}'
```

将返回中的 `token` 放到：

```bash
curl http://127.0.0.1:8080/api/me \
  -H "Authorization: Bearer <token>"
```

