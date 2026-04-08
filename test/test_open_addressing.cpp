/*
    Copyright 2026 CppMakeALL. All Rights Reserved.
*/
#include "../src/AE_KVEngine.hpp"
#include <iostream>
#include <thread>
#include <vector>

using namespace Aether;

// 重置KVEngine单例
void reset_kv_engine() {
    // 由于KVEngine是单例，我们需要访问其私有成员来重置
    // 这里使用一个技巧：创建一个派生类来访问私有成员
    class KVEngineResetter : public KVEngine {
    public:
        static void reset() {
            if (inst) {
                delete inst;
                inst = nullptr;
            }
        }
    };
    KVEngineResetter::reset();
}

void test_basic_operations() {
    std::cout << "Testing basic operations..." << std::endl;
    
    // 重置KVEngine
    reset_kv_engine();
    
    // 初始化KVEngine
    KVEngine::init(16, 1024 * 1024); // 1MB内存
    auto& engine = KVEngine::instance();
    
    // 测试set和get
    engine.set("key1", "value1");
    auto value1 = engine.get("key1");
    if (value1 && *value1 == "value1") {
        std::cout << "✓ set and get work correctly" << std::endl;
    } else {
        std::cout << "✗ set and get failed" << std::endl;
    }
    
    // 测试更新
    engine.set("key1", "value1_updated");
    auto value1_updated = engine.get("key1");
    if (value1_updated && *value1_updated == "value1_updated") {
        std::cout << "✓ update works correctly" << std::endl;
    } else {
        std::cout << "✗ update failed" << std::endl;
    }
    
    // 测试不存在的键
    auto value_nonexistent = engine.get("nonexistent");
    if (!value_nonexistent) {
        std::cout << "✓ get nonexistent key works correctly" << std::endl;
    } else {
        std::cout << "✗ get nonexistent key failed" << std::endl;
    }
}

void test_concurrent_operations() {
    std::cout << "\nTesting concurrent operations..." << std::endl;
    
    // 重置KVEngine
    reset_kv_engine();
    
    // 初始化KVEngine
    KVEngine::init(16, 1024 * 1024); // 1MB内存
    auto& engine = KVEngine::instance();
    
    const int num_threads = 4;
    const int num_operations = 1000;
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&engine, i, num_operations]() {
            for (int j = 0; j < num_operations; ++j) {
                std::string key = "key_" + std::to_string(i) + "_" + std::to_string(j);
                std::string value = "value_" + std::to_string(i) + "_" + std::to_string(j);
                engine.set(key, value);
                auto retrieved = engine.get(key);
                if (retrieved && *retrieved == value) {
                    // 操作成功
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "✓ Concurrent operations completed" << std::endl;
}

void test_eviction() {
    std::cout << "\nTesting eviction..." << std::endl;
    
    // 重置KVEngine
    reset_kv_engine();
    
    // 初始化KVEngine，使用较小的内存限制和单个分片
    KVEngine::init(1, 100); // 1个分片，100字节内存
    auto& engine = KVEngine::instance();
    
    // 插入多个键值对，触发驱逐
    for (int i = 0; i < 10; ++i) {
        std::string key = "key_" + std::to_string(i);
        std::string value = std::string(20, 'a'); // 20字节的值
        std::cout << "Inserting key_" << i << " with value size " << value.size() << std::endl;
        engine.set(key, value);
    }
    
    // 检查是否有键被驱逐
    bool eviction_happened = false;
    for (int i = 0; i < 10; ++i) {
        auto value = engine.get("key_" + std::to_string(i));
        if (value) {
            std::cout << "key_" << i << " still exists: " << *value << std::endl;
        } else {
            std::cout << "key_" << i << " has been evicted" << std::endl;
            eviction_happened = true;
        }
    }
    
    if (eviction_happened) {
        std::cout << "✓ Eviction works correctly" << std::endl;
    } else {
        std::cout << "✗ Eviction failed" << std::endl;
    }
}

int main() {
    test_basic_operations();
    test_concurrent_operations();
    test_eviction();
    
    std::cout << "\nAll tests completed!" << std::endl;
    return 0;
}