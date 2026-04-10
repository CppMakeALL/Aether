/*
    Copyright 2026 CppMakeALL. All Rights Reserved.
*/ 
#include "AE_KVShard.hpp"
#include <immintrin.h>
namespace Aether {
    bool LockFreeHashTable::exist(const std::string& key) {
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
    void LockFreeHashTable::evict_key() {
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
    void LockFreeHashTable::set(const std::string& key, const std::string& value) {
        const uint64_t target_hash = hash(key);  // 👈 只算一次 hash
        size_t index = target_hash & (capacity_ - 1);
        const size_t original_index = index;
        size_t probe_count = 0;
        size_t first_deleted_index = capacity_;

        while (true) {
            // ==============================
            //  AVX2 批量比较 8 个 slot（核心加速）
            // ==============================
            if (probe_count % 8 == 0 && (index + 7) < capacity_) {
                // 一次加载 8 个 hash
                __m256i hashes = _mm256_loadu_si256(
                    (__m256i*)&buckets_[index].hash
                );
                __m256i target = _mm256_set1_epi64x(target_hash);
                __m256i cmp = _mm256_cmpeq_epi64(hashes, target);
                int mask = _mm256_movemask_epi8(cmp);

                if (mask != 0) {
                    // 找到匹配的 hash
                    int pos = __builtin_ctz(mask) / 8;//这里只处理第一个匹配的hash,会漏掉其他匹配的hash，需要一次性全部处理
                    HashSlot& slot = buckets_[index + pos];

                    if (slot.status == SlotStatus::OCCUPIED && slot.key == key) {
                        slot.value = value;
                        return;
                    }
                }
            }

            // ==============================
            // 原来的逻辑不变
            // ==============================
            auto status = buckets_[index].status.load(std::memory_order_acquire);

            if (status == SlotStatus::EMPTY) {
                if (first_deleted_index != capacity_) {
                    index = first_deleted_index;
                }

                size_t memory_needed = key.size() + value.size();
                size_t current_used = used_memory_.load(std::memory_order_relaxed);
                size_t new_used = current_used + memory_needed;

                if (new_used > max_memory_) {
                    evict_key();
                    return set(key, value);
                }

                if (buckets_[index].status.compare_exchange_weak(
                    status, SlotStatus::OCCUPIED,
                    std::memory_order_release,
                    std::memory_order_relaxed))
                {
                    // 👈 写入时把 hash 一起存进去
                    buckets_[index].hash = target_hash;
                    buckets_[index].key = key;
                    buckets_[index].value = value;
                    used_memory_.fetch_add(memory_needed, std::memory_order_relaxed);
                    metadata_manager_->add_entry(key, value);
                    notify_all(key, DBEvent::INSERT);
                    return;
                }
                status = buckets_[index].status.load(std::memory_order_acquire);
            }

            if (status == SlotStatus::OCCUPIED) {
                // 这里已经被 AVX2 加速了，所以几乎不会跑到
                if (buckets_[index].hash == target_hash && buckets_[index].key == key) {
                    buckets_[index].value = value;
                    return;
                }
            } else if (status == SlotStatus::DELETED) {
                if (first_deleted_index == capacity_) {
                    first_deleted_index = index;
                }
            }

            index = (index + 1) & (capacity_ - 1);
            probe_count++;

            if (probe_count >= capacity_ || index == original_index) {
                evict_key();
                return set(key, value);
            }
        }
    }
    std::optional<std::string> LockFreeHashTable::get(const std::string& key) {
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
}