#include "KVEngine.hpp"
#include <iostream>
#include <thread>
#include <vector>

int main() {
    auto& engine = Aether::KVEngine::instance();
    
    // 测试set和get
    engine.set(1, "Hello");
    engine.set(2, "World");
    
    auto val1 = engine.get(1);
    auto val2 = engine.get(2);
    
    std::cout << "Key 1: " << (val1 ? *val1 : "not found") << std::endl;
    std::cout << "Key 2: " << (val2 ? *val2 : "not found") << std::endl;
    
    // 多线程测试
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([i, &engine]() {
            for (int j = 0; j < 100; ++j) {
                uint64_t key = i * 100 + j;
                engine.set(key, "value_" + std::to_string(key));
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "Multi-thread test passed!" << std::endl;
    
    // 验证数据
    bool all_found = true;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 100; ++j) {
            uint64_t key = i * 100 + j;
            auto val = engine.get(key);
            if (!val) {
                std::cout << "Key " << key << " not found!" << std::endl;
                all_found = false;
            }
        }
    }
    
    if (all_found) {
        std::cout << "All data verified!" << std::endl;
    }
    
    return 0;
}
