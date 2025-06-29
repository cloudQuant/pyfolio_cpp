cmake_minimum_required(VERSION 3.25)
project(pyfolio_cpp VERSION 1.0.0 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Python bindings option
option(BUILD_PYTHON_BINDINGS "Build Python bindings with pybind11" OFF)

# Static Analysis Integration
option(ENABLE_CLANG_TIDY "Enable clang-tidy analysis" OFF)
option(ENABLE_CPPCHECK "Enable cppcheck analysis" OFF)

if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
    if(CLANG_TIDY_EXE)
        message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy")
    else()
        message(WARNING "clang-tidy not found")
    endif()
endif()

if(ENABLE_CPPCHECK)
    find_program(CPPCHECK_EXE NAMES "cppcheck")
    if(CPPCHECK_EXE)
        message(STATUS "cppcheck found: ${CPPCHECK_EXE}")
        set(CMAKE_CXX_CPPCHECK "${CPPCHECK_EXE};--enable=all;--std=c++20;--suppress=missingIncludeSystem;--inline-suppr")
    else()
        message(WARNING "cppcheck not found")
    endif()
endif()

# Coverage support
option(ENABLE_COVERAGE "Enable code coverage analysis" OFF)
if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Code coverage enabled")
    add_compile_options(-g -O0 --coverage -fprofile-arcs -ftest-coverage)
    add_link_options(--coverage)
    
    # Find gcov and lcov
    find_program(GCOV_PATH gcov)
    find_program(LCOV_PATH lcov)
    find_program(GENHTML_PATH genhtml)
    
    if(LCOV_PATH AND GENHTML_PATH)
        message(STATUS "lcov and genhtml found - HTML coverage reports enabled")
        set(COVERAGE_HTML_ENABLED TRUE)
    else()
        message(WARNING "lcov or genhtml not found - only basic coverage available")
        set(COVERAGE_HTML_ENABLED FALSE)
    endif()
endif()

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -march=native)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -DNDEBUG -flto)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -march=native)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -DNDEBUG -flto)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4 /arch:AVX2)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2 /DNDEBUG /GL)
    endif()
endif()

# Find dependencies
find_package(Eigen3 QUIET)
find_package(Threads REQUIRED)

# Optional dependencies
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    add_compile_definitions(PYFOLIO_HAS_OPENMP)
endif()

# GPU acceleration support
option(ENABLE_CUDA "Enable CUDA GPU acceleration" ON)
option(ENABLE_OPENCL "Enable OpenCL GPU acceleration" ON)

# CUDA support
if(ENABLE_CUDA)
    # Try to find CUDA using both modern and legacy methods
    find_package(CUDAToolkit QUIET)
    if(NOT CUDAToolkit_FOUND)
        find_package(CUDA QUIET)
    endif()
    
    if(CUDAToolkit_FOUND)
        enable_language(CUDA)
        message(STATUS "CUDA found: ${CUDAToolkit_VERSION}")
        set(CUDA_FOUND TRUE)
        add_compile_definitions(PYFOLIO_HAS_CUDA)
    elseif(CUDA_FOUND)
        message(STATUS "CUDA found (legacy): ${CUDA_VERSION}")
        add_compile_definitions(PYFOLIO_HAS_CUDA)
    else()
        message(STATUS "CUDA not found - GPU acceleration will use CPU fallback")
        set(CUDA_FOUND FALSE)
    endif()
else()
    message(STATUS "CUDA support disabled")
    set(CUDA_FOUND FALSE)
endif()

# OpenCL support
if(ENABLE_OPENCL)
    # OpenCL is deprecated on macOS since 10.14, disable it
    if(APPLE)
        message(STATUS "OpenCL disabled on macOS (deprecated since 10.14)")
        set(OpenCL_FOUND FALSE)
    else()
        find_package(OpenCL QUIET)
        if(OpenCL_FOUND)
            message(STATUS "OpenCL found: ${OpenCL_VERSION_STRING}")
            add_compile_definitions(PYFOLIO_HAS_OPENCL)
        else()
            message(STATUS "OpenCL not found - GPU acceleration will use CPU fallback")
        endif()
    endif()
else()
    message(STATUS "OpenCL support disabled")
