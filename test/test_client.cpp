#include "Aether_client.hpp"
#include <iostream>
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
    // 测试set和get
    Aether::set("key1", "value1");
    Aether::set("key2", "value2");
    auto val1 = Aether::get("key1");
    auto val2 = Aether::get("key2");
    std::cout << "Key 1: " << val1 << std::endl;
    std::cout << "Key 2: " << val2 << std::endl;
    Aether::set("key1", "value3");
    val1 = Aether::get("key1");
    std::cout << "Key 1: " << val1 << std::endl;
    return 0;
}
//g++ -o test_client test_client.cpp -I ../src -L ../build -lAether_client