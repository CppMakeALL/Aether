cmake_minimum_required(VERSION 3.16.0)

project(Aether CXX)

################################################################################
# Set the C++ standard
################################################################################
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


################################################################################
# Config
################################################################################
add_compile_options(-mavx2 -O2) 

################################################################################
# Dependencies
################################################################################
find_package(Threads REQUIRED)

################################################################################
# Source groups
################################################################################
set(server_files
    src/KVShard.hpp
    src/KVShard.cpp
    src/KVEngine.hpp
    src/KVEngine.cpp
    src/Aether_Persist.hpp
    src/Aether_Persist.cpp
)
set(SERVER_FILES
    ${server_files}
)
################################################################################
# Target
################################################################################
add_executable(server ${SERVER_FILES} src/server.cpp)
target_include_directories(server PUBLIC ThirdParty/spdlog/include)

add_executable(client ${SERVER_FILES} src/client.cpp)

link_libraries(ibverbs rdmacm Threads::Threads)
# 服务端
add_executable(rdma_server src/rdma_server.cpp)

# 客户端
add_executable(rdma_client src/rdma_client.cpp)

#SIMD TEST
add_executable(SIMD_test test/SIMD_test.cpp)

#spdlog_test
add_executable(spdlog_test test/spdlog_test.cpp)
target_include_directories(spdlog_test PUBLIC ThirdParty/spdlog/include)

#cista_test
add_executable(cista_test test/cista_test.cpp)
target_include_directories(cista_test PUBLIC ThirdParty/cista/include)

#Aether_server
add_executable(Aether_server ${SERVER_FILES} src/Aether_server.cpp src/Aether_main.cpp)
target_include_directories(Aether_server PUBLIC ThirdParty/spdlog/include)

#Aether_client
add_library(Aether_client STATIC ${SERVER_FILES} src/Aether_client.cpp)
target_include_directories(Aether_client PUBLIC ThirdParty/spdlog/include)

#Aether_client_test
add_executable(Aether_client_test test/test_client.cpp)
target_include_directories(Aether_client_test PUBLIC ./src)
target_link_directories(Aether_client_test PUBLIC ./build)
target_link_libraries(Aether_client_test PUBLIC libAether_client.a)