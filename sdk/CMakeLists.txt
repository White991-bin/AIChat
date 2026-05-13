# CMake 最低版本要求
cmake_minimum_required(VERSION 3.10)

# 设置 SDK 的名称
set(SDK_NAME "ai_chat_sdk")

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型
set(CMAKE_BUILD_TYPE Release)

# 收集源文件
file(GLOB_RECURSE SDK_SOURCES "src/*.cpp")
file(GLOB_RECURSE SDK_HEADERS "include/*.h")

# 生成静态库
add_library(${SDK_NAME} STATIC ${SDK_SOURCES} ${SDK_HEADERS})

# 设置输出目录
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/build)

# 定义 httplib 中的 CPPHTTPLIB_OPENSSL_SUPPORT 宏
target_compile_definitions(${SDK_NAME} PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)

# 设置头文件包含目录
target_include_directories(${SDK_NAME} PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 设置静态库的目录
link_directories(/usr/local/lib)

# 查找 OpenSSL 库
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# 添加 CPPHTTPLIB_OPENSSL_SUPPORT 定义
target_compile_definitions(${SDK_NAME} PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

# 链接库
target_link_libraries(${SDK_NAME} 
    jsoncpp 
    fmt  
    spdlog 
    sqlite3 
    OpenSSL::SSL 
    OpenSSL::Crypto
) 

# 安装规则：拷贝静态库到系统目录 /usr/local/lib
install(TARGETS ${SDK_NAME}
    ARCHIVE DESTINATION lib
)

# 安装规则：拷贝头文件到系统目录 /usr/local/include/ai_chat_sdk
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include/ai_chat_sdk
    FILES_MATCHING PATTERN "*.h"
)

