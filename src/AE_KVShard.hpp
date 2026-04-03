/*
    Copyright 2026 CppMakeALL. All Rights Reserved.
*/ 
#pragma once
#include <atomic>
#include <string>
#include <optional>
#include <cstdint>
#include <spdlog/spdlog.h>
#include "AE_Evict.hpp"
//TODO SIMD加速set和get运算
namespace Aether {
    // 无锁内存池
    class LockFreeMemoryPool {
    public:
        struct MemoryBlock {
            MemoryBlock* next;
            char data[0]; // 柔性数组
        };

        LockFreeMemoryPool() : block_size_(0), max_memory_(0), used_memory_(0) {}

        LockFreeMemoryPool(size_t block_size, size_t initial_blocks = 1024, size_t max_mem = 1024) 
            : block_size_(block_size + sizeof(MemoryBlock)), max_memory_(max_mem), used_memory_(0) {
            // 预分配初始内存块
            for (size_t i = 0; i < initial_blocks; ++i) {
                auto* block = static_cast<MemoryBlock*>(::malloc(block_size_));
                if (!block) break;
                
                // 检查是否超过最大内存
                size_t new_used = used_memory_.load(std::memory_order_relaxed) + block_size_;
                if (new_used > max_memory_) {
                    ::free(block);
                    break;
                }
                
                // 更新已用内存
                used_memory_.fetch_add(block_size_, std::memory_order_relaxed);
                
                block->next = free_list_.load(std::memory_order_relaxed);
                while (!free_list_.compare_exchange_weak(
                    block->next, block, 
                    std::memory_order_release, 
                    std::memory_order_relaxed));
            }
        }

        // 移动构造函数
        LockFreeMemoryPool(LockFreeMemoryPool&& other) noexcept
            : block_size_(other.block_size_), 
              max_memory_(other.max_memory_),
              used_memory_(other.used_memory_.load(std::memory_order_relaxed)),
              free_list_(other.free_list_.load(std::memory_order_relaxed)) {
            other.block_size_ = 0;
            other.max_memory_ = 0;
            other.used_memory_.store(0, std::memory_order_relaxed);
            other.free_list_.store(nullptr, std::memory_order_relaxed);
        }

        // 移动赋值运算符
        LockFreeMemoryPool& operator=(LockFreeMemoryPool&& other) noexcept {
            if (this != &other) {
                block_size_ = other.block_size_;
                max_memory_ = other.max_memory_;
                used_memory_.store(other.used_memory_.load(std::memory_order_relaxed), 
                                 std::memory_order_relaxed);
                free_list_.store(other.free_list_.load(std::memory_order_relaxed), 
                                 std::memory_order_relaxed);
                other.block_size_ = 0;
                other.max_memory_ = 0;
                other.used_memory_.store(0, std::memory_order_relaxed);
                other.free_list_.store(nullptr, std::memory_order_relaxed);
            }
            return *this;
        }

        ~LockFreeMemoryPool() {
            // 释放所有内存块
            auto* block = free_list_.load(std::memory_order_relaxed);
            while (block) {
                auto* next = block->next;
                ::free(block);
                block = next;
            }
        }

        void* allocate() {
            auto* block = free_list_.load(std::memory_order_relaxed);
            while (block) {
                if (free_list_.compare_exchange_weak(
                    block, block->next, 
                    std::memory_order_acquire, 
                    std::memory_order_relaxed)) {
                    return block->data;
                }
                block = free_list_.load(std::memory_order_relaxed);
            }
            
            // 如果没有空闲块，分配新的
            // 检查是否超过最大内存
            size_t new_used = used_memory_.load(std::memory_order_relaxed) + block_size_;
            if (new_used > max_memory_) {
                return nullptr; // 超过最大内存，拒绝分配
            }
            
            // 尝试分配新内存
            auto* new_block = static_cast<MemoryBlock*>(::malloc(block_size_));
            if (new_block) {
                // 更新已用内存
                used_memory_.fetch_add(block_size_, std::memory_order_relaxed);
                return new_block->data;
            }
            
            return nullptr;
        }

