#include <AE_Client.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <chrono>
#include <random>
#include <cstddef>
std::string generate_random_string(size_t size_bytes) {
    // 1KB = 1024 字节
    const size_t total_bytes = size_bytes;
    
    // 可见 ASCII 字符范围：32 ~ 126
    static const char chars[] = 
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "!@#$%^&*()_+-=[]{}|;:,.<>?";
    
    static const size_t char_count = sizeof(chars) - 1;

    // 高性能随机数生成器
    static std::mt19937 rng(std::random_device{}());
    static std::uniform_int_distribution<> dist(0, (int)char_count - 1);

    std::string result;
    result.reserve(total_bytes); // 预分配内存，超快

    for (size_t i = 0; i < total_bytes; ++i) {
        result += chars[dist(rng)];
    }

    return result;
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

    // 预分配key和value
    const int num_keys = 1000;
    const int num_tests = 5; // 运行5次测试
    std::vector<std::string> keys;
    std::vector<std::string> values;
    
    // 生成所有key和value
    for(int i = 0; i < num_keys; i++){
        keys.push_back("key_" + std::to_string(i));
        values.push_back(generate_random_string(50 * 1024));
    }

    // 运行多次测试
    std::vector<long long> set_times;
    std::vector<long long> get_times;
    
    for(int test = 0; test < num_tests; test++){
        std::cout << "\nTest " << test + 1 << ":" << std::endl;
        
        // 测试set操作
        auto start = std::chrono::high_resolution_clock::now();
        for(int i = 0; i < num_keys; i++){
            Aether::set(keys[i], values[i]);
        }
        auto end = std::chrono::high_resolution_clock::now();
        long long set_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        set_times.push_back(set_time);
        std::cout << "Set " << num_keys << " keys in " << set_time << " us" << std::endl;
        
        // 测试get操作
        start = std::chrono::high_resolution_clock::now();
        for(int i = 0; i < num_keys; i++){
            auto val = Aether::get(keys[i]);
            // if (val != values[i]) {
            //     std::cerr << "Get error: key " << keys[i] << " expected " << values[i] << std::endl << "but got " << val << std::endl;
            //     return 1;
            // }
        }
        end = std::chrono::high_resolution_clock::now();
        long long get_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        get_times.push_back(get_time);
        std::cout << "Get " << num_keys << " keys in " << get_time << " us" << std::endl;
        
        // 短暂休息，避免系统负载影响
        usleep(100000); // 100ms
    }

    // 计算平均时间
    long long total_set_time = 0;
    long long total_get_time = 0;
    for(int i = 0; i < num_tests; i++){
        total_set_time += set_times[i];
        total_get_time += get_times[i];
    }
    
    std::cout << "\nAverage results:" << std::endl;
    std::cout << "Average set time: " << total_set_time / num_tests/num_keys << " us/key" << std::endl;
    std::cout << "Average get time: " << total_get_time / num_tests/num_keys << " us/key" << std::endl;

    return 0;
}
//g++ -o test_client test_client.cpp -I ../src -L ../build -lAether_client