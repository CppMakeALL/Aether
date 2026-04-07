/*
    Copyright 2026 CppMakeALL. All Rights Reserved.
*/ 
#pragma once
#include <string>
#include <memory>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include "AE_Config.hpp"
namespace Aether {
    //统一服务端接口基类
    class ServerInterface
    {
    public:
        ServerInterface() = default;
        virtual ~ServerInterface() = default;
        virtual bool start(const std::string& ip, int port) = 0;
        virtual bool stop() = 0;
        void show_logo();
    private:
        std::string ip_;
        int port_;
    };

    class ServerFactory
    {
    public:
        static std::unique_ptr<ServerInterface> create(TransMode mode);
    };
    
    class TCPServer : public ServerInterface
    {
    public:
        TCPServer() = default;
        virtual ~TCPServer() = default;
        virtual bool start(const std::string& ip, int port) override;
        virtual bool stop() override;
        void set_nonblocking(int fd);
        void event_loop();
        void handle_client_request(Client* client);
    private:
        std::string ip_;
        int port_;
        int listen_fd_;
        int epoll_fd_;
        struct epoll_event event_;
        struct epoll_event events_[MAX_EVENTS];
    };
    
    class RDMAServer : public ServerInterface
    {
    public:
        RDMAServer() = default;
        virtual ~RDMAServer() = default;
        virtual bool start(const std::string& ip, int port) override;
        virtual bool stop() override;
    };

}
