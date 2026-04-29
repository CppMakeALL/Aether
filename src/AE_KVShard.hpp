/*
    Copyright 2026 CppMakeALL. All Rights Reserved.
*/ 
#pragma once
#include <atomic>
#include <string>
#include <optional>
#include <cstdint>
#include <spdlog/spdlog.h>
#include "AE_Observer.hpp"

namespace Aether {
    // 开放寻址哈希表的槽位状态
    enum class SlotStatus : uint8_t {
        EMPTY = 0,      // 空槽位
        OCCUPIED = 1,   // 已占用
        DELETED = 2     // 已删除（惰性删除标记）
    };

    // 开放寻址哈希表的槽位
    struct HashSlot {
        std::atomic<SlotStatus> status{SlotStatus::EMPTY};
        uint64_t hash;  
        std::string key;
        std::string value;
    };

    // 无锁开放寻址哈希表
    class LockFreeHashTable {
    public:
        LockFreeHashTable() : capacity_(0), buckets_(nullptr), max_memory_(0), used_memory_(0), strategy_(nullptr) {}

        LockFreeHashTable(size_t capacity, size_t max_memory)
            : capacity_(capacity), 
              buckets_(new HashSlot[capacity]),
              max_memory_(max_memory),
              used_memory_(0) {
            metadata_manager_ = std::make_shared<MetadataManager>();
            register_observer(metadata_manager_);
        }

        // 移动构造函数
        LockFreeHashTable(LockFreeHashTable&& other) noexcept
            : capacity_(other.capacity_), 
              buckets_(other.buckets_),
              strategy_(std::move(other.strategy_)),
              metadata_manager_(std::move(other.metadata_manager_)),
              observers_(std::move(other.observers_)),
              max_memory_(other.max_memory_),
              used_memory_(other.used_memory_.load(std::memory_order_relaxed)) {
            other.capacity_ = 0;
            other.buckets_ = nullptr;
            other.max_memory_ = 0;
            other.used_memory_.store(0, std::memory_order_relaxed);
        }

        // 移动赋值运算符
        LockFreeHashTable& operator=(LockFreeHashTable&& other) noexcept {
            if (this != &other) {
                // 清理当前资源
                if (buckets_) {
                    delete[] buckets_;
                }
                
                capacity_ = other.capacity_;
                buckets_ = other.buckets_;
                strategy_ = std::move(other.strategy_);
                metadata_manager_ = std::move(other.metadata_manager_);
                observers_ = std::move(other.observers_);
                max_memory_ = other.max_memory_;
                used_memory_.store(other.used_memory_.load(std::memory_order_relaxed), std::memory_order_relaxed);
                
                other.capacity_ = 0;
                other.buckets_ = nullptr;
                other.max_memory_ = 0;
                other.used_memory_.store(0, std::memory_order_relaxed);
            }
            return *this;
        }

        ~LockFreeHashTable() {
            if (buckets_) {
                delete[] buckets_;
            }
        }

        void register_observer(std::shared_ptr<KVObserver> observer) {
            std::lock_guard<std::mutex> lock(mtx_);
            observers_.push_back(observer);
        }

        void set_evict_strategy(std::unique_ptr<EvictStrategy> strategy) {
            strategy_ = std::move(strategy);
        }

        bool exist(const std::string& key);

        void evict_key();

        void set(const std::string& key, const std::string& value);

        std::optional<std::string> get(const std::string& key);

        std::optional<std::string> get_nosimd(const std::string& key);

        std::shared_ptr<MetadataManager> get_metadata_manager() {
            return metadata_manager_;
        }

    private:
        void notify_all(const std::string& key, DBEvent event) {
            std::lock_guard<std::mutex> lock(mtx_);
            for (auto& obs : observers_) {
                obs->on_event(key, event);
            }
        }

        size_t hash(const std::string& key) {
            uint64_t h = std::hash<std::string>{}(key);
            // 简单的哈希函数
            h = ((h >> 32) ^ h) * 0x45d9f3b;
            h = ((h >> 32) ^ h) * 0x45d9f3b;
            h = (h >> 32) ^ h;
            return h % capacity_;
        }

        size_t capacity_;
        HashSlot* buckets_;
        std::unique_ptr<EvictStrategy> strategy_;
        std::shared_ptr<MetadataManager> metadata_manager_;
        std::mutex mtx_;
        std::vector<std::shared_ptr<KVObserver>> observers_;
        size_t max_memory_;
        std::atomic<size_t> used_memory_;
    };

    // KV分片
    class KVShard {
    public:
        KVShard(size_t max_memory) : hash_table_(1024, max_memory) {
            hash_table_.set_evict_strategy(std::make_unique<FIFOStrategy>());
        }
        // 移动构造函数
        KVShard(KVShard&& other) noexcept 
            : hash_table_(std::move(other.hash_table_)) {}

        // 移动赋值运算符
        KVShard& operator=(KVShard&& other) noexcept {
            if (this != &other) {
                hash_table_ = std::move(other.hash_table_);
            }
            return *this;
        }

        void set(const std::string& key, const std::string& value) {
            hash_table_.set(key, value);
        }

        std::optional<std::string> get(const std::string& key) {
            return hash_table_.get(key);
        }

        std::optional<std::string> get_nosimd(const std::string& key) {
            return hash_table_.get_nosimd(key);
        }

        bool exist(const std::string& key) {
            return hash_table_.exist(key);
        }

    private:
        LockFreeHashTable hash_table_;
    };
}