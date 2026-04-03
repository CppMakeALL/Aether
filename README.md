# Aether
High Performance In-Memory KV & Messaging Engine
- RDMA Network Acceleration
- SIMD (AVX2) Vectorization
- Multi-Threaded Sharding Architecture (Lock Free Memory Pool and Lock Free Hash Table based CAS)
- KV Store + MQ Message Queue

## Dependencies
- C++ Standard: C++17 (default), C++20 (optional)
- Compiler: GCC 9+ (C++17), GCC 11+ (C++20)
- Libraries: ibverbs, rdmacm, pthread

## Install dependencies (Linux)
(Ubuntu/Debian)
```bash
sudo apt-get install -y libibverbs-dev librdmacm-dev
```
(RHEL/CentOS)
```bash
sudo yum install -y libibverbs-devel rdmacm-devel
```

## compile
Supports C++17 by default for maximum portability and compatibility.
Optional C++20 mode enables modern features including:
- std::span for safe and zero-cost memory views
- C++20 coroutines for async network I/O
- Additional modern C++ improvements
```bash
mkdir build
cd build
cmake ..
make
```

## Acknowledgments & Third-Party Projects
This project uses the following open-source libraries from GitHub:
- **[nanobind](https://github.com/wjakob/nanobind)** - Tiny and efficient C++/Python bindings
- **[spdlog](https://github.com/gabime/spdlog)** – Fast C++ logging library