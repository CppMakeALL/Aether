/*
    Copyright 2026 CppMakeALL. All Rights Reserved.
*/ 
#include <stdexcept>
#include "AE_Client.hpp"
#include <spdlog/spdlog.h>
namespace Aether {
    std::unique_ptr<ClientInterface> ClientFactory::create(TransMode mode)
    {
        if (mode == TransMode::TCP) {
            return std::make_unique<TCPClient>();
        } 
        else if (mode == TransMode::RDMA) {
            return std::make_unique<RDMAClient>();
        }
        return nullptr;
    }
    bool TCPClient::connect(const std::string& ip, int port)
    {
        sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_ == -1) {
            perror("socket");
            return false;
        }
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
            perror("inet_pton");
            close(sockfd_);
            return false;
        }

        if (::connect(sockfd_, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            perror("connect");
            close(sockfd_);
            return false;
        }
        return true;
    }

    std::string TCPClient::send_command(const std::string& command)
    {
        std::string cmd = command + "\r\n";
        if (send(sockfd_, cmd.c_str(), cmd.size(), 0) == -1) {
            perror("send");
            return "ERROR";
        }

        char buffer[4096] = {0};
        std::string response;
        int n;
        
        // 循环接收完整的响应
        while ((n = recv(sockfd_, buffer, sizeof(buffer) - 1, 0)) > 0) {
            response.append(buffer, n);
            // 检查是否收到完整的响应（以\r\n结尾）
            if (response.find("\r\n") != std::string::npos) {
                break;
            }
        }
        
        if (n == -1) {
            perror("recv");
            return "ERROR";
        }

        return response;
    }

    bool TCPClient::set(const std::string& key, const std::string& value)
    {
        std::string cmd = "SET " + key + " " + value;
        return send_command(cmd) == "OK";
    }

    std::string TCPClient::get(const std::string& key)
    {
        std::string cmd = "GET " + key;
        std::string response = send_command(cmd);
        if (response == "ERROR"){
            return "";
        }
        std::string value = response.substr(3);
        // 移除响应末尾的\r\n
        size_t pos = value.find("\r\n");
        if (pos != std::string::npos) {
            value = value.substr(0, pos);
        }
        return value;
    }

    bool TCPClient::disconnect()
    {
        if (sockfd_ == -1){
            spdlog::error("Aether_client::disconnect: client is not connected");
            return false;
        }
        close(sockfd_);
        sockfd_ = -1;
        return true;
    }
    //TODO: 实现RDMA连接
    bool RDMAClient::connect(const std::string& ip, int port)
    {
        return false;
    }
    bool RDMAClient::disconnect()
    {
        return false;
    }
    bool RDMAClient::set(const std::string& key, const std::string& value)
    {
        return false;
    }
    std::string RDMAClient::get(const std::string& key)
    {
        return "";
    }

    // 全局客户端实例
    static std::unique_ptr<ClientInterface> g_client;
    bool connect(const std::string& ip, int port, TransMode mode)
    {
        g_client = ClientFactory::create(mode);
        if (!g_client) return false;
        return g_client->connect(ip, port);
    }
    
    bool set(const std::string& key, const std::string& value)
    {
        if (!g_client){
            spdlog::error("Aether_client::set: client is not connected");
            return false;
        } 
        return g_client->set(key, value);
    }

    std::string get(const std::string& key)
    {
        if (!g_client){
            spdlog::error("Aether_client::get: client is not connected");
            return "";
        } 
        return g_client->get(key);
    }

    bool disconnect()
    {
        if (!g_client){
            spdlog::error("Aether_client::disconnect: client is not connected");
            return false;
        } 
        bool ret = g_client->disconnect();
        //g_client.reset();
        return ret;
    }
}