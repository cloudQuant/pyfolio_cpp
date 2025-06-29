# Test configuration for pyfolio_cpp

# Enable testing
enable_testing()

# Use FetchContent to build GoogleTest from source for proper static linking
include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Enable test discovery
include(GoogleTest)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Test data library
add_library(pyfolio_test_data STATIC
    test_data/pyfolio_equivalent_data.cpp
)

target_link_libraries(pyfolio_test_data 
    PUBLIC 
    pyfolio_cpp
    Threads::Threads
)

target_include_directories(pyfolio_test_data 
    PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Test sources
set(TEST_SOURCES
    test_core_types.cpp
    test_datetime.cpp
    test_time_series.cpp
    test_statistics.cpp
    test_performance_metrics.cpp
    test_transactions.cpp
    test_round_trips.cpp
    test_positions.cpp
    test_attribution.cpp
    test_risk_analysis.cpp
    test_capacity_analysis.cpp
    test_bayesian_analysis.cpp
    test_regime_detection.cpp
    test_integration.cpp
    test_pyfolio_equivalent.cpp
    test_exact_python_equivalence.cpp
    test_python_txn_equivalence.cpp
    test_python_pos_equivalence.cpp
    test_tear_sheets.cpp
    test_rolling_metrics.cpp
    test_data_loader.cpp
    test_visualization.cpp
    test_cached_performance_metrics.cpp
    test_parallel_algorithms.cpp
    # test_parallel_performance_suite.cpp  # Temporarily disabled due to API issues
    test_plotly_enhanced.cpp
    test_ml_regime_detection.cpp
    test_streaming_analysis.cpp
    test_gpu_acceleration.cpp
    test_advanced_backtesting.cpp
    test_advanced_risk_models.cpp
)

# Create test executable
add_executable(pyfolio_tests ${TEST_SOURCES})

# Link libraries
target_link_libraries(pyfolio_tests 
    PRIVATE 
    pyfolio_test_data
    pyfolio_cpp
    gtest
    gtest_main
    Threads::Threads
)

# Set C++20 standard
target_compile_features(pyfolio_tests PRIVATE cxx_std_20)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(pyfolio_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Discover tests
gtest_discover_tests(pyfolio_tests)

# Add individual test files as separate targets for granular testing
function(add_individual_test test_name test_file)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} 
        PRIVATE 
        pyfolio_cpp
        gtest
        gtest_main
        Threads::Threads
    )
    target_compile_features(${test_name} PRIVATE cxx_std_20)
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

# Individual test targets
add_individual_test(test_core test_core_types.cpp)
add_individual_test(test_datetime_only test_datetime.cpp)
add_individual_test(test_time_series_only test_time_series.cpp)
add_individual_test(test_statistics_only test_statistics.cpp)
add_individual_test(test_performance_only test_performance_metrics.cpp)
add_individual_test(test_transactions_only test_transactions.cpp)
add_individual_test(test_round_trips_only test_round_trips.cpp)
add_individual_test(test_positions_only test_positions.cpp)
add_individual_test(test_attribution_only test_attribution.cpp)
add_individual_test(test_risk_only test_risk_analysis.cpp)
add_individual_test(test_capacity_only test_capacity_analysis.cpp)
add_individual_test(test_bayesian_only test_bayesian_analysis.cpp)
add_individual_test(test_regime_only test_regime_detection.cpp)
add_individual_test(test_integration_only test_integration.cpp)
add_individual_test(test_tear_sheets_only test_tear_sheets.cpp)
add_individual_test(test_rolling_metrics_only test_rolling_metrics.cpp)
add_individual_test(test_data_loader_only test_data_loader.cpp)
add_individual_test(test_visualization_only test_visualization.cpp)
add_individual_test(test_cached_performance_only test_cached_performance_metrics.cpp)
add_individual_test(test_parallel_algorithms_only test_parallel_algorithms.cpp)
# add_individual_test(test_parallel_performance_suite_only test_parallel_performance_suite.cpp)  # Temporarily disabled
add_individual_test(test_plotly_enhanced_only test_plotly_enhanced.cpp)
add_individual_test(test_ml_regime_detection_only test_ml_regime_detection.cpp)
add_individual_test(test_streaming_analysis_only test_streaming_analysis.cpp)
add_individual_test(test_gpu_acceleration_only test_gpu_acceleration.cpp)
add_individual_test(test_advanced_backtesting_only test_advanced_backtesting.cpp)
add_individual_test(test_options_pricing test_options_pricing.cpp)
add_individual_test(test_advanced_risk_models_only test_advanced_risk_models.cpp)

# Python Equivalence Tests - Individual Test Executables
add_individual_test(test_exact_python_equivalence_only test_exact_python_equivalence.cpp)
add_individual_test(test_python_txn_equivalence_only test_python_txn_equivalence.cpp) 
add_individual_test(test_python_pos_equivalence_only test_python_pos_equivalence.cpp)

