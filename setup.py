from setuptools import setup, Extension
from pybind11.setup_helpers import Pybind11Extension, build_ext
import os

# 获取源文件
sources = [
    "src/backtest_wrapper.cpp",
    "src/backtest.cpp",
    "src/account_details.cpp"  # 添加账户详细信息文件
]

# 编译C++模块
ext_modules = [
    Pybind11Extension(
        "cpp_backtest",
        sources=sources,
        include_dirs=["include"],
        # 添加C++17标准支持
        extra_compile_args=["-std=c++17"],
    ),
]

setup(
    name="cpp_backtest",
    version="0.1",
    author="User",
    author_email="user@example.com",
    description="C++ implementation of backtesting functionality",
    long_description="Bridge between Python and C++ for efficient backtesting",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    zip_safe=False,
    python_requires=">=3.6",
) 