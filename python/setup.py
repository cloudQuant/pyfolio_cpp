#!/usr/bin/env python3
"""
PyFolio C++ Python Bindings Setup Script
High-performance portfolio analytics with seamless Python integration
"""

import pybind11
from pybind11.setup_helpers import Pybind11Extension, build_ext
from pybind11.setup_helpers import ParallelCompile
from setuptools import setup, Extension
import os
import sys
import platform

# Parallel compilation
ParallelCompile("NPY_NUM_BUILD_JOBS", default=4).install()

# Determine platform-specific settings
def get_platform_settings():
    """Get platform-specific compiler and linker settings"""
    if platform.system() == "Windows":
        extra_compile_args = ["/std:c++20", "/O2", "/MD"]
        extra_link_args = []
        libraries = []
    elif platform.system() == "Darwin":  # macOS
        extra_compile_args = ["-std=c++20", "-O3", "-march=native", "-ffast-math"]
        extra_link_args = ["-framework", "Accelerate"]
        libraries = []
    else:  # Linux
        extra_compile_args = ["-std=c++20", "-O3", "-march=native", "-ffast-math", "-fopenmp"]
        extra_link_args = ["-fopenmp"]
        libraries = ["blas", "lapack"]
    
    return extra_compile_args, extra_link_args, libraries

# Get project root directory
project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Include directories
include_dirs = [
    pybind11.get_cmake_dir() + "/../include",
    os.path.join(project_root, "include"),
    os.path.join(project_root, "third_party"),
    os.path.join(project_root, "third_party", "nlohmann_json", "include"),
    os.path.join(project_root, "third_party", "httplib"),
]

# Add system include directories
if platform.system() == "Darwin":
    include_dirs.extend([
        "/opt/homebrew/include",
        "/usr/local/include"
    ])
elif platform.system() == "Linux":
    include_dirs.extend([
        "/usr/include/eigen3",
        "/usr/local/include"
    ])

# Source files
sources = [
    "pyfolio_bindings.cpp",
    # Core sources
    os.path.join(project_root, "src", "math", "simd_math.cpp"),
    os.path.join(project_root, "src", "visualization", "plotly_enhanced.cpp"),
    os.path.join(project_root, "src", "backtesting", "advanced_backtester.cpp"),
]

# Get platform settings
extra_compile_args, extra_link_args, libraries = get_platform_settings()

# Add OpenMP support if available
def check_openmp():
    """Check if OpenMP is available"""
    try:
        import subprocess
        result = subprocess.run(["gcc", "-fopenmp", "-x", "c", "-", "-o", "/dev/null"],
                              input="int main(){return 0;}", 
                              text=True, capture_output=True)
        return result.returncode == 0
    except:
        return False

if check_openmp() and platform.system() != "Darwin":
    extra_compile_args.append("-fopenmp")
    extra_link_args.append("-fopenmp")

# Define the extension
ext_modules = [
    Pybind11Extension(
        "pyfolio_cpp",
        sources,
        include_dirs=include_dirs,
        language="c++",
        cxx_std=20,
        extra_compile_args=extra_compile_args,
        extra_link_args=extra_link_args,
        libraries=libraries,
    ),
]

# Read long description
long_description = """
PyFolio C++ - High-Performance Portfolio Analytics

A comprehensive C++ library for quantitative finance and portfolio analytics
with seamless Python integration through pybind11.

Features:
- High-performance time series analysis
- Advanced portfolio metrics (Sharpe, Sortino, Calmar ratios)
- Comprehensive backtesting framework with transaction costs
- Machine learning regime detection
- Real-time streaming analysis
- GPU acceleration support
- Interactive visualizations

Key Capabilities:
- SIMD-optimized financial computations
- Memory-efficient data structures
- Comprehensive risk analytics (VaR, CVaR, drawdown analysis)
- Multi-strategy backtesting with realistic transaction costs
- Advanced market impact and slippage modeling
- Rolling window metrics with O(n) complexity
- Real-time streaming data processing
- Machine learning regime detection algorithms

Performance Benefits:
- 10-100x faster than pure Python implementations
- Memory-efficient time series operations
- Parallel processing for large datasets
- GPU acceleration for portfolio optimization
- SIMD vectorization for mathematical operations

Python Integration:
- Seamless NumPy array integration
- Pandas DataFrame compatibility
- Jupyter notebook support
- Matplotlib/Plotly visualization integration
"""

setup(
    name="pyfolio-cpp",
    version="1.0.0",
    author="PyFolio Development Team",
    author_email="dev@pyfolio.ai",
    description="High-performance portfolio analytics library",
    long_description=long_description,
    long_description_content_type="text/plain",
    url="https://github.com/pyfolio/pyfolio-cpp",
    project_urls={
        "Documentation": "https://pyfolio-cpp.readthedocs.io/",
        "Source": "https://github.com/pyfolio/pyfolio-cpp",
        "Tracker": "https://github.com/pyfolio/pyfolio-cpp/issues",
    },
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    python_requires=">=3.8",
    install_requires=[
        "numpy>=1.20.0",
        "pandas>=1.3.0",
        "matplotlib>=3.3.0",
        "scipy>=1.7.0",
    ],
    extras_require={
        "dev": [
            "pytest>=6.0",
            "pytest-cov",
            "black",
            "flake8",
            "mypy",
            "sphinx",
            "sphinx-rtd-theme",
        ],
        "viz": [
            "plotly>=5.0.0",
            "dash>=2.0.0",
            "jupyter",
        ],
        "ml": [
            "scikit-learn>=1.0.0",
            "tensorflow>=2.6.0",
            "torch>=1.9.0",
        ],
        "all": [
            "plotly>=5.0.0",
            "dash>=2.0.0",
            "jupyter",
            "scikit-learn>=1.0.0",
            "tensorflow>=2.6.0",
            "torch>=1.9.0",
        ],
    },
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Financial and Insurance Industry",
        "Intended Audience :: Science/Research",
        "License :: OSI Approved :: Apache Software License",
        "Operating System :: OS Independent",
        "Programming Language :: C++",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Topic :: Office/Business :: Financial :: Investment",
        "Topic :: Scientific/Engineering :: Mathematics",
        "Topic :: Software Development :: Libraries :: Python Modules",
    ],
    keywords="finance, portfolio, analytics, quantitative, backtesting, risk, performance",
    zip_safe=False,
)