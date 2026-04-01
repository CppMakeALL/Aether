/*
    Copyright 2026 CppMakeALL. All Rights Reserved.
*/ 
#include "Aether_server.hpp"
#include "Aether_config.hpp"
#include <csignal>
#include <spdlog/spdlog.h>
#include <memory>
//信号处理函数（signal handler）不能访问 main 里的局部变量，只能访问全局变量
static std::unique_ptr<Aether::ServerInterface> g_server;

void signal_handler(int sig) {
    spdlog::info("Received signal: {}, stopping server...", sig);
    if (g_server) {
        g_server->stop();
    }
}

int main()
{
    //注册信号处理函数
    // signal(SIGINT, signal_handler);
    // signal(SIGTERM, signal_handler);

    try{
        g_server = Aether::ServerFactory::create(Aether::TransMode::TCP);

        g_server->start("192.168.20.184", 8001);

    }
    catch(const std::exception& e){
        spdlog::error("Aether Server Start Exception: {}", e.what());
    }
    return 0;
}