
# CMake 最低版本要求
cmake_minimum_required(VERSION 3.10)

# 项目名称
project(LLMTest)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型为 Debug
set(CMAKE_BUILD_TYPE Debug)

# 设置输出目录
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/build)

# 添加头文件搜索路径
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/../sdk/include
    ${PROJECT_SOURCE_DIR}/../sdk/include/util
    /usr/include/jsoncpp
)

# 查找 OpenSSL 包
find_package(OpenSSL REQUIRED)

# 包含 OpenSSL 头文件路径
include_directories(${OPENSSL_INCLUDE_DIR})

# 设置库的目录
link_directories(/usr/local/lib)

# 添加可执行文件（修正 mylog.cpp 路径）
add_executable(LLMTest 
    testLLMProvider.cpp
    ../sdk/src/DeepSeekProvider.cpp
    ../sdk/src/GeminiProvider.cpp
    ../sdk/src/OllamaLLMProvider.cpp
    ../sdk/src/mylog.cpp
    ../sdk/src/ChatSdk.cpp
     ../sdk/src/LLMManager.cpp        
    ../sdk/src/SessionManager.cpp    
    ../sdk/src/DataManager.cpp   
)

# 添加编译定义
target_compile_definitions(LLMTest PUBLIC
    CPPHTTPLIB_OPENSSL_SUPPORT
    ENABLE_DEEPSEEK
    ENABLE_GEMINI
)

# 链接库
target_link_libraries(LLMTest 
    fmt 
    jsoncpp 
    OpenSSL::SSL 
    OpenSSL::Crypto 
    spdlog 
    gtest
    pthread
    sqlite3 
)