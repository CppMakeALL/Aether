#include <stdio.h>
#include <immintrin.h>
#include <stdint.h>
#include <chrono>
#include <random>
int normal_find_eq(const int* data, int count, int target) {
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (data[i] == target) {
            found++;
        }
    }
    return found;
}

int avx2_find_eq(const int* data, int count, int target) {
    int found = 0;
    int i = 0;

    // AVX2 一次处理 8 个 int
    for (; i <= count - 8; i += 8) {
        __m256i vec = _mm256_loadu_si256((const __m256i*)(data + i));
        __m256i target_vec = _mm256_set1_epi32(target);
        
        // 并行比较 8 个 int
        __m256i cmp_result = _mm256_cmpeq_epi32(vec, target_vec);
        // 把比较结果转成掩码
        int mask = _mm256_movemask_ps((__m256)cmp_result);

        // 统计命中数
        found += __builtin_popcount(mask);
    }

    // 处理剩余数据
    for (; i < count; i++) {
        if (data[i] == target)
            found++;
    }
    return found;
}

int main() {
    const int count = 100000;
    int data[count];
    // 1. 初始化随机数引擎 (使用随机设备作为种子)
    std::random_device rd;  
    std::mt19937 gen(rd()); 
    // 2. 定义范围 [0, 100]
    std::uniform_int_distribution<> distrib(0, 100); 

    // 3. 循环赋值
    for (int i = 0; i < count; ++i) {
        data[i] = distrib(gen);
    }
    int target = 5;                          // 要查找的值

    // 调用两个函数
    auto start_normal = std::chrono::high_resolution_clock::now();
    int ret_normal = normal_find_eq(data, count, target);
    auto end_normal = std::chrono::high_resolution_clock::now();
    auto normal_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_normal - start_normal);
    printf("普通版耗时: %ld 微秒\n", normal_duration.count());

    auto start_avx2 = std::chrono::high_resolution_clock::now();
    int ret_avx2 = avx2_find_eq(data, count, target);
    auto end_avx2 = std::chrono::high_resolution_clock::now();
    auto avx2_duration = std::chrono::duration_cast<std::chrono::microseconds>(end_avx2 - start_avx2);
    printf("AVX2版耗时: %ld 微秒\n", avx2_duration.count());

    // 输出结果
    printf("数据总数: %d\n", count);
    printf("查找目标: %d\n", target);
    printf("普通版找到数量: %d\n", ret_normal);
    printf("AVX512版找到数量: %d\n", ret_avx2);

    return 0;
}