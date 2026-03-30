# Aether
High Performance In-Memory KV & Messaging Engine
- RDMA Network Acceleration
- SIMD (AVX2) Vectorization
- Multi-Threaded Sharding Architecture (Lock Free Memory Pool and Lock Free Hash Table based CAS)
- KV Store + MQ Message Queue

## compile
```bash
mkdir build
cd build
cmake ..
make -j