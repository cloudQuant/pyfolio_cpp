# CMakeLists.txt for PyFolio C++ Python Bindings
cmake_minimum_required(VERSION 3.20)

project(pyfolio_cpp_bindings VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(pybind11 REQUIRED)

# Include parent project directories
set(PYFOLIO_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${PYFOLIO_ROOT_DIR}/include)
include_directories(${PYFOLIO_ROOT_DIR}/third_party)

# Add third-party includes
include_directories(${PYFOLIO_ROOT_DIR}/third_party/nlohmann_json/include)
include_directories(${PYFOLIO_ROOT_DIR}/third_party/httplib)

# Source files from parent project
set(PYFOLIO_SOURCES
    ${PYFOLIO_ROOT_DIR}/src/math/simd_math.cpp
    ${PYFOLIO_ROOT_DIR}/src/visualization/plotly_enhanced.cpp
    ${PYFOLIO_ROOT_DIR}/src/backtesting/advanced_backtester.cpp
)

# Python binding sources
set(BINDING_SOURCES
    pyfolio_bindings.cpp
)

# Create pybind11 module
pybind11_add_module(pyfolio_cpp 
    ${BINDING_SOURCES}
    ${PYFOLIO_SOURCES}
)

# Compiler-specific options
target_compile_definitions(pyfolio_cpp PRIVATE VERSION_INFO=${EXAMPLE_VERSION_INFO})

# Platform-specific settings
if(WIN32)
    target_compile_options(pyfolio_cpp PRIVATE /O2 /MD)
    target_compile_definitions(pyfolio_cpp PRIVATE _WIN32_WINNT=0x0601)
elseif(APPLE)
    target_compile_options(pyfolio_cpp PRIVATE -O3 -march=native -ffast-math)
    target_link_libraries(pyfolio_cpp PRIVATE "-framework Accelerate")
else()
    target_compile_options(pyfolio_cpp PRIVATE -O3 -march=native -ffast-math)
    
    # Try to find and link OpenMP
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        target_link_libraries(pyfolio_cpp PRIVATE OpenMP::OpenMP_CXX)
    endif()
    
    # Try to find BLAS/LAPACK
    find_package(BLAS)
    find_package(LAPACK)
    if(BLAS_FOUND)
        target_link_libraries(pyfolio_cpp PRIVATE ${BLAS_LIBRARIES})
    endif()
    if(LAPACK_FOUND)
        target_link_libraries(pyfolio_cpp PRIVATE ${LAPACK_LIBRARIES})
    endif()
endif()

# Try to find and configure CUDA
find_package(CUDA QUIET)
if(CUDA_FOUND)
    enable_language(CUDA)
    target_compile_definitions(pyfolio_cpp PRIVATE PYFOLIO_HAS_CUDA=1)
    
    find_package(CUDAToolkit QUIET)
    if(CUDAToolkit_FOUND)
        target_link_libraries(pyfolio_cpp PRIVATE CUDA::cudart CUDA::cublas CUDA::curand)
    endif()
endif()

# Try to find and configure OpenCL
find_package(OpenCL QUIET)
if(OpenCL_FOUND)
    target_compile_definitions(pyfolio_cpp PRIVATE PYFOLIO_HAS_OPENCL=1)
    target_link_libraries(pyfolio_cpp PRIVATE ${OpenCL_LIBRARIES})
    target_include_directories(pyfolio_cpp PRIVATE ${OpenCL_INCLUDE_DIRS})
endif()

# Additional optimizations
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(pyfolio_cpp PRIVATE 
        -fno-strict-aliasing
        -fwrapv
        -Wall
        -Wextra
    )
    
    # Enable SIMD optimizations if available
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
    if(COMPILER_SUPPORTS_AVX2)
        target_compile_options(pyfolio_cpp PRIVATE -mavx2)
        target_compile_definitions(pyfolio_cpp PRIVATE PYFOLIO_HAS_AVX2=1)
    endif()
    
    check_cxx_compiler_flag("-msse4.2" COMPILER_SUPPORTS_SSE42)
    if(COMPILER_SUPPORTS_SSE42)
        target_compile_options(pyfolio_cpp PRIVATE -msse4.2)
        target_compile_definitions(pyfolio_cpp PRIVATE PYFOLIO_HAS_SSE42=1)
    endif()
endif()

# Installation
install(TARGETS pyfolio_cpp
    LIBRARY DESTINATION .
    RUNTIME DESTINATION .
)

# Testing (optional)
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    enable_testing()
    
    # Find Python interpreter
    find_package(Python COMPONENTS Interpreter REQUIRED)
    
    # Add Python tests
    add_test(
        NAME python_basic_test
        COMMAND ${Python_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/tests/
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    
    # Set test environment
    set_tests_properties(python_basic_test PROPERTIES
        ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_BINARY_DIR}:$ENV{PYTHONPATH}"
    )
endif()

# Print configuration summary
message(STATUS "PyFolio C++ Python Bindings Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Python version: ${Python_VERSION}")
message(STATUS "  pybind11 version: ${pybind11_VERSION}")

if(CUDA_FOUND)
    message(STATUS "  CUDA support: YES (${CUDA_VERSION})")
else()
    message(STATUS "  CUDA support: NO")
endif()

if(OpenCL_FOUND)
    message(STATUS "  OpenCL support: YES")
else()
    message(STATUS "  OpenCL support: NO")
endif()

if(OpenMP_CXX_FOUND)
    message(STATUS "  OpenMP support: YES")
else()
    message(STATUS "  OpenMP support: NO")
endif()