        void deallocate(void* ptr) {
            if (!ptr) return;
            auto* block = reinterpret_cast<MemoryBlock*>(
                static_cast<char*>(ptr) - sizeof(MemoryBlock));
            
            // 更新已用内存
            used_memory_.fetch_sub(block_size_, std::memory_order_relaxed);
            
            block->next = free_list_.load(std::memory_order_relaxed);
            while (!free_list_.compare_exchange_weak(
                block->next, block, 
                std::memory_order_release, 
                std::memory_order_relaxed));
        }

        // 获取已用内存
        size_t used_memory() const {
            return used_memory_.load(std::memory_order_relaxed);
        }

        // 获取最大内存
        size_t max_memory() const {
            return max_memory_;
        }

    private:
        size_t block_size_;
        size_t max_memory_; // 最大内存限制
        std::atomic<size_t> used_memory_; // 已用内存
        std::atomic<MemoryBlock*> free_list_{nullptr};
    };

    // 无锁哈希表节点
    struct HashNode {
        //重大bug:key是引用类型传入的如果是临时变量,会导致悬空引用
        //const std::string& key;
        std::string key;
        std::string value;
        std::atomic<HashNode*> next{nullptr}; // 下一个节点的指针，用原子操作确保线程安全

        HashNode(const std::string& k, const std::string& v) 
            : key(k), value(v), next(nullptr) {}
    };

    // 无锁哈希表
    class LockFreeHashTable {
    public:
        LockFreeHashTable() : capacity_(0), buckets_(nullptr) {}

        LockFreeHashTable(size_t capacity)
            : capacity_(capacity), 
              buckets_(new std::atomic<HashNode*>[capacity]),
              memory_pool_(sizeof(HashNode) + 256) { // 假设value最大256字节
            for (size_t i = 0; i < capacity; ++i) {
                buckets_[i].store(nullptr, std::memory_order_relaxed);
            }
        }

        // 移动构造函数
        LockFreeHashTable(LockFreeHashTable&& other) noexcept
            : capacity_(other.capacity_), 
              buckets_(other.buckets_),
              memory_pool_(std::move(other.memory_pool_)) {
            other.capacity_ = 0;
            other.buckets_ = nullptr;
        }

        // 移动赋值运算符
        LockFreeHashTable& operator=(LockFreeHashTable&& other) noexcept {
            if (this != &other) {
                // 清理当前资源
                if (buckets_) {
                    for (size_t i = 0; i < capacity_; ++i) {
                        auto* node = buckets_[i].load(std::memory_order_relaxed);
                        while (node) {
                            auto* next = node->next.load(std::memory_order_acquire);
                            memory_pool_.deallocate(node);
                            node = next;
                        }
                    }
                    delete[] buckets_;
                }
                
                capacity_ = other.capacity_;
                buckets_ = other.buckets_;
                memory_pool_ = std::move(other.memory_pool_);
                other.capacity_ = 0;
                other.buckets_ = nullptr;
            }
            return *this;
        }

        ~LockFreeHashTable() {
            if (!buckets_) return;
            for (size_t i = 0; i < capacity_; ++i) {
                auto* node = buckets_[i].load(std::memory_order_relaxed);
                while (node) {
                    auto* next = node->next.load(std::memory_order_acquire);
                    memory_pool_.deallocate(node);
                    node = next;
                }
            }
            delete[] buckets_;
        }

        void set_evict_strategy(std::unique_ptr<EvictStrategy> strategy) {
            strategy_ = std::move(strategy);
        }

        bool exist(const std::string& key) {
            //AVX2/AVX512 加速哈希计算
            size_t index = hash(key);
            auto* node = buckets_[index].load(std::memory_order_acquire);
            while (node) {
                if (node->key == key) {
                    return true;
                }
                //必须用load方法读取next指针，确保读取到最新的节点地址，防止多线程并发修改
                node = node->next.load(std::memory_order_acquire);
            }
            return false;
        }
        // void evict_key() {
        //     auto key = strategy_->evict();
        //     if(key){
        //         // 从哈希表和内存池中删除键值对
        //         size_t index = hash(*key);
        //         auto* head = buckets_[index].load(std::memory_order_relaxed);
                
