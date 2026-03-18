cmake_minimum_required(VERSION 3.16)
project(example_nanobind)

# 找到 Python 和 nanobind
find_package(Python 3.9 REQUIRED COMPONENTS Interpreter Development)

execute_process(
    COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE nanobind_DIR
)

find_package(nanobind REQUIRED)

# 构建扩展模块
nanobind_add_module(
    Aether  # 模块名（Python中import的名字）
    STABLE_ABI  # 可选：启用稳定ABI，兼容更多Python版本
    LTO  # 可选：链接时优化，提升性能
    main.cpp  # 源文件
)