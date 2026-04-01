/*
    Copyright 2026 CppMakeALL. All Rights Reserved.
*/ 
#pragma once
namespace Aether {
    constexpr int MAX_EVENTS = 1024;
    constexpr int BUFFER_SIZE = 4096;
    enum class TransMode
    {
        TCP,
        RDMA
    };
    // 客户端连接结构体
    struct Client {
        int fd;
        char buffer[BUFFER_SIZE];
        int buffer_len;
    };
}