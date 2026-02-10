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



# C++ Spring Boot Style Auth Server

这是一个基于 C++ 17 编写的简易 HTTP 认证服务器。它采用了类似 **Java Spring Boot** 的经典分层架构（Controller - Service - Repository），旨在帮助习惯 Java 开发模式的同学快速上手 C++ 服务端开发。

---

## 📂 项目结构

项目采用了标准的 MVC 分层结构，各模块职责分明：

```text
backend/
├── CMakeLists.txt          # 项目构建文件 (类似 Maven pom.xml)
├── build/                  # 编译输出目录 (自动生成)
└── src/
    ├── main.cpp            # 程序入口 (Application.java)
    ├── model/              # 实体类 (Entity/DTO)
    │   ├── User.h          # 用户数据模型
    │   ├── Session.h       # 会话模型
    │   └── HttpRequest.h   # HTTP 请求对象
    ├── repository/         # 数据访问层 (DAO/Repository)
    │   └── UserRepository  # 负责数据的增删改查
    ├── service/            # 业务逻辑层 (Service)
    │   └── AuthService     # 处理登录、Token 生成等业务
    ├── controller/         # 接口层 (Controller)
    │   └── AuthController  # 解析 HTTP 请求，调用 Service
    ├── server/             # 核心网络层 (Infrastructure)
    │   └── HttpServer      # 封装 Socket 通信 (类似 Tomcat)
    └── util/               # 工具类
        └── AppUtils        # 字符串处理、哈希加密等工具