#include <iostream>
#include <memory>
#include "repository/UserRepository.h"
#include "service/AuthService.h"
#include "controller/AuthController.h"
#include "server/HttpServer.h"

int main() {
    // 1. 实例化 Repository (相当于 @Repository)
    auto userRepo = std::make_shared<UserRepository>();

    // 2. 实例化 Service，并注入 Repository (相当于 @Service + @Autowired)
    auto authService = std::make_shared<AuthService>(userRepo);

    // 3. 实例化 Controller，并注入 Service (相当于 @RestController + @Autowired)
    auto authController = std::make_shared<AuthController>(authService);

    // 4. 定义请求处理的回调函数 (连接 Server 和 Controller)
    auto router = [authController](int client_fd, const HttpRequest& req) {
        authController->handleRequest(client_fd, req);
    };

    // 5. 启动服务器
    HttpServer::run(8080, router);

    return 0;
}