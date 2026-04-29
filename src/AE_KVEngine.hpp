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
            static void init(int shard_count, size_t max_memory) {
                if(inst) return;
                inst = new KVEngine(shard_count, max_memory);
            }
            static KVEngine& instance() {
                if(!inst) {
                    spdlog::error("KVEngine not initialized");
                    exit(1);
                }
                return *inst;
            }

            void set(const std::string& key, const std::string& value) {
                auto& shard = get_shard(key);  // 路由到对应分片
                shard.set(key, value);        // 只操作这个分片
            }

            std::optional<std::string> get(const std::string& key) {
                return get_shard(key).get(key);
            }

            std::optional<std::string> get_nosimd(const std::string& key) {
                return get_shard(key).get_nosimd(key);
            }

            bool exist(const std::string& key) {
                return get_shard(key).exist(key);
            }

        protected:
            // 饿汉模式：静态实例
            static KVEngine* inst;

        private:
            KVEngine(int shard_count, size_t max_memory) : max_memory_(max_memory), shard_count_(shard_count) {
                // 初始化 N 个分片
                shards_.reserve(shard_count_);
                for (size_t i = 0; i < shard_count_; ++i) {
                    shards_.emplace_back(max_memory_);
                }
            }
            KVEngine(const KVEngine&) = delete;
            KVEngine& operator=(const KVEngine&) = delete;
            
            // 路由：key → 分片下标
            KVShard& get_shard(const std::string& key) {
                size_t hash = std::hash<std::string>{}(key);
                return shards_[hash % shard_count_];
            }

            size_t shard_count_; // 可配，一般=CPU核心数
            std::vector<KVShard> shards_;
            size_t max_memory_;
    };
}
