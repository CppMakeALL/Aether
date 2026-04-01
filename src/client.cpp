#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// 连接服务器
int connect_server(const char* ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

// 发送命令并接收响应
std::string send_command(int sockfd, const std::string& command) {
    std::string cmd = command + "\r\n";
    if (send(sockfd, cmd.c_str(), cmd.size(), 0) == -1) {
        perror("send");
        return "ERROR";
    }

    char buffer[4096] = {0};
    int n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (n == -1) {
        perror("recv");
        return "ERROR";
    }

    return std::string(buffer, n);
}

// 测试单线程存取
void test_single_thread(const char* ip, int port) {
    std::cout << "=== 单线程测试 ===" << std::endl;
    
    int sockfd = connect_server(ip, port);
    if (sockfd == -1) {
        return;
    }

    // 测试 SET 命令
    std::string response = send_command(sockfd, "SET 1 test_value");
    std::cout << "SET 1 test_value: " << response;

    // 测试 GET 命令
    response = send_command(sockfd, "GET 1");
    std::cout << "GET 1: " << response;

    // 测试不存在的键
    response = send_command(sockfd, "GET 999");
    std::cout << "GET 999: " << response;

    close(sockfd);
}

// 线程测试函数
void thread_test(const char* ip, int port, int thread_id, int num_operations) {
    int sockfd = connect_server(ip, port);
    if (sockfd == -1) {
        return;
    }

    for (int i = 0; i < num_operations; ++i) {
        // 生成唯一的键
        uint64_t key = thread_id * 1000000 + i;
        std::string value = "value_" + std::to_string(key);

        // SET 操作
        std::string set_cmd = "SET " + std::to_string(key) + " " + value;
        send_command(sockfd, set_cmd);

        // GET 操作
        std::string get_cmd = "GET " + std::to_string(key);
        send_command(sockfd, get_cmd);
    }

    close(sockfd);
}

// 测试多线程存取
void test_multi_thread(const char* ip, int port, int num_threads, int operations_per_thread) {
    std::cout << "\n=== 多线程测试 ===" << std::endl;
    std::cout << "线程数: " << num_threads << std::endl;
    std::cout << "每个线程操作数: " << operations_per_thread << std::endl;

    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(thread_test, ip, port, i, operations_per_thread);
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "多线程测试完成" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <ip> <port>" << std::endl;
        return 1;
    }

    const char* ip = argv[1];
    int port = std::stoi(argv[2]);

    // 测试单线程
    //test_single_thread(ip, port);

    // 测试多线程
    test_multi_thread(ip, port, 2, 10);

    return 0;
}