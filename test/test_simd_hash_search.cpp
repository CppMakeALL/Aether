#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <immintrin.h>

// 简单的哈希函数
uint32_t hash_string(const std::string& key) {
    uint32_t hash = 5381;
    for (char c : key) {
        hash = ((hash << 5) + hash) + static_cast<uint32_t>(c);
    }
    return hash;
}

// 生成随机字符串
std::string generate_random_string(int length) {
    static const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, chars.size() - 1);

    std::string str;
    str.reserve(length);
    for (int i = 0; i < length; ++i) {
        str += chars[dis(gen)];
    }
    return str;
}

// 使用16-bit哈希签名的开放寻址哈希表
class SimpleHashTable16Bit {
public:
    SimpleHashTable16Bit(size_t capacity) : capacity_(capacity), size_(0) {
        hash16_array_ = new uint16_t[capacity_]();
        hash32_array_ = new uint32_t[capacity_]();
        keys_ = new std::string[capacity_];
        values_ = new std::string[capacity_];
    }

    ~SimpleHashTable16Bit() {
        delete[] hash16_array_;
        delete[] hash32_array_;
        delete[] keys_;
        delete[] values_;
    }

    void insert(const std::string& key, const std::string& value) {
        uint32_t h32 = hash_string(key);
        uint16_t h16 = static_cast<uint16_t>(h32 >> 16);
        size_t index = h32 % capacity_;

        while (hash16_array_[index] != 0) {
            if (hash32_array_[index] == h32 && keys_[index] == key) {
                values_[index] = value;
                return;
            }
            index = (index + 1) % capacity_;
        }

        hash16_array_[index] = h16;
        hash32_array_[index] = h32;
        keys_[index] = key;
        values_[index] = value;
        size_++;
    }

    // 16-bit SIMD查找：一次比较16个16位哈希签名
    bool find_simd_16bit(const std::string& key) {
        uint32_t target_hash32 = hash_string(key);
        //uint16_t target_hash16 = static_cast<uint16_t>(target_hash32 >> 16);
        uint16_t target_hash16 = static_cast<uint16_t>(target_hash32 >> 16);
        size_t index = target_hash32 % capacity_;
        size_t probe_count = 0;

        __m256i target = _mm256_set1_epi16(target_hash16);

        while (probe_count < capacity_) {
            if (index + 15 < capacity_) {
                __m256i hashes = _mm256_loadu_si256((__m256i*)&hash16_array_[index]);
                __m256i cmp = _mm256_cmpeq_epi16(hashes, target);
                int simd_mask = _mm256_movemask_epi8(cmp);

                if (simd_mask != 0) {
                    int mask_copy = simd_mask;
                    while (mask_copy) {
                        int pos = __builtin_ctz(mask_copy) / 2;
                        mask_copy &= mask_copy - 1;
                        size_t actual_pos = index + pos;
                        if (hash32_array_[actual_pos] == target_hash32 && keys_[actual_pos] == key) {
                            return true;
                        }
                        //else std::cout<< "once not found"<<std::endl;
                    }
                    //return true;
                }

                __m256i zero = _mm256_setzero_si256();
                __m256i cmp_zero = _mm256_cmpeq_epi16(hashes, zero);
                if (_mm256_movemask_epi8(cmp_zero) != 0) {
                    return false;
                }
                index = (index + 16) % capacity_;
                probe_count += 16;
            } else {
                for (size_t i = index; i < capacity_ && probe_count < capacity_; ++i, ++probe_count) {
                    if (hash16_array_[i] == 0) return false;
                    if (hash32_array_[i] == target_hash32 && keys_[i] == key) return true;
                }
                for (size_t i = 0; i < index && probe_count < capacity_; ++i, ++probe_count) {
                    if (hash16_array_[i] == 0) return false;
                    if (hash32_array_[i] == target_hash32 && keys_[i] == key) return true;
                }
                break;
            }
        }
        return false;
    }

