#include "AE_Client.hpp"
#include <iostream>
#include <unistd.h>
#include <chrono>
#include <string>
#include <vector>
#include <random>

// 生成随机字符串作为key
std::string generate_random_key(int length) {
    static const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, chars.size() - 1);
    
    std::string key;
    key.reserve(length);
    for (int i = 0; i < length; ++i) {
        key += chars[dis(gen)];
    }
    return key;
}

int main()
{
    auto ret = Aether::connect("192.168.20.184", 8001, Aether::TransMode::TCP);
    if (!ret) {
        std::cerr << "connect failed" << std::endl;
        return 1;
    }
    else {
        std::cout << "connect success" << std::endl;
    }

    // 预先设置一些key-value对（使用随机key增加哈希冲突）
    const int NUM_KEYS = 1000;
    std::vector<std::string> keys;
    keys.reserve(NUM_KEYS);
    
    std::cout << "Setting " << NUM_KEYS << " random keys..." << std::endl;
    for(int i = 0; i < NUM_KEYS; i++){
        std::string key = generate_random_key(10); // 生成10字符的随机key
        keys.push_back(key);
        Aether::set(key, "value" + std::to_string(i));
    }
    std::cout << "Set completed" << std::endl;

    // 测试GET（SIMD版本）
    std::cout << "\n=== Testing GET (SIMD version) ===" << std::endl;
    auto start_simd = std::chrono::high_resolution_clock::now();
    for(const auto& key : keys){
        auto val = Aether::get(key);
    }
    auto end_simd = std::chrono::high_resolution_clock::now();
    auto duration_simd = std::chrono::duration_cast<std::chrono::microseconds>(end_simd - start_simd);
    std::cout << "GET (SIMD) completed in " << duration_simd.count() << " us" << std::endl;
    std::cout << "Average: " << duration_simd.count() / NUM_KEYS << " us/key" << std::endl;

    // 测试GET_NOSIMD（非SIMD版本）
    std::cout << "\n=== Testing GET_NOSIMD (non-SIMD version) ===" << std::endl;
    auto start_nosimd = std::chrono::high_resolution_clock::now();
    for(const auto& key : keys){
        auto val = Aether::get_nosimd(key);
    }
    auto end_nosimd = std::chrono::high_resolution_clock::now();
    auto duration_nosimd = std::chrono::duration_cast<std::chrono::microseconds>(end_nosimd - start_nosimd);
    std::cout << "GET_NOSIMD completed in " << duration_nosimd.count() << " us" << std::endl;
    std::cout << "Average: " << duration_nosimd.count() / NUM_KEYS << " us/key" << std::endl;

    // 计算性能差距
    std::cout << "\n=== Performance Comparison ===" << std::endl;
    double speedup = static_cast<double>(duration_nosimd.count()) / static_cast<double>(duration_simd.count());
    std::cout << "SIMD version is " << speedup << "x faster than non-SIMD version" << std::endl;

    return 0;
}