        //         // 遍历链表查找要删除的节点
        //         HashNode* prev = nullptr;
        //         HashNode* curr = head;
                
        //         while (curr) {
        //             if (curr->key == *key) {
        //                 // 找到要删除的节点
        //                 if (prev) {
        //                     // 不是头节点，更新前一个节点的next指针
        //                     while (!prev->next.compare_exchange_weak(
        //                         curr, curr->next.load(std::memory_order_acquire),
        //                         std::memory_order_release,
        //                         std::memory_order_relaxed)) {
        //                         // 如果失败，重新获取当前节点的next指针
        //                         curr = prev->next.load(std::memory_order_acquire);
        //                         if (!curr || curr->key != *key) {
        //                             // 节点可能已经被其他线程删除或修改
        //                             return;
        //                         }
        //                     }
        //                 } else {
        //                     // 是头节点，更新桶的头指针
        //                     while (!buckets_[index].compare_exchange_weak(
        //                         head, curr->next.load(std::memory_order_acquire),
        //                         std::memory_order_release,
        //                         std::memory_order_relaxed)) {
        //                         // 如果失败，重新获取头指针
        //                         head = buckets_[index].load(std::memory_order_relaxed);
        //                         if (head != curr) {
        //                             // 头节点可能已经被其他线程修改
        //                             return;
        //                         }
        //                     }
        //                 }
                        
        //                 // 释放节点内存
        //                 memory_pool_.deallocate(curr);
        //                 spdlog::info("Evicted key: {}", *key);
        //                 return;
        //             }
                    
        //             prev = curr;
        //             curr = curr->next.load(std::memory_order_acquire);
        //         }
        //     }
        // }

        void set(const std::string& key, const std::string& value) {
            //AVX2/AVX512 加速哈希计算
            size_t index = hash(key);
            
            // 先检查键是否存在，如果存在则更新值
            auto* node = buckets_[index].load(std::memory_order_acquire);
            while (node) {
                if (node->key == key) {
                    node->value = value;
                    return;
                }
                node = node->next.load(std::memory_order_acquire);
            }
            
            // 键不存在，创建新节点
            void* memory = memory_pool_.allocate();
            if (!memory) {
                // 内存分配失败，超过最大内存限制
                spdlog::error("Memory allocation failed, max memory limit reached");
                return;
            }
            
            HashNode* new_node = new (memory) HashNode(key, value);
            
            auto* head = buckets_[index].load(std::memory_order_relaxed);
            while (true) {
                new_node->next = head;
                if (buckets_[index].compare_exchange_weak(
                    head, new_node, 
                    std::memory_order_release, 
                    std::memory_order_relaxed)) {
                    break;
                }
            }
        }

        std::optional<std::string> get(const std::string& key) {
            //AVX2/AVX512 加速哈希计算
            size_t index = hash(key);
            auto* node = buckets_[index].load(std::memory_order_acquire);
            while (node) {
                if (node->key == key) {
                    return node->value;
                }
                //必须用load方法读取next指针，确保读取到最新的节点地址，防止多线程并发修改
                node = node->next.load(std::memory_order_acquire);
            }
            return std::nullopt;
        }
        //加入DEL和查询功能

    private:
        size_t hash(const std::string& key) {
            uint64_t h = std::hash<std::string>{}(key);
            // 简单的哈希函数
            h = ((h >> 32) ^ h) * 0x45d9f3b;
            h = ((h >> 32) ^ h) * 0x45d9f3b;
            h = (h >> 32) ^ h;
            return h % capacity_;
            // return h & (capacity_ - 1); ???
        }

        size_t capacity_;
        std::atomic<HashNode*>* buckets_;
        LockFreeMemoryPool memory_pool_;
        std::unique_ptr<EvictStrategy> strategy_;
    };

    // KV分片
    class KVShard {
    public:
        KVShard() : hash_table_(1024) {
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
