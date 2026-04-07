/*
    Copyright 2026 CppMakeALL. All Rights Reserved.
*/ 
#include "AE_Server.hpp"
#include "AE_Config.hpp"
#include <csignal>
#include <spdlog/spdlog.h>
#include <memory>
#include <yaml-cpp/yaml.h>
//信号处理函数（signal handler）不能访问 main 里的局部变量，只能访问全局变量
static std::unique_ptr<Aether::ServerInterface> g_server;

void signal_handler(int sig) {
    spdlog::info("Received signal: {}, stopping server...", sig);
    if (g_server) {
        g_server->stop();
    }
}

struct KVConfig {
    std::string ip;
    int port;
    Aether::TransMode trans_mode;
};

KVConfig load_config(const std::string& config_path = "../Config.yml"){
    KVConfig cfg;
    try {
        YAML::Node config = YAML::LoadFile(config_path);

        // 读取参数
        cfg.ip = config["ip"].as<std::string>();
        cfg.port = config["port"].as<int>();
        auto tmp_trans_mode = config["trans_mode"].as<std::string>();
        if (tmp_trans_mode == "TCP") {
            cfg.trans_mode = Aether::TransMode::TCP;
        } else if (tmp_trans_mode == "RDMA") {
            cfg.trans_mode = Aether::TransMode::RDMA;
        } else {
            spdlog::error("Invalid trans_mode: {}", tmp_trans_mode);
            exit(1);
        }

    } catch (const YAML::Exception& e) {
        spdlog::error("Config fileload failed: {}", e.what());
        exit(1);
    }
    return cfg;
}

int main()
{
    //注册信号处理函数
    // signal(SIGINT, signal_handler);
    // signal(SIGTERM, signal_handler);
    KVConfig cfg = load_config();

    try{
        g_server = Aether::ServerFactory::create(cfg.trans_mode);

        g_server->start(cfg.ip, cfg.port);

    }
    catch(const std::exception& e){
        spdlog::error("Aether Server Start Exception: {}", e.what());
    }
    return 0;
}