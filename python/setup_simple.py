#!/usr/bin/env python3
"""
Simple Setup Script for PyFolio C++ Python Bindings
Core functionality only - for testing and development
"""

import pybind11
from pybind11.setup_helpers import Pybind11Extension, build_ext
from pybind11.setup_helpers import ParallelCompile
from setuptools import setup
import os
import platform

# Parallel compilation
ParallelCompile("NPY_NUM_BUILD_JOBS", default=4).install()

# Get project root directory
project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Include directories
include_dirs = [
    pybind11.get_cmake_dir() + "/../include",
    os.path.join(project_root, "include"),
]

# Source files (core only)
sources = [
    "pyfolio_bindings_minimal.cpp",
]

# Platform-specific settings
if platform.system() == "Windows":
    extra_compile_args = ["/std:c++20", "/O2"]
    extra_link_args = []
elif platform.system() == "Darwin":  # macOS
    extra_compile_args = ["-std=c++20", "-O3", "-march=native"]
    extra_link_args = []
else:  # Linux
    extra_compile_args = ["-std=c++20", "-O3", "-march=native"]
    extra_link_args = []

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
    ),
]

setup(
    name="pyfolio-cpp-simple",
    version="1.0.0",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    python_requires=">=3.8",
    install_requires=[
        "numpy>=1.20.0",
        "pybind11>=2.10.0",
    ],
    zip_safe=False,
)