cmake_minimum_required(VERSION 3.10)

project(AuthServer)

set(CMAKE_CXX_STANDARD 17)

include_directories(src)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(app ${SOURCES})

# === 新增的部分：如果是 Windows，链接 ws2_32 网络库 ===
if(WIN32)
    target_link_libraries(app ws2_32)
endif()