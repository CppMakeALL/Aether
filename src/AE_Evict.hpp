/*
    Copyright 2026 CppMakeALL. All Rights Reserved.
*/ 
#pragma once
#include <string>
#include <unordered_map>
namespace Aether {
    struct KVEntry {
        std::string key_;
        std::string value_;

        // 用于不同策略的元数据
        uint64_t create_time_{0};   // FIFO
        uint64_t visit_time_{0};    // LRU
        uint32_t visit_count_{0};   // LFU
    };
    class EvictStrategy {
        public:
            virtual ~EvictStrategy() = default;

            // 传入所有候选 key，返回要被淘汰的 key
            // 无合适可返回空，表示无法淘汰
            virtual std::string evict(const std::unordered_map<std::string, KVEntry>& entries) = 0;

            // 策略名称（调试/日志用）
            virtual std::string name() const = 0;
    };

    class FIFOStrategy : public EvictStrategy {
    public:
        std::string evict(const std::unordered_map<std::string, KVEntry>& entries) override;

        std::string name() const override { return "FIFO"; }
    };

    class LRUStrategy : public EvictStrategy {
    public:
        std::string evict(const std::unordered_map<std::string, KVEntry>& entries) override;

        std::string name() const override { return "LRU"; }
    };

    class LFUtrategy : public EvictStrategy {
    public:
        std::string evict(const std::unordered_map<std::string, KVEntry>& entries) override;

        std::string name() const override { return "LFU"; }
    };

}