# Python Equivalence Tests - Comprehensive
add_executable(test_pyfolio_equivalent_only test_pyfolio_equivalent.cpp)
target_link_libraries(test_pyfolio_equivalent_only 
    PRIVATE 
    pyfolio_test_data
    pyfolio_cpp
    gtest
    gtest_main
    Threads::Threads
)
target_compile_features(test_pyfolio_equivalent_only PRIVATE cxx_std_20)
add_test(NAME test_pyfolio_equivalent_only COMMAND test_pyfolio_equivalent_only)

# Comprehensive Python Equivalence Test Runner
add_executable(run_python_equivalent_tests run_python_equivalent_tests.cpp)
target_link_libraries(run_python_equivalent_tests 
    PRIVATE 
    pyfolio_test_data
    pyfolio_cpp
    gtest
    gtest_main
    Threads::Threads
)
target_compile_features(run_python_equivalent_tests PRIVATE cxx_std_20)
add_test(NAME run_python_equivalent_tests COMMAND run_python_equivalent_tests)

# Performance benchmarks
add_executable(performance_benchmarks benchmark_performance.cpp)
target_link_libraries(performance_benchmarks 
    PRIVATE 
    pyfolio_cpp
    gtest
    gtest_main
    Threads::Threads
)
target_compile_features(performance_benchmarks PRIVATE cxx_std_20)

# Comprehensive benchmarks
add_executable(comprehensive_benchmarks test_comprehensive_benchmarks.cpp)
target_link_libraries(comprehensive_benchmarks 
    PRIVATE 
    pyfolio_cpp
    gtest
    gtest_main
    Threads::Threads
)
target_compile_features(comprehensive_benchmarks PRIVATE cxx_std_20)

# Memory tests
add_executable(memory_tests test_memory_usage.cpp)
target_link_libraries(memory_tests 
    PRIVATE 
    pyfolio_cpp
    gtest
    gtest_main
    Threads::Threads
)
target_compile_features(memory_tests PRIVATE cxx_std_20)

# SIMD performance tests
add_executable(simd_performance_tests test_simd_performance.cpp)
target_link_libraries(simd_performance_tests 
    PRIVATE 
    pyfolio_cpp
    gtest
    gtest_main
    Threads::Threads
)
target_compile_features(simd_performance_tests PRIVATE cxx_std_20)
add_test(NAME simd_performance_tests COMMAND simd_performance_tests)

# Memory pool tests
add_executable(memory_pool_tests test_memory_pool.cpp)
target_link_libraries(memory_pool_tests 
    PRIVATE 
    pyfolio_cpp
    gtest
    gtest_main
    Threads::Threads
)
target_compile_features(memory_pool_tests PRIVATE cxx_std_20)
add_test(NAME memory_pool_tests COMMAND memory_pool_tests)

# Simple memory pool tests (backup implementation)
add_executable(simple_memory_pool_tests test_simple_memory_pool.cpp)
target_link_libraries(simple_memory_pool_tests 
    PRIVATE 
    pyfolio_cpp
    gtest
    gtest_main
    Threads::Threads
)
target_compile_features(simple_memory_pool_tests PRIVATE cxx_std_20)
add_test(NAME simple_memory_pool_tests COMMAND simple_memory_pool_tests)

# Test data files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test_data/sample_returns.csv 
               ${CMAKE_CURRENT_BINARY_DIR}/test_data/sample_returns.csv COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test_data/sample_transactions.csv 
               ${CMAKE_CURRENT_BINARY_DIR}/test_data/sample_transactions.csv COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test_data/sample_positions.csv 
               ${CMAKE_CURRENT_BINARY_DIR}/test_data/sample_positions.csv COPYONLY)

# Python equivalence test data files (from original pyfolio tests)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test_data/test_returns.csv.gz 
               ${CMAKE_CURRENT_BINARY_DIR}/test_data/test_returns.csv.gz COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test_data/test_pos.csv.gz 
               ${CMAKE_CURRENT_BINARY_DIR}/test_data/test_pos.csv.gz COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test_data/test_txn.csv.gz 
               ${CMAKE_CURRENT_BINARY_DIR}/test_data/test_txn.csv.gz COPYONLY)

# Add custom target for Python equivalence testing
add_custom_target(test_python_equivalence
    COMMAND test_pyfolio_equivalent_only
    COMMAND run_python_equivalent_tests
    DEPENDS test_pyfolio_equivalent_only run_python_equivalent_tests
    COMMENT "Running Python pyfolio equivalence tests"
)

# Print additional test configuration
message(STATUS "")
message(STATUS "Python Equivalence Tests:")
message(STATUS "  test_pyfolio_equivalent_only: Individual test cases")
message(STATUS "  run_python_equivalent_tests: Comprehensive validation")
message(STATUS "  Run with: make test_python_equivalence")
message(STATUS "")
# Simple REST API tests
add_executable(test_simple_rest_api test_simple_rest_api.cpp)
target_include_directories(test_simple_rest_api PRIVATE 
    $<TARGET_PROPERTY:nlohmann_json::nlohmann_json,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:httplib::httplib,INTERFACE_INCLUDE_DIRECTORIES>
)
target_link_libraries(test_simple_rest_api gtest_main)
target_compile_features(test_simple_rest_api PRIVATE cxx_std_20)
add_test(NAME test_simple_rest_api COMMAND test_simple_rest_api)