endif()

# Fetch dependencies for web API
include(FetchContent)

# cpp-httplib - header-only HTTP library
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.14.3
)

# nlohmann/json - header-only JSON library  
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

FetchContent_MakeAvailable(httplib json)

# Include directories
include_directories(${CMAKE_CURRENT_Source_DIR}/include)

# Core library with SIMD implementations
set(PYFOLIO_SOURCES
    src/math/simd_math.cpp
    src/visualization/plotly_enhanced.cpp
    src/backtesting/advanced_backtester.cpp
    src/risk/advanced_risk_models.cpp
    src/ml/model_persistence.cpp
    src/ml/serializable_models.cpp
)

add_library(pyfolio_cpp STATIC ${PYFOLIO_SOURCES})

target_include_directories(pyfolio_cpp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(pyfolio_cpp PUBLIC
    Threads::Threads
)

# Include directories for header-only libraries
target_include_directories(pyfolio_cpp PUBLIC
    $<TARGET_PROPERTY:nlohmann_json::nlohmann_json,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:httplib::httplib,INTERFACE_INCLUDE_DIRECTORIES>
)

if(Eigen3_FOUND)
    target_link_libraries(pyfolio_cpp PUBLIC Eigen3::Eigen)
    target_compile_definitions(pyfolio_cpp PUBLIC PYFOLIO_HAS_EIGEN)
endif()

if(OpenMP_CXX_FOUND)
    target_link_libraries(pyfolio_cpp PUBLIC OpenMP::OpenMP_CXX)
endif()

# Link GPU libraries if available
if(CUDA_FOUND)
    if(CUDAToolkit_FOUND)
        target_link_libraries(pyfolio_cpp PUBLIC CUDA::cudart CUDA::cublas CUDA::curand)
    elseif(CUDA_LIBRARIES)
        target_link_libraries(pyfolio_cpp PUBLIC ${CUDA_LIBRARIES})
        target_include_directories(pyfolio_cpp PUBLIC ${CUDA_INCLUDE_DIRS})
    endif()
endif()

if(OpenCL_FOUND)
    target_link_libraries(pyfolio_cpp PUBLIC ${OpenCL_LIBRARIES})
    target_include_directories(pyfolio_cpp PUBLIC ${OpenCL_INCLUDE_DIRS})
endif()

target_compile_features(pyfolio_cpp PUBLIC cxx_std_20)

# Documentation with Doxygen
option(BUILD_DOCUMENTATION "Build API documentation with Doxygen" OFF)
if(BUILD_DOCUMENTATION)
    find_package(Doxygen REQUIRED dot)
    if(DOXYGEN_FOUND)
        # Set Doxygen configuration
        set(DOXYGEN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/docs/api)
        set(DOXYGEN_WARNINGS_LOG ${CMAKE_BINARY_DIR}/docs/doxygen_warnings.log)
        
        # Configure Doxyfile
        configure_file(${CMAKE_SOURCE_DIR}/Doxyfile ${CMAKE_BINARY_DIR}/Doxyfile @ONLY)
        
        # Add documentation target
        add_custom_target(docs
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
        
        # Add docs to all if requested
        option(BUILD_DOCS_BY_DEFAULT "Build documentation by default" OFF)
        if(BUILD_DOCS_BY_DEFAULT)
            add_custom_target(docs_all ALL DEPENDS docs)
        endif()
        
        message(STATUS "Doxygen found: Documentation target 'docs' available")
        message(STATUS "  Run 'make docs' to generate documentation")
        message(STATUS "  Documentation will be in: ${DOXYGEN_OUTPUT_DIR}/html/index.html")
    else()
        message(WARNING "Doxygen not found - documentation cannot be built")
    endif()
endif()

# Examples
option(BUILD_EXAMPLES "Build examples" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Python bindings
if(BUILD_PYTHON_BINDINGS)
    message(STATUS "Building Python bindings with pybind11")
    find_package(pybind11 QUIET)
    
    if(pybind11_FOUND)
        add_subdirectory(python)
        message(STATUS "Python bindings will be built")
        message(STATUS "  pybind11 version: ${pybind11_VERSION}")
        message(STATUS "  Python version: ${PYTHON_VERSION_STRING}")
    else()
        message(WARNING "pybind11 not found - Python bindings cannot be built")
        message(STATUS "  Install pybind11: pip install pybind11[global]")
    endif()
endif()

# Tests
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests)
endif()

# Python bindings
option(BUILD_PYTHON_BINDINGS "Build Python bindings" OFF)
if(BUILD_PYTHON_BINDINGS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/python/CMakeLists.txt")
    add_subdirectory(python)
endif()

# Install (disabled for now due to dependency export issues)
# install(TARGETS pyfolio_cpp
#         EXPORT pyfolio_cpp-targets
#         ARCHIVE DESTINATION lib
#         LIBRARY DESTINATION lib
#         RUNTIME DESTINATION bin
# )
# 
# install(DIRECTORY include/pyfolio
#         DESTINATION include
#         FILES_MATCHING PATTERN "*.h"
# )
# 
# install(EXPORT pyfolio_cpp-targets
#         FILE pyfolio_cpp-config.cmake
#         NAMESPACE pyfolio_cpp::
#         DESTINATION lib/cmake/pyfolio_cpp
# )

# Coverage targets
if(ENABLE_COVERAGE AND COVERAGE_HTML_ENABLED)
    # Create coverage directory
    set(COVERAGE_OUTPUT_DIR ${CMAKE_BINARY_DIR}/coverage)
    
    # Coverage target for running tests and generating report
    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_OUTPUT_DIR}
        
        # Clean previous coverage data
        COMMAND ${LCOV_PATH} --directory . --zerocounters
        
        # Run tests
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target test
        
        # Capture coverage data
        COMMAND ${LCOV_PATH} --directory . --capture --output-file ${COVERAGE_OUTPUT_DIR}/coverage.info
        
        # Remove system and external files from coverage
        COMMAND ${LCOV_PATH} --remove ${COVERAGE_OUTPUT_DIR}/coverage.info 
                '/usr/*' '/opt/*' '*/gtest/*' '*/googletest/*' '*/test_*' '*/tests/*'
                --output-file ${COVERAGE_OUTPUT_DIR}/coverage_filtered.info
        
        # Generate HTML report
        COMMAND ${GENHTML_PATH} ${COVERAGE_OUTPUT_DIR}/coverage_filtered.info 
                --output-directory ${COVERAGE_OUTPUT_DIR}/html
                --title "Pyfolio C++ Coverage Report"
                --show-details --legend
        
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating code coverage report..."
    )
    
    # Coverage summary target
    add_custom_target(coverage-summary
        COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_OUTPUT_DIR}
        COMMAND ${LCOV_PATH} --directory . --zerocounters
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target test
        COMMAND ${LCOV_PATH} --directory . --capture --output-file ${COVERAGE_OUTPUT_DIR}/coverage.info
        COMMAND ${LCOV_PATH} --remove ${COVERAGE_OUTPUT_DIR}/coverage.info 
                '/usr/*' '/opt/*' '*/gtest/*' '*/googletest/*' '*/test_*' '*/tests/*'
                --output-file ${COVERAGE_OUTPUT_DIR}/coverage_filtered.info
        COMMAND ${LCOV_PATH} --summary ${COVERAGE_OUTPUT_DIR}/coverage_filtered.info
        
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating coverage summary..."
    )
    
    message(STATUS "Coverage targets available:")
    message(STATUS "  make coverage        - Full HTML coverage report")
    message(STATUS "  make coverage-summary - Coverage summary to console")
    message(STATUS "  Coverage report will be in: ${COVERAGE_OUTPUT_DIR}/html/index.html")
