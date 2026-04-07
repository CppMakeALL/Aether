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
    src/AE_KVShard.hpp
    src/AE_KVShard.cpp
    src/AE_KVEngine.hpp
    src/AE_KVEngine.cpp
    src/AE_Persist.hpp
    src/AE_Persist.cpp
    src/AE_Evict.hpp
    src/AE_Evict.cpp
    src/AE_Observer.cpp
)
set(SERVER_FILES
    ${server_files}
)
################################################################################
# Target
################################################################################

include_directories(ThirdParty/spdlog/include)

link_libraries(ibverbs rdmacm Threads::Threads)
# 服务端
add_executable(rdma_server test/rdma_server.cpp)

# 客户端
add_executable(rdma_client test/rdma_client.cpp)

#SIMD TEST
add_executable(SIMD_test test/SIMD_test.cpp)

#spdlog_test
add_executable(spdlog_test test/spdlog_test.cpp)
target_include_directories(spdlog_test PUBLIC ThirdParty/spdlog/include)

#cista_test
add_executable(cista_test test/cista_test.cpp)
target_include_directories(cista_test PUBLIC ThirdParty/cista/include)

#Aether_server
add_executable(Aether_server ${SERVER_FILES} src/AE_Server.cpp src/AE_main.cpp)
target_include_directories(Aether_server PUBLIC ThirdParty/spdlog/include)

#Aether_client
add_library(Aether_client STATIC ${SERVER_FILES} src/AE_Client.cpp)
target_include_directories(Aether_client PUBLIC ThirdParty/spdlog/include)

#Aether_client_test
add_executable(Aether_client_test test/test_client.cpp)
target_include_directories(Aether_client_test PUBLIC ./src)
target_link_directories(Aether_client_test PUBLIC ./build)
target_link_libraries(Aether_client_test PUBLIC Aether_client)