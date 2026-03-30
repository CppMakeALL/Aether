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
)
set(SERVER_FILES
    ${server_files}
)
################################################################################
# Target
################################################################################
add_executable(server ${SERVER_FILES} src/server.cpp)

add_executable(client ${SERVER_FILES} src/client.cpp)

link_libraries(ibverbs rdmacm Threads::Threads)
# 服务端
add_executable(rdma_server src/rdma_server.cpp)

# 客户端
add_executable(rdma_client src/rdma_client.cpp)

#SIMD TEST
add_executable(SIMD_test test/SIMD_test.cpp)