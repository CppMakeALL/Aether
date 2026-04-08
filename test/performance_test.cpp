#include <AE_Client.hpp>
#include <iostream>
#include <unistd.h>
#include <chrono>
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

    auto start = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < 1000; i++){
        Aether::set("key" + std::to_string(i), "value" + std::to_string(i));
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "set keys in " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " us" << std::endl;
    
    start = std::chrono::high_resolution_clock::now();
    for(int i = 0; i < 1000; i++){
        auto val = Aether::get("key" + std::to_string(i));
    }
    end = std::chrono::high_resolution_clock::now();
    std::cout << "get keys in " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << " us" << std::endl;

    return 0;
}
//g++ -o test_client test_client.cpp -I ../src -L ../build -lAether_client