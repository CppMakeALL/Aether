#include "Aether_client.hpp"
#include <iostream>
using namespace Aether;
int main()
{
    auto ret = connect("192.168.20.184", 8001, TransMode::TCP);
    if (!ret) {
        std::cerr << "connect failed" << std::endl;
        return 1;
    }
    else {
        std::cout << "connect success" << std::endl;
    }
    return 0;
}
//g++ -o test_client test_client.cpp -I ../src -L ../build -lAether_client