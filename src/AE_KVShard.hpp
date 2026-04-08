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

        bool exist(const std::string& key) {
            size_t index = hash(key);
            size_t original_index = index;
            size_t probe_count = 0;

            while (true) {
                auto status = buckets_[index].status.load(std::memory_order_acquire);
                
                if (status == SlotStatus::EMPTY) {
                    return false;
                } else if (status == SlotStatus::OCCUPIED && buckets_[index].key == key) {
                    return true;
                }

                // 线性探测
                index = (index + 1) % capacity_;
                probe_count++;
                
                // 探测完整个表
                if (probe_count >= capacity_) {
                    return false;
                }
                
                // 回到起点，说明表已满
                if (index == original_index) {
                    return false;
                }
            }
        }

        void evict_key() {
            if (!strategy_) {
                spdlog::error("Evict strategy not set");
                return;
            }
            auto& entries = metadata_manager_->get_all_entries();
            auto key = strategy_->evict(entries);
            if(!key.empty()) {
                // 从哈希表中删除键值对
                size_t index = hash(key);
                size_t original_index = index;
                size_t probe_count = 0;

                while (true) {
                    auto status = buckets_[index].status.load(std::memory_order_acquire);
                    
                    if (status == SlotStatus::EMPTY) {
                        return; // 键不存在
                    } else if (status == SlotStatus::OCCUPIED && buckets_[index].key == key) {
                        // 计算释放的内存（只计算动态分配的部分）
                        size_t memory_freed = buckets_[index].key.size() + buckets_[index].value.size();
                        // 标记为已删除
                        buckets_[index].status.store(SlotStatus::DELETED, std::memory_order_release);
                        // 减少内存使用
                        used_memory_.fetch_sub(memory_freed, std::memory_order_relaxed);
                        // 从MetadataManager中删除对应的entry
                        metadata_manager_->remove_entry(key);
                        spdlog::info("Evicted key: {}", key);
                        return;
                    }

                    // 线性探测
                    index = (index + 1) % capacity_;
                    probe_count++;
                    
                    // 探测完整个表
                    if (probe_count >= capacity_) {
                        return;
                    }
                    
                    // 回到起点，说明表已满
                    if (index == original_index) {
                        return;
                    }
                }
            }
        }

        void set(const std::string& key, const std::string& value) {
            size_t index = hash(key);
            size_t original_index = index;
            size_t probe_count = 0;
            size_t first_deleted_index = capacity_; // 记录第一个已删除的槽位

            while (true) {
                auto status = buckets_[index].status.load(std::memory_order_acquire);
                
                if (status == SlotStatus::EMPTY) {
                    // 找到空槽位，尝试插入
                    if (first_deleted_index != capacity_) {
                        // 使用之前找到的已删除槽位
                        index = first_deleted_index;
                    }
                    
                    // 计算内存使用（只计算动态分配的部分）
                    size_t memory_needed = key.size() + value.size();
                    size_t current_used = used_memory_.load(std::memory_order_relaxed);
                    size_t new_used = current_used + memory_needed;
                    
                    spdlog::info("Current used memory: {}, needed: {}, new: {}, max: {}", 
                        current_used, memory_needed, new_used, max_memory_);
                    
                    // 检查内存是否超过限制
                    if (new_used > max_memory_) {
                        // 内存不足，尝试驱逐
                        spdlog::info("Memory allocation failed for key {}, try to evict", key);
                        evict_key();
                        // 重新尝试插入
                        return set(key, value);
                    }
                    
                    if (buckets_[index].status.compare_exchange_weak(
                        status, SlotStatus::OCCUPIED, 
                        std::memory_order_release, 
                        std::memory_order_relaxed)) {
                        // 插入成功，更新键值和内存使用
                        buckets_[index].key = key;
                        buckets_[index].value = value;
                        used_memory_.fetch_add(memory_needed, std::memory_order_relaxed);
                        metadata_manager_->add_entry(key, value);
                        notify_all(key, DBEvent::INSERT);
                        spdlog::info("Inserted key {}, memory used now: {}", key, current_used + memory_needed);
                        return;
                    }
                    // 插入失败，重新探测
                    status = buckets_[index].status.load(std::memory_order_acquire);
                }
                
                if (status == SlotStatus::OCCUPIED) {
                    if (buckets_[index].key == key) {
                        // 键已存在，更新值
                        buckets_[index].value = value;
                        return;
                    }
                } else if (status == SlotStatus::DELETED) {
                    // 记录第一个已删除的槽位
                    if (first_deleted_index == capacity_) {
                        first_deleted_index = index;
                    }
                }

                // 线性探测
                index = (index + 1) % capacity_;
                probe_count++;
                
                // 探测完整个表
                if (probe_count >= capacity_) {
                    // 表已满，尝试驱逐
                    spdlog::info("Hash table full for key {}, try to evict", key);
                    evict_key();
                    // 重新尝试插入
                    return set(key, value);
                }
                
                // 回到起点，说明表已满
                if (index == original_index) {
                    // 表已满，尝试驱逐
                    spdlog::info("Hash table full for key {}, try to evict", key);
                    evict_key();
                    // 重新尝试插入
                    return set(key, value);
                }
            }
        }

        std::optional<std::string> get(const std::string& key) {
            size_t index = hash(key);
            size_t original_index = index;
            size_t probe_count = 0;

            while (true) {
                auto status = buckets_[index].status.load(std::memory_order_acquire);
                
                if (status == SlotStatus::EMPTY) {
                    return std::nullopt;
                } else if (status == SlotStatus::OCCUPIED && buckets_[index].key == key) {
                    notify_all(key, DBEvent::GET);
                    return buckets_[index].value;
                }

                // 线性探测
                index = (index + 1) % capacity_;
                probe_count++;
                
                // 探测完整个表
                if (probe_count >= capacity_) {
                    return std::nullopt;
                }
                
                // 回到起点，说明表已满
                if (index == original_index) {
                    return std::nullopt;
                }
            }
        }

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

    private:
        LockFreeHashTable hash_table_;
    };
}