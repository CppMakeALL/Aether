/*
    Copyright 2026 CppMakeALL. All Rights Reserved.
*/ 
#pragma once
#include "AE_KVShard.hpp"
#include <vector>
namespace Aether {
    // 单例，只负责：分片数、路由、创建shard
    class KVEngine {
        public:
            static KVEngine& instance() {
                return inst;
            }

            void set(const std::string& key, const std::string& value) {
                auto& shard = get_shard(key);  // 路由到对应分片
                shard.set(key, value);        // 只操作这个分片
            }

            std::optional<std::string> get(const std::string& key) {
                return get_shard(key).get(key);
            }

        private:
            KVEngine() {
                // 初始化 N 个分片
                shards_.resize(shard_count_);
            }
            KVEngine(const KVEngine&) = delete;
            KVEngine& operator=(const KVEngine&) = delete;
            
            // 路由：key → 分片下标
            KVShard& get_shard(const std::string& key) {
                size_t hash = std::hash<std::string>{}(key);
                return shards_[hash % shard_count_];
            }

            static constexpr size_t shard_count_ = 16; // 可配，一般=CPU核心数
            std::vector<KVShard> shards_;

            // 饿汉模式：静态实例
            static KVEngine inst;
    };
}
