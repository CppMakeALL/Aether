/*
    Copyright 2026 CppMakeALL. All Rights Reserved.
*/ 
//工厂模式实现客户端连接类，同时实现tcp和rdma连接
#pragma once
#include "AE_Config.hpp"
#include <string>
#include <memory>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
namespace Aether {    
    //forward declaration
    class TCPClient;
    class RDMAClient;

    //统一客户端接口基类
    class ClientInterface
    {
    public:
        virtual ~ClientInterface() = default;
        virtual bool connect(const std::string& ip, int port) = 0;
        virtual bool disconnect() = 0;
        virtual bool set(const std::string& key, const std::string& value) = 0;
        virtual std::string get(const std::string& key) = 0;
    };
    //客户端工厂类
    //根据传输模式创建对应的客户端连接类
    //同时实现tcp和rdma连接
    //返回智能指针
    class ClientFactory
    {
    public:
        static std::unique_ptr<ClientInterface> create(TransMode mode);
    };

    class TCPClient : public ClientInterface
    {
    public:
        TCPClient() = default;
        ~TCPClient() = default;
        bool connect(const std::string& ip, int port) override;
        bool disconnect() override;
        bool set(const std::string& key, const std::string& value) override;
        std::string get(const std::string& key) override;
        std::string send_command(const std::string& command);
    private:
        int sockfd_;
    };

    class RDMAClient : public ClientInterface
    {
    public:
        RDMAClient() = default;
        ~RDMAClient() = default;
        bool connect(const std::string& ip, int port) override;
        bool disconnect() override;
        bool set(const std::string& key, const std::string& value) override;
        std::string get(const std::string& key) override;
    };

    //封装的全局函数声明
    bool connect(const std::string& ip, int port, TransMode mode);
    bool set(const std::string& key, const std::string& value);
    std::string get(const std::string& key);
    bool disconnect();
}
