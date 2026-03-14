cmake_minimum_required(VERSION 3.16.0)

project(Aether CXX)

################################################################################
# Set the C++ standard
################################################################################
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


################################################################################
# Dependencies
################################################################################
find_package(Threads REQUIRED)

################################################################################
# Source groups
################################################################################
#set(source_files
#    "framework.h"
#   "PyKSLib.cpp"
#    "Util/DataConversion.h"
#    "Util/Utility_Export.h"
#)
#set(ALL_FILES
#    ${source_files}
#)
################################################################################
# Target
################################################################################

link_libraries(ibverbs rdmacm Threads::Threads)
# 服务端
add_executable(aether_server rdma_server.cpp)

# 客户端
add_executable(aether_client rdma_client.cpp)