endif()

# Static Analysis Targets
if(ENABLE_CLANG_TIDY OR ENABLE_CPPCHECK)
    # Create analysis output directory
    set(ANALYSIS_OUTPUT_DIR ${CMAKE_BINARY_DIR}/static_analysis)
    
    # clang-tidy analysis target
    if(ENABLE_CLANG_TIDY AND CLANG_TIDY_EXE)
        add_custom_target(clang-tidy-analysis
            COMMAND ${CMAKE_COMMAND} -E make_directory ${ANALYSIS_OUTPUT_DIR}
            COMMAND find ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src -name "*.h" -o -name "*.cpp" | 
                    xargs ${CLANG_TIDY_EXE} --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy 
                    --header-filter=${CMAKE_SOURCE_DIR}/include/.* 
                    > ${ANALYSIS_OUTPUT_DIR}/clang-tidy-report.txt 2>&1 || true
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running clang-tidy analysis..."
        )
    endif()
    
    # cppcheck analysis target
    if(ENABLE_CPPCHECK AND CPPCHECK_EXE)
        add_custom_target(cppcheck-analysis
            COMMAND ${CMAKE_COMMAND} -E make_directory ${ANALYSIS_OUTPUT_DIR}
            COMMAND ${CPPCHECK_EXE} --enable=all --std=c++20 --suppress=missingIncludeSystem 
                    --inline-suppr --xml --xml-version=2 
                    --include=${CMAKE_SOURCE_DIR}/include
                    ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src
                    2> ${ANALYSIS_OUTPUT_DIR}/cppcheck-report.xml || true
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running cppcheck analysis..."
        )
    endif()
    
    # Combined static analysis target
    set(STATIC_ANALYSIS_TARGETS "")
    if(ENABLE_CLANG_TIDY AND CLANG_TIDY_EXE)
        list(APPEND STATIC_ANALYSIS_TARGETS clang-tidy-analysis)
    endif()
    if(ENABLE_CPPCHECK AND CPPCHECK_EXE)
        list(APPEND STATIC_ANALYSIS_TARGETS cppcheck-analysis)
    endif()
    
    if(STATIC_ANALYSIS_TARGETS)
        add_custom_target(static-analysis
            DEPENDS ${STATIC_ANALYSIS_TARGETS}
            COMMENT "Running all static analysis tools..."
        )
        
        message(STATUS "Static analysis targets available:")
        if(ENABLE_CLANG_TIDY AND CLANG_TIDY_EXE)
            message(STATUS "  make clang-tidy-analysis - Run clang-tidy")
        endif()
        if(ENABLE_CPPCHECK AND CPPCHECK_EXE)
            message(STATUS "  make cppcheck-analysis   - Run cppcheck")
        endif()
        message(STATUS "  make static-analysis     - Run all analysis tools")
        message(STATUS "  Analysis reports will be in: ${ANALYSIS_OUTPUT_DIR}/")
    endif()
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "=== Pyfolio C++ Configuration Summary ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build examples: ${BUILD_EXAMPLES}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "Build documentation: ${BUILD_DOCUMENTATION}")
message(STATUS "Build Python bindings: ${BUILD_PYTHON_BINDINGS}")
message(STATUS "Coverage enabled: ${ENABLE_COVERAGE}")
if(ENABLE_COVERAGE AND COVERAGE_HTML_ENABLED)
    message(STATUS "Coverage HTML reports: Available")
