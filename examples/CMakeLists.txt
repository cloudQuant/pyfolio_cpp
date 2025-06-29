add_executable(basic_example basic_example.cpp)
target_link_libraries(basic_example pyfolio_cpp)
target_compile_features(basic_example PRIVATE cxx_std_20)

add_executable(transaction_analysis_example transaction_analysis_example.cpp)
target_link_libraries(transaction_analysis_example pyfolio_cpp)
target_compile_features(transaction_analysis_example PRIVATE cxx_std_20)

add_executable(attribution_example attribution_example.cpp)
target_link_libraries(attribution_example pyfolio_cpp)
target_compile_features(attribution_example PRIVATE cxx_std_20)

add_executable(risk_analysis_example risk_analysis_example.cpp)
target_link_libraries(risk_analysis_example pyfolio_cpp)
target_compile_features(risk_analysis_example PRIVATE cxx_std_20)

add_executable(capacity_analysis_example capacity_analysis_example.cpp)
target_link_libraries(capacity_analysis_example pyfolio_cpp)
target_compile_features(capacity_analysis_example PRIVATE cxx_std_20)

add_executable(bayesian_analysis_example bayesian_analysis_example.cpp)
target_link_libraries(bayesian_analysis_example pyfolio_cpp)
target_compile_features(bayesian_analysis_example PRIVATE cxx_std_20)

# REST API example (complex - currently has dependency issues)
# add_executable(rest_api_example rest_api_example.cpp)
# target_link_libraries(rest_api_example pyfolio_cpp)
# target_compile_features(rest_api_example PRIVATE cxx_std_20)

# Simple REST API example (working)
add_executable(simple_rest_api_example simple_rest_api_example.cpp)
target_include_directories(simple_rest_api_example PRIVATE 
    $<TARGET_PROPERTY:nlohmann_json::nlohmann_json,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:httplib::httplib,INTERFACE_INCLUDE_DIRECTORIES>
)
target_compile_features(simple_rest_api_example PRIVATE cxx_std_20)

# Enhanced Plotly visualization example
add_executable(plotly_enhanced_example plotly_enhanced_example.cpp)
target_link_libraries(plotly_enhanced_example pyfolio_cpp)
target_compile_features(plotly_enhanced_example PRIVATE cxx_std_20)

# Machine Learning Regime Detection example
add_executable(ml_regime_detection_example ml_regime_detection_example.cpp)
target_link_libraries(ml_regime_detection_example pyfolio_cpp)
target_compile_features(ml_regime_detection_example PRIVATE cxx_std_20)

# Real-time Streaming Analysis example
add_executable(streaming_analysis_example streaming_analysis_example.cpp)
target_link_libraries(streaming_analysis_example pyfolio_cpp)
target_compile_features(streaming_analysis_example PRIVATE cxx_std_20)

# Quick streaming demo
add_executable(streaming_demo_quick streaming_demo_quick.cpp)
target_link_libraries(streaming_demo_quick pyfolio_cpp)
target_compile_features(streaming_demo_quick PRIVATE cxx_std_20)

# GPU acceleration example
add_executable(gpu_acceleration_example gpu_acceleration_example.cpp)
target_link_libraries(gpu_acceleration_example pyfolio_cpp)
target_compile_features(gpu_acceleration_example PRIVATE cxx_std_20)

# Add CUDA support if available
if(CUDA_FOUND OR CMAKE_CUDA_COMPILER)
    target_compile_definitions(gpu_acceleration_example PRIVATE PYFOLIO_HAS_CUDA=1)
    if(TARGET CUDA::cudart)
        target_link_libraries(gpu_acceleration_example CUDA::cudart CUDA::cublas CUDA::curand)
    endif()
endif()

# Add OpenCL support if available
if(OpenCL_FOUND)
    target_compile_definitions(gpu_acceleration_example PRIVATE PYFOLIO_HAS_OPENCL=1)
    target_link_libraries(gpu_acceleration_example ${OpenCL_LIBRARIES})
    target_include_directories(gpu_acceleration_example PRIVATE ${OpenCL_INCLUDE_DIRS})
endif()

# Simple backtesting example
add_executable(simple_backtesting_example simple_backtesting_example.cpp)
target_link_libraries(simple_backtesting_example pyfolio_cpp)
target_compile_features(simple_backtesting_example PRIVATE cxx_std_20)

# Options pricing example
add_executable(options_pricing_example options_pricing_example.cpp)
target_link_libraries(options_pricing_example pyfolio_cpp)
target_compile_features(options_pricing_example PRIVATE cxx_std_20)

# Advanced risk models example
add_executable(advanced_risk_example advanced_risk_example.cpp)
target_link_libraries(advanced_risk_example pyfolio_cpp)
target_compile_features(advanced_risk_example PRIVATE cxx_std_20)

# ML model persistence example
add_executable(ml_persistence_example ml_persistence_example.cpp)
target_link_libraries(ml_persistence_example pyfolio_cpp)
target_compile_features(ml_persistence_example PRIVATE cxx_std_20)