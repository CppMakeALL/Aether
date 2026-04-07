/*
    Copyright 2026 CppMakeALL. All Rights Reserved.
*/ 
#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include "AE_Evict.hpp"
#include <spdlog/spdlog.h>
namespace Aether {  
    // 数据库事件类型
    enum class DBEvent {
        INSERT,  // 插入
        GET      // 查询
    };

    // 观察者接口（监听 KV 变化，自动更新元数据）
    class KVObserver {
    public:
        virtual ~KVObserver() = default;
        // 事件触发：key + 事件类型
        virtual void on_event(const std::string& key, DBEvent event) = 0;
    };

    // 元数据管理器（实现观察者）
    class MetadataManager : public KVObserver {
    public:
        // 插入/查询事件触发时自动更新元数据
        void on_event(const std::string& key, DBEvent event) override {
            std::lock_guard<std::mutex> lock(mtx_);

            // 如果 entry 不存在，直接返回
            if (entries_.find(key) == entries_.end()) return;

            auto& entry = entries_[key];
            auto now = get_current_time();

            if (event == DBEvent::INSERT) {
                entry.create_time_ = now;    // 插入时设置创建时间
                entry.visit_time_ = now;     // 初始化访问时间
                entry.visit_count_ = 1;      // 初始化访问次数
                // spdlog::info("INSERT: {}", key);
                // spdlog::info("INSERT: {}", entry.create_time_);
                // spdlog::info("INSERT: {}", entry.visit_time_);
                // spdlog::info("INSERT: {}", entry.visit_count_);
            } 
            else if (event == DBEvent::GET) {
                entry.visit_time_ = now;     // 每次查询更新访问时间
                entry.visit_count_++;        // 每次查询访问次数 +1
                // spdlog::info("GET: {}", key);
                // spdlog::info("GET: {}", entry.visit_time_);
                // spdlog::info("GET: {}", entry.visit_count_);
            }
        }

        // 外部添加 entry（插入KV时调用）
        void add_entry(const std::string& key, const std::string& value) {
            std::lock_guard<std::mutex> lock(mtx_);
            entries_[key] = {key, value, 0, 0, 0};
        }

        // 删除 entry
        void remove_entry(const std::string& key) {
            std::lock_guard<std::mutex> lock(mtx_);
            entries_.erase(key);
        }

        // 获取所有 entry（给淘汰策略用）
        std::unordered_map<std::string, KVEntry>& get_all_entries() {
            return entries_;
        }

    private:
        // 获取当前时间戳（毫秒）
        uint64_t get_current_time() {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
        }

        std::unordered_map<std::string, KVEntry> entries_;
        std::mutex mtx_;
    };
}