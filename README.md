# Aether
High Performance In-Memory KV & Messaging Engine
- RDMA Network Acceleration
- SIMD (AVX2) Vectorization
- Multi-Threaded Sharding Architecture (Lock Free Hash Table based CAS)
- KV Store + MQ Message Queue

## Dependencies
- C++ Standard: C++20 (optional)
- Compiler: GCC 11+ (C++20)
- Libraries: ibverbs, rdmacm, pthread, yaml-cpp

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