    // 32-bit SIMD查找：一次比较8个32位哈希值
    bool find_simd_32bit(const std::string& key) {
        uint32_t target_hash = hash_string(key);
        size_t index = target_hash % capacity_;
        size_t probe_count = 0;

        __m256i target = _mm256_set1_epi32(target_hash);

        while (probe_count < capacity_) {
            if (index + 7 < capacity_) {
                __m256i hashes = _mm256_loadu_si256((__m256i*)&hash32_array_[index]);
                __m256i cmp = _mm256_cmpeq_epi32(hashes, target);
                int simd_mask = _mm256_movemask_epi8(cmp);

                if (simd_mask != 0) {
                    int mask_copy = simd_mask;
                    while (mask_copy) {
                        int pos = __builtin_ctz(mask_copy) / 4;
                        mask_copy &= mask_copy - 1;
                        size_t actual_pos = index + pos;
                        if (hash32_array_[actual_pos] == target_hash && keys_[actual_pos] == key) {
                            return true;
                        }
                        //else std::cout<< "once not found"<<std::endl;
                    }
                }

                __m256i zero = _mm256_setzero_si256();
                __m256i cmp_zero = _mm256_cmpeq_epi32(hashes, zero);
                if (_mm256_movemask_epi8(cmp_zero) != 0) {
                    return false;
                }
                index = (index + 8) % capacity_;
                probe_count += 8;
            } else {
                for (size_t i = index; i < capacity_ && probe_count < capacity_; ++i, ++probe_count) {
                    if (hash16_array_[i] == 0) return false;
                    if (hash32_array_[i] == target_hash && keys_[i] == key) return true;
                }
                for (size_t i = 0; i < index && probe_count < capacity_; ++i, ++probe_count) {
                    if (hash16_array_[i] == 0) return false;
                    if (hash32_array_[i] == target_hash && keys_[i] == key) return true;
                }
                break;
            }
        }
        return false;
    }

    // 非SIMD查找：逐个比较
    bool find_scalar(const std::string& key) {
        uint32_t target_hash = hash_string(key);
        size_t index = target_hash % capacity_;
        size_t probe_count = 0;

        while (probe_count < capacity_) {
            if (hash16_array_[index] == 0) return false;
            if (hash32_array_[index] == target_hash && keys_[index] == key) return true;
            index = (index + 1) % capacity_;
            probe_count++;
        }
        return false;
    }

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }

private:
    size_t capacity_;
    size_t size_;
    uint16_t* hash16_array_;
    uint32_t* hash32_array_;
    std::string* keys_;
    std::string* values_;
};