elseif(ENABLE_COVERAGE)
    message(STATUS "Coverage HTML reports: Not available (lcov/genhtml not found)")
endif()
message(STATUS "Static analysis enabled:")
message(STATUS "  clang-tidy: ${ENABLE_CLANG_TIDY}")
message(STATUS "  cppcheck: ${ENABLE_CPPCHECK}")

# Optional dependencies status
if(Eigen3_FOUND)
    message(STATUS "Eigen3: Found (${Eigen3_VERSION})")
else()
    message(STATUS "Eigen3: Not found")
endif()

if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP: Found")
else()
    message(STATUS "OpenMP: Not found")
endif()

# GPU acceleration status
if(CUDA_FOUND)
    if(CUDAToolkit_FOUND)
        message(STATUS "CUDA: Found (${CUDAToolkit_VERSION})")
    else()
        message(STATUS "CUDA: Found (${CUDA_VERSION})")
    endif()
else()
    message(STATUS "CUDA: Not found")
endif()

if(OpenCL_FOUND)
    message(STATUS "OpenCL: Found (${OpenCL_VERSION_STRING})")
else()
    message(STATUS "OpenCL: Not found")
endif()

message(STATUS "GPU acceleration: ${ENABLE_CUDA} (CUDA), ${ENABLE_OPENCL} (OpenCL)")
message(STATUS "==========================================")
message(STATUS "")