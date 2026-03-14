#pragma once
#include <atomic>
#include <string>
#include <optional>
#include <cstdint>
namespace Aether {
    // 无锁内存池
    class LockFreeMemoryPool {
    public:
        struct MemoryBlock {
            MemoryBlock* next;
            char data[0]; // 柔性数组
        };

        LockFreeMemoryPool() : block_size_(0) {}

        LockFreeMemoryPool(size_t block_size, size_t initial_blocks = 1024)
            : block_size_(block_size + sizeof(MemoryBlock)) {
            // 预分配初始内存块
            for (size_t i = 0; i < initial_blocks; ++i) {
                auto* block = static_cast<MemoryBlock*>(::malloc(block_size_));
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
              free_list_(other.free_list_.load(std::memory_order_relaxed)) {
            other.block_size_ = 0;
            other.free_list_.store(nullptr, std::memory_order_relaxed);
        }

        // 移动赋值运算符
        LockFreeMemoryPool& operator=(LockFreeMemoryPool&& other) noexcept {
            if (this != &other) {
                block_size_ = other.block_size_;
                free_list_.store(other.free_list_.load(std::memory_order_relaxed), 
                                 std::memory_order_relaxed);
                other.block_size_ = 0;
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
            return ::malloc(block_size_);
        }

        void deallocate(void* ptr) {
            if (!ptr) return;
            auto* block = reinterpret_cast<MemoryBlock*>(
                static_cast<char*>(ptr) - sizeof(MemoryBlock));
            block->next = free_list_.load(std::memory_order_relaxed);
            while (!free_list_.compare_exchange_weak(
                block->next, block, 
                std::memory_order_release, 
                std::memory_order_relaxed));
        }

    private:
        size_t block_size_;
        std::atomic<MemoryBlock*> free_list_{nullptr};
    };

    // 无锁哈希表节点
    struct HashNode {
        uint64_t key;
        std::string value;
        HashNode* next;

        HashNode(uint64_t k, const std::string& v) 
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
                            auto* next = node->next;
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
                    auto* next = node->next;
                    memory_pool_.deallocate(node);
                    node = next;
                }
            }
            delete[] buckets_;
        }

        void set(uint64_t key, const std::string& value) {
            size_t index = hash(key);
            HashNode* new_node = new (memory_pool_.allocate()) HashNode(key, value);
            
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

        std::optional<std::string> get(uint64_t key) {
            size_t index = hash(key);
            auto* node = buckets_[index].load(std::memory_order_acquire);
            while (node) {
                if (node->key == key) {
                    return node->value;
                }
                node = node->next;
            }
            return std::nullopt;
        }

    private:
        size_t hash(uint64_t key) {
            // 简单的哈希函数
            key = ((key >> 32) ^ key) * 0x45d9f3b;
            key = ((key >> 32) ^ key) * 0x45d9f3b;
            key = (key >> 32) ^ key;
            return key % capacity_;
        }

        size_t capacity_;
        std::atomic<HashNode*>* buckets_;
        LockFreeMemoryPool memory_pool_;
    };

    // KV分片
    class KVShard {
    public:
        KVShard() : hash_table_(1024) {}

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

        void set(uint64_t key, const std::string& value) {
            hash_table_.set(key, value);
        }

        std::optional<std::string> get(uint64_t key) {
            return hash_table_.get(key);
        }

    private:
        LockFreeHashTable hash_table_;
    };
}