int main() {
    const int NUM_KEYS = 12000;
    const int NUM_ITERATIONS = 10;
    const size_t TABLE_CAPACITY = 20000;

    std::cout << "=== SIMD Hash Search Performance Test ===" << std::endl;
    std::cout << "Number of keys: " << NUM_KEYS << std::endl;
    std::cout << "Table capacity: " << TABLE_CAPACITY << std::endl;
    std::cout << "Load factor: " << (NUM_KEYS * 100.0 / TABLE_CAPACITY) << "%" << std::endl;
    std::cout << "Number of iterations: " << NUM_ITERATIONS << std::endl;
    std::cout << std::endl;

    SimpleHashTable16Bit hash_table(TABLE_CAPACITY);

    std::vector<std::string> keys;
    keys.reserve(NUM_KEYS);

    std::cout << "Generating and inserting " << NUM_KEYS << " random keys..." << std::endl;
    for (int i = 0; i < NUM_KEYS; ++i) {
        std::string key = generate_random_string(16);  // 生成16字符的随机key
        keys.push_back(key);
        hash_table.insert(key, "value" + std::to_string(i));
    }
    std::cout << "Insertion completed. Actual load factor: " << (hash_table.size() * 100.0 / hash_table.capacity()) << "%" << std::endl;
    std::cout << std::endl;

    // 测试16-bit SIMD查找（一次16个）
    std::cout << "=== Testing 16-bit SIMD Search (16 at a time) ===" << std::endl;
    std::vector<long long> simd16_times;
    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        auto start = std::chrono::high_resolution_clock::now();
        int found = 0;
        for (const std::string& key : keys) {
            if (hash_table.find_simd_16bit(key)) {
                found++;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        simd16_times.push_back(duration.count());
        std::cout << "Iteration " << (iter + 1) << ": " << duration.count() << " us (found: " << found << "/" << NUM_KEYS << ")" << std::endl;
    }

    long long simd16_avg = 0;
    for (auto t : simd16_times) simd16_avg += t;
    simd16_avg /= NUM_ITERATIONS;
    std::cout << "Average: " << simd16_avg << " us (" << (simd16_avg / NUM_KEYS) << " us/key)" << std::endl;
    std::cout << std::endl;

    // 测试32-bit SIMD查找（一次8个）
    std::cout << "=== Testing 32-bit SIMD Search (8 at a time) ===" << std::endl;
    std::vector<long long> simd32_times;
    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        auto start = std::chrono::high_resolution_clock::now();
        int found = 0;
        for (const std::string& key : keys) {
            if (hash_table.find_simd_32bit(key)) {
                found++;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        simd32_times.push_back(duration.count());
        std::cout << "Iteration " << (iter + 1) << ": " << duration.count() << " us (found: " << found << "/" << NUM_KEYS << ")" << std::endl;
    }

    long long simd32_avg = 0;
    for (auto t : simd32_times) simd32_avg += t;
    simd32_avg /= NUM_ITERATIONS;
    std::cout << "Average: " << simd32_avg << " us (" << (simd32_avg / NUM_KEYS) << " us/key)" << std::endl;
    std::cout << std::endl;

    // 测试标量查找
    std::cout << "=== Testing Scalar Search (one at a time) ===" << std::endl;
    std::vector<long long> scalar_times;
    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        auto start = std::chrono::high_resolution_clock::now();
        int found = 0;
        for (const std::string& key : keys) {
            if (hash_table.find_scalar(key)) {
                found++;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        scalar_times.push_back(duration.count());
        std::cout << "Iteration " << (iter + 1) << ": " << duration.count() << " us (found: " << found << "/" << NUM_KEYS << ")" << std::endl;
    }

    long long scalar_avg = 0;
    for (auto t : scalar_times) scalar_avg += t;
    scalar_avg /= NUM_ITERATIONS;
    std::cout << "Average: " << scalar_avg << " us (" << (scalar_avg / NUM_KEYS) << " us/key)" << std::endl;
    std::cout << std::endl;

    // 性能对比
    std::cout << "=== Performance Comparison ===" << std::endl;
    double speedup_16bit_vs_scalar = static_cast<double>(scalar_avg) / static_cast<double>(simd16_avg);
    double speedup_32bit_vs_scalar = static_cast<double>(scalar_avg) / static_cast<double>(simd32_avg);
    double speedup_16bit_vs_32bit = static_cast<double>(simd32_avg) / static_cast<double>(simd16_avg);

    std::cout << "16-bit SIMD vs Scalar: " << speedup_16bit_vs_scalar << "x faster" << std::endl;
    std::cout << "32-bit SIMD vs Scalar: " << speedup_32bit_vs_scalar << "x faster" << std::endl;
    std::cout << "16-bit SIMD vs 32-bit SIMD: " << speedup_16bit_vs_32bit << "x faster" << std::endl;
    std::cout << std::endl;

    if (speedup_16bit_vs_32bit > 1.0) {
        std::cout << "✓ 16-bit SIMD is faster than 32-bit SIMD!" << std::endl;
        std::cout << "Performance improvement: " << ((1.0 - (static_cast<double>(simd16_avg) / static_cast<double>(simd32_avg))) * 100) << "%" << std::endl;
    } else {
        std::cout << "✗ 16-bit SIMD is slower than 32-bit SIMD!" << std::endl;
        std::cout << "Performance difference: " << ((1.0 - speedup_16bit_vs_32bit) * 100) << "%" << std::endl;
    }

    return 0;
}