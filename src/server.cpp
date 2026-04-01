#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <spdlog/spdlog.h>
#include "KVEngine.hpp"
#include <mutex> 
using namespace Aether;

//std::mutex clients_mutex; 
const int MAX_EVENTS = 1024;
const int BUFFER_SIZE = 4096;

// 客户端连接结构体
struct Client {
    int fd;
    char buffer[BUFFER_SIZE];
    int buffer_len;
};

// 设置非阻塞
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 启动服务器
int start_server(const char* ip, int port) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0); //SOCK_STREAM 暂时用TCP协议
    if (listen_fd == -1) {
        perror("socket");
        return -1;
    }

    // 设置地址重用
    //允许这个端口在 “刚关闭、还没完全释放” 的状态下，立刻被重新绑定使用。
    //一个端口关闭后，会进入 TIME_WAIT 状态，持续 1~2 分钟才能真正释放。如果不加这行，服务关闭立即重启会报错绑定失败
    int opt = 1;
    //SOL_SOCKET：对socket 本身设置选项
    //SO_REUSEADDR：允许端口重用
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 绑定地址
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);//统一大端小端字节序
    //把传入的ip放入addr.sin_addr，转换失败就报错
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(listen_fd);
        return -1;
    }

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(listen_fd);
        return -1;
    }

    // 监听
    if (listen(listen_fd, 1024) == -1) {
        perror("listen");
        close(listen_fd);
        return -1;
    }

    // 设置非阻塞
    set_nonblocking(listen_fd);

    return listen_fd;
}

// 处理客户端请求
void handle_client_request(Client* client) {
    // 解析请求
    std::string request(client->buffer, client->buffer_len);
    size_t pos = request.find("\r\n");
    if (pos == std::string::npos) {
        // 未收到完整请求
        return;
    }

    std::string cmd = request.substr(0, pos);
    client->buffer_len = 0; // 清空缓冲区

    // 解析命令
    size_t space_pos = cmd.find(' ');
    if (space_pos == std::string::npos) {
        // 无效命令
        send(client->fd, "ERROR\r\n", 7, 0);
        return;
    }

    std::string op = cmd.substr(0, space_pos);
    std::string rest = cmd.substr(space_pos + 1);

    if (op == "SET") {
        // SET key value
        size_t value_pos = rest.find(' ');
        if (value_pos == std::string::npos) {
            send(client->fd, "ERROR\r\n", 7, 0);
            return;
        }
        std::string key_str = rest.substr(0, value_pos);
        std::string value = rest.substr(value_pos + 1);
        uint64_t key = std::stoull(key_str);
        KVEngine::instance().set(key, value);
        send(client->fd, "OK\r\n", 4, 0);
    } else if (op == "GET") {
        // GET key
        std::string key_str = rest;
        uint64_t key = std::stoull(key_str);
        auto value = KVEngine::instance().get(key);
        if (value) {
            std::string response = "OK " + *value + "\r\n";
            send(client->fd, response.c_str(), response.size(), 0);
        } else {
            send(client->fd, "ERROR\r\n", 7, 0);
        }
    } else {
        // 未知命令
        send(client->fd, "ERROR\r\n", 7, 0);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <ip> <port>" << std::endl;
        return 1;
    }

    const char* ip = argv[1];
    int port = std::stoi(argv[2]);

    // 启动服务器
    int listen_fd = start_server(ip, port);
    if (listen_fd == -1) {
        return 1;
    }

    // 创建 epoll
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        close(listen_fd);
        return 1;
    }

    // 添加监听套接字到 epoll
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &event) == -1) {
        perror("epoll_ctl");
        close(listen_fd);
        close(epoll_fd);
        return 1;
    }

    // 事件循环
    struct epoll_event events[MAX_EVENTS];
    spdlog::info("Server started on {}:{} port", ip, port);
    while (true) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == listen_fd) {
                // 新连接 - 边缘触发模式下需要循环接受所有待处理的连接
                while (true) {
                    struct sockaddr_in client_addr;
                    socklen_t client_addr_len = sizeof(client_addr);
                    int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_addr_len);
                    if (client_fd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // 没有更多连接了
                            break;
                        } else {
                            perror("accept");
                            break;
                        }
                    }

                    // 设置非阻塞
                    set_nonblocking(client_fd);

                    // 创建客户端结构
                    Client* client = new Client();
                    client->fd = client_fd;
                    client->buffer_len = 0;

                    // 添加到 epoll
                    event.events = EPOLLIN | EPOLLET;
                    event.data.ptr = client;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                        perror("epoll_ctl");
                        delete client;
                        close(client_fd);
                        continue;
                    }

                    spdlog::info("New client connected: {}", client_fd);
                }
            } else {
                // 客户端数据
                Client* client = static_cast<Client*>(events[i].data.ptr);
                int client_fd = client->fd;
                spdlog::info("Client {} send data", client_fd);

                if (!client) continue;

                // 读取数据
                char buffer[BUFFER_SIZE];
                bool read_error = false;
                
                // 边缘触发模式下，需要循环读取直到EAGAIN
                while (true) {
                    int n = read(client_fd, buffer, BUFFER_SIZE);
                    if (n < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // 数据已读完
                            break;
                        } else {
                            // 真正的错误
                            spdlog::error("Client {} read error: {}", client_fd, strerror(errno));
                            read_error = true;
                            break;
                        }
                    } else if (n == 0) {
                        // 连接关闭
                        read_error = true;
                        spdlog::info("Client {} closed", client_fd);
                        break;
                    } else {
                        // 处理数据
                        if (client->buffer_len + n < BUFFER_SIZE) {
                            memcpy(client->buffer + client->buffer_len, buffer, n);
                            client->buffer_len += n;
                            // 尝试处理请求
                            handle_client_request(client);
                        } else {
                            spdlog::error("Client {} buffer overflow", client_fd);
                            // 缓冲区已满，关闭连接
                            send(client_fd, "ERROR\r\n", 7, 0);
                            read_error = true;
                            break;
                        }
                    }
                }
                
                if (read_error) {
                    // 连接关闭或出错
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr);
                    close(client_fd);
                    // 直接删除客户端对象
                    delete client;
                }
            }
        }
    }

    // 清理
    close(listen_fd);
    close(epoll_fd);

    return 0;
}