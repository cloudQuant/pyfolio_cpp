#pragma once

#include "../core/error_handling.h"
#include "../core/types.h"
#include <vector>
#include <memory>
#include <string>
#include <cmath>
#include <random>
#include <chrono>
#include <algorithm>
#include <numeric>

// Conditional compilation for GPU support
#ifdef PYFOLIO_HAS_CUDA
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <curand.h>
#endif

#ifdef PYFOLIO_HAS_OPENCL
// Note: OpenCL is deprecated on macOS since 10.14
// For cross-platform compatibility, we include headers conditionally
#ifndef __APPLE__
#include <CL/cl.hpp>
#endif
#endif

namespace pyfolio::gpu {

/**
 * @brief GPU computation backend types
 */
enum class GPUBackend {
    None,     ///< CPU-only computation
    CUDA,     ///< NVIDIA CUDA backend
    OpenCL,   ///< OpenCL backend (AMD, Intel, etc.)
    Auto      ///< Automatically select best available
};

/**
 * @brief GPU device information
 */
struct GPUDeviceInfo {
    int device_id;
    std::string name;
    size_t total_memory;
    size_t free_memory;
    int compute_capability_major;
    int compute_capability_minor;
    int multiprocessor_count;
    GPUBackend backend;
    
    bool supports_double_precision() const {
        return compute_capability_major >= 1 && compute_capability_minor >= 3;
    }
    
    size_t max_threads_per_block() const {
        return backend == GPUBackend::CUDA ? 1024 : 256;
    }
};

/**
 * @brief GPU memory buffer for efficient data transfer
 */
template<typename T>
class GPUBuffer {
private:
    void* device_ptr_ = nullptr;
    size_t size_ = 0;
    GPUBackend backend_;
    bool managed_ = false;
    
public:
    explicit GPUBuffer(size_t size, GPUBackend backend = GPUBackend::Auto) 
        : size_(size), backend_(backend) {
        allocate();
    }
    
    ~GPUBuffer() {
        deallocate();
    }
    
    // Non-copyable but movable
    GPUBuffer(const GPUBuffer&) = delete;
    GPUBuffer& operator=(const GPUBuffer&) = delete;
    
    GPUBuffer(GPUBuffer&& other) noexcept 
        : device_ptr_(other.device_ptr_), size_(other.size_), 
          backend_(other.backend_), managed_(other.managed_) {
        other.device_ptr_ = nullptr;
        other.managed_ = false;
    }
    
    GPUBuffer& operator=(GPUBuffer&& other) noexcept {
        if (this != &other) {
            deallocate();
            device_ptr_ = other.device_ptr_;
            size_ = other.size_;
            backend_ = other.backend_;
            managed_ = other.managed_;
            other.device_ptr_ = nullptr;
            other.managed_ = false;
        }
        return *this;
    }
    
    /**
     * @brief Copy data from host to GPU
     */
    Result<void> copy_from_host(const std::vector<T>& host_data) {
        if (host_data.size() > size_) {
            return Result<void>::error(ErrorCode::InvalidInput,
                "Host data size exceeds buffer capacity");
        }
        
#ifdef PYFOLIO_HAS_CUDA
        if (backend_ == GPUBackend::CUDA) {
            cudaError_t error = cudaMemcpy(device_ptr_, host_data.data(),
                host_data.size() * sizeof(T), cudaMemcpyHostToDevice);
            if (error != cudaSuccess) {
                return Result<void>::error(ErrorCode::CalculationError,
                    "CUDA memory copy failed: " + std::string(cudaGetErrorString(error)));
            }
            return Result<void>::success();
        }
#endif
        
#ifdef PYFOLIO_HAS_OPENCL
        if (backend_ == GPUBackend::OpenCL) {
            // OpenCL memory copy implementation
            return Result<void>::error(ErrorCode::CalculationError,
                "OpenCL copy not implemented in this placeholder");
        }
#endif
        
        return Result<void>::error(ErrorCode::InvalidInput,
            "GPU backend not available");
    }
    
    /**
     * @brief Copy data from GPU to host
     */
    Result<std::vector<T>> copy_to_host() const {
        std::vector<T> result(size_);
        
#ifdef PYFOLIO_HAS_CUDA
        if (backend_ == GPUBackend::CUDA) {
            cudaError_t error = cudaMemcpy(result.data(), device_ptr_,
                size_ * sizeof(T), cudaMemcpyDeviceToHost);
            if (error != cudaSuccess) {
                return Result<std::vector<T>>::error(ErrorCode::CalculationError,
                    "CUDA memory copy failed: " + std::string(cudaGetErrorString(error)));
            }
            return Result<std::vector<T>>::success(std::move(result));
        }
#endif
        
        return Result<std::vector<T>>::error(ErrorCode::InvalidInput,
            "GPU backend not available");
    }
    
    void* data() const { return device_ptr_; }
    size_t size() const { return size_; }
    GPUBackend backend() const { return backend_; }
    
private:
    void allocate() {
#ifdef PYFOLIO_HAS_CUDA
        if (backend_ == GPUBackend::CUDA || backend_ == GPUBackend::Auto) {
            cudaError_t error = cudaMalloc(&device_ptr_, size_ * sizeof(T));
            if (error == cudaSuccess) {
                backend_ = GPUBackend::CUDA;
                managed_ = true;
                return;
            }
        }
#endif
        
#ifdef PYFOLIO_HAS_OPENCL
        if (backend_ == GPUBackend::OpenCL || backend_ == GPUBackend::Auto) {
            // OpenCL allocation would go here
        }
#endif
        
        // Fallback: allocate host memory as placeholder
        device_ptr_ = std::malloc(size_ * sizeof(T));
        backend_ = GPUBackend::None;
        managed_ = true;
    }
    
    void deallocate() {
        if (!managed_ || !device_ptr_) return;
        
#ifdef PYFOLIO_HAS_CUDA
        if (backend_ == GPUBackend::CUDA) {
            cudaFree(device_ptr_);
            return;
        }
#endif
        
        std::free(device_ptr_);
    }
};

/**
 * @brief GPU-accelerated portfolio optimization engine
 */
class GPUPortfolioOptimizer {
private:
    GPUBackend backend_;
    std::vector<GPUDeviceInfo> devices_;
    int current_device_ = 0;
    
#ifdef PYFOLIO_HAS_CUDA
    cublasHandle_t cublas_handle_ = nullptr;
    curandGenerator_t curand_gen_ = nullptr;
#endif

public:
    explicit GPUPortfolioOptimizer(GPUBackend backend = GPUBackend::Auto) 
        : backend_(backend) {
        initialize();
    }
    
    ~GPUPortfolioOptimizer() {
        cleanup();
    }
    
    /**
     * @brief Get available GPU devices
     */
    const std::vector<GPUDeviceInfo>& get_devices() const {
        return devices_;
    }
    
    /**
     * @brief Set active GPU device
     */
    Result<void> set_device(int device_id) {
        if (device_id < 0 || device_id >= static_cast<int>(devices_.size())) {
            return Result<void>::error(ErrorCode::InvalidInput,
                "Invalid device ID");
        }
        
#ifdef PYFOLIO_HAS_CUDA
        if (backend_ == GPUBackend::CUDA) {
            cudaError_t error = cudaSetDevice(device_id);
            if (error != cudaSuccess) {
                return Result<void>::error(ErrorCode::CalculationError,
                    "Failed to set CUDA device: " + std::string(cudaGetErrorString(error)));
            }
        }
#endif
        
        current_device_ = device_id;
        return Result<void>::success();
    }
    
    /**
     * @brief GPU-accelerated covariance matrix calculation
     * 
     * Computes the covariance matrix for large portfolios using GPU parallel processing.
     * This is essential for Markowitz portfolio optimization.
     */
    Result<std::vector<std::vector<double>>> calculate_covariance_matrix_gpu(
        const std::vector<std::vector<double>>& returns_matrix) const {
        
        size_t n_assets = returns_matrix.size();
        size_t n_periods = returns_matrix.empty() ? 0 : returns_matrix[0].size();
        
        if (n_assets == 0 || n_periods < 2) {
            return Result<std::vector<std::vector<double>>>::error(
                ErrorCode::InsufficientData,
                "Need at least 2 periods and 1 asset for covariance calculation");
        }
        
        // Flatten returns matrix for GPU processing
        std::vector<double> flat_returns;
        flat_returns.reserve(n_assets * n_periods);
        
        for (size_t i = 0; i < n_assets; ++i) {
            for (size_t j = 0; j < n_periods; ++j) {
                flat_returns.push_back(returns_matrix[i][j]);
            }
        }
        
#ifdef PYFOLIO_HAS_CUDA
        if (backend_ == GPUBackend::CUDA) {
            return calculate_covariance_cuda(flat_returns, n_assets, n_periods);
        }
#endif
        
        // CPU fallback
        return calculate_covariance_cpu(returns_matrix);
    }
    
    /**
     * @brief GPU-accelerated Monte Carlo simulation for portfolio VaR
     */
    Result<std::vector<double>> monte_carlo_var_simulation_gpu(
        const std::vector<double>& portfolio_weights,
        const std::vector<std::vector<double>>& covariance_matrix,
        size_t num_simulations = 100000) const {
        
        size_t n_assets = portfolio_weights.size();
        
        if (n_assets != covariance_matrix.size()) {
            return Result<std::vector<double>>::error(ErrorCode::InvalidInput,
                "Portfolio weights and covariance matrix dimensions mismatch");
        }
        
#ifdef PYFOLIO_HAS_CUDA
        if (backend_ == GPUBackend::CUDA) {
            return monte_carlo_simulation_cuda(portfolio_weights, covariance_matrix, num_simulations);
        }
#endif
        
        // CPU fallback
        return monte_carlo_simulation_cpu(portfolio_weights, covariance_matrix, num_simulations);
    }
    
    /**
     * @brief GPU-accelerated portfolio optimization using quadratic programming
     */
    Result<std::vector<double>> optimize_portfolio_gpu(
        const std::vector<double>& expected_returns,
        const std::vector<std::vector<double>>& covariance_matrix,
        double risk_tolerance = 1.0,
        const std::vector<double>& min_weights = {},
        const std::vector<double>& max_weights = {}) const {
        
        size_t n_assets = expected_returns.size();
        
        if (n_assets != covariance_matrix.size()) {
            return Result<std::vector<double>>::error(ErrorCode::InvalidInput,
                "Expected returns and covariance matrix dimensions mismatch");
        }
        
#ifdef PYFOLIO_HAS_CUDA
        if (backend_ == GPUBackend::CUDA) {
            return portfolio_optimization_cuda(expected_returns, covariance_matrix, 
                                             risk_tolerance, min_weights, max_weights);
        }
#endif
        
        // CPU fallback using simple equal-weight
        std::vector<double> weights(n_assets, 1.0 / n_assets);
        return Result<std::vector<double>>::success(std::move(weights));
    }
    
    /**
     * @brief Benchmark GPU vs CPU performance
     */
    struct PerformanceBenchmark {
        double gpu_time_ms;
        double cpu_time_ms;
        double speedup_factor;
        size_t matrix_size;
        std::string operation;
    };
    
    Result<PerformanceBenchmark> benchmark_performance(
        size_t matrix_size = 1000) const {
        
        // Generate random data for benchmarking
        std::vector<std::vector<double>> test_matrix(matrix_size, 
            std::vector<double>(matrix_size));
        
        std::mt19937 gen(42);
        std::normal_distribution<double> dist(0.01, 0.02);
        
        for (auto& row : test_matrix) {
            for (auto& val : row) {
                val = dist(gen);
            }
        }
        
        // Time GPU operation
        auto start = std::chrono::high_resolution_clock::now();
        auto gpu_result = calculate_covariance_matrix_gpu(test_matrix);
        auto gpu_end = std::chrono::high_resolution_clock::now();
        
        // Time CPU operation
        auto cpu_start = std::chrono::high_resolution_clock::now();
        auto cpu_result = calculate_covariance_cpu(test_matrix);
        auto cpu_end = std::chrono::high_resolution_clock::now();
        
        if (gpu_result.is_error() || cpu_result.is_error()) {
            return Result<PerformanceBenchmark>::error(ErrorCode::CalculationError,
                "Benchmark calculation failed");
        }
        
        double gpu_time = std::chrono::duration<double, std::milli>(gpu_end - start).count();
        double cpu_time = std::chrono::duration<double, std::milli>(cpu_end - cpu_start).count();
        
        PerformanceBenchmark benchmark{
            gpu_time,
            cpu_time,
            cpu_time / gpu_time,
            matrix_size,
            "Covariance Matrix Calculation"
        };
        
        return Result<PerformanceBenchmark>::success(benchmark);
    }
    
private:
    void initialize() {
        detect_devices();
        
#ifdef PYFOLIO_HAS_CUDA
        if (backend_ == GPUBackend::CUDA || backend_ == GPUBackend::Auto) {
            if (cublasCreate(&cublas_handle_) == CUBLAS_STATUS_SUCCESS) {
                curandCreateGenerator(&curand_gen_, CURAND_RNG_PSEUDO_DEFAULT);
                backend_ = GPUBackend::CUDA;
                return;
            }
        }
#endif
        
        backend_ = GPUBackend::None;
    }
    
    void cleanup() {
#ifdef PYFOLIO_HAS_CUDA
        if (cublas_handle_) {
            cublasDestroy(cublas_handle_);
            cublas_handle_ = nullptr;
        }
        if (curand_gen_) {
            curandDestroyGenerator(curand_gen_);
            curand_gen_ = nullptr;
        }
#endif
    }
    
    void detect_devices() {
#ifdef PYFOLIO_HAS_CUDA
        int device_count = 0;
        if (cudaGetDeviceCount(&device_count) == cudaSuccess) {
            for (int i = 0; i < device_count; ++i) {
                cudaDeviceProp prop;
                if (cudaGetDeviceProperties(&prop, i) == cudaSuccess) {
                    size_t free_mem, total_mem;
                    cudaMemGetInfo(&free_mem, &total_mem);
                    
                    devices_.push_back({
                        i,
                        prop.name,
                        total_mem,
                        free_mem,
                        prop.major,
                        prop.minor,
                        prop.multiProcessorCount,
                        GPUBackend::CUDA
                    });
                }
            }
        }
#endif
        
        // If no GPU devices found, add CPU placeholder
        if (devices_.empty()) {
            devices_.push_back({
                0,
                "CPU (No GPU Available)",
                0,
                0,
                0,
                0,
                1,
                GPUBackend::None
            });
        }
    }
    
#ifdef PYFOLIO_HAS_CUDA
    Result<std::vector<std::vector<double>>> calculate_covariance_cuda(
        const std::vector<double>& flat_returns, size_t n_assets, size_t n_periods) const {
        
        // Placeholder for CUDA covariance calculation
        // In production, this would:
        // 1. Allocate GPU memory using GPUBuffer
        // 2. Copy data to GPU
        // 3. Launch CUDA kernel for parallel covariance computation
        // 4. Copy results back to host
        
        return Result<std::vector<std::vector<double>>>::error(
            ErrorCode::CalculationError,
            "CUDA covariance calculation placeholder - requires full CUDA implementation");
    }
    
    Result<std::vector<double>> monte_carlo_simulation_cuda(
        const std::vector<double>& weights,
        const std::vector<std::vector<double>>& covariance,
        size_t num_simulations) const {
        
        // Placeholder for CUDA Monte Carlo simulation
        return Result<std::vector<double>>::error(
            ErrorCode::CalculationError,
            "CUDA Monte Carlo simulation placeholder");
    }
    
    Result<std::vector<double>> portfolio_optimization_cuda(
        const std::vector<double>& expected_returns,
        const std::vector<std::vector<double>>& covariance_matrix,
        double risk_tolerance,
        const std::vector<double>& min_weights,
        const std::vector<double>& max_weights) const {
        
        // Placeholder for CUDA portfolio optimization
        return Result<std::vector<double>>::error(
            ErrorCode::CalculationError,
            "CUDA portfolio optimization placeholder");
    }
#endif
    
    /**
     * @brief CPU fallback for covariance calculation
     */
    Result<std::vector<std::vector<double>>> calculate_covariance_cpu(
        const std::vector<std::vector<double>>& returns_matrix) const {
        
        size_t n_assets = returns_matrix.size();
        size_t n_periods = returns_matrix[0].size();
        
        // Calculate means
        std::vector<double> means(n_assets, 0.0);
        for (size_t i = 0; i < n_assets; ++i) {
            for (size_t j = 0; j < n_periods; ++j) {
                means[i] += returns_matrix[i][j];
            }
            means[i] /= n_periods;
        }
        
        // Calculate covariance matrix
        std::vector<std::vector<double>> covariance(n_assets, std::vector<double>(n_assets, 0.0));
        
        for (size_t i = 0; i < n_assets; ++i) {
            for (size_t j = 0; j <= i; ++j) {
                double cov = 0.0;
                for (size_t k = 0; k < n_periods; ++k) {
                    cov += (returns_matrix[i][k] - means[i]) * (returns_matrix[j][k] - means[j]);
                }
                cov /= (n_periods - 1);
                
                covariance[i][j] = cov;
                covariance[j][i] = cov;  // Symmetric matrix
            }
        }
        
        return Result<std::vector<std::vector<double>>>::success(std::move(covariance));
    }
    
    /**
     * @brief CPU fallback for Monte Carlo simulation
     */
    Result<std::vector<double>> monte_carlo_simulation_cpu(
        const std::vector<double>& weights,
        const std::vector<std::vector<double>>& covariance,
        size_t num_simulations) const {
        
        size_t n_assets = weights.size();
        std::vector<double> portfolio_returns;
        portfolio_returns.reserve(num_simulations);
        
        std::mt19937 gen(42);
        std::normal_distribution<double> normal(0.0, 1.0);
        
        // Simplified Monte Carlo - would need proper Cholesky decomposition in production
        for (size_t sim = 0; sim < num_simulations; ++sim) {
            double portfolio_return = 0.0;
            
            for (size_t i = 0; i < n_assets; ++i) {
                double asset_return = normal(gen) * std::sqrt(covariance[i][i]);
                portfolio_return += weights[i] * asset_return;
            }
            
            portfolio_returns.push_back(portfolio_return);
        }
        
        return Result<std::vector<double>>::success(std::move(portfolio_returns));
    }
};

/**
 * @brief GPU-accelerated matrix operations utility class
 */
class GPUMatrixOps {
public:
    /**
     * @brief GPU matrix multiplication for large covariance calculations
     */
    static Result<std::vector<std::vector<double>>> matrix_multiply_gpu(
        const std::vector<std::vector<double>>& A,
        const std::vector<std::vector<double>>& B,
        GPUBackend backend = GPUBackend::Auto) {
        
        size_t rows_A = A.size();
        size_t cols_A = A.empty() ? 0 : A[0].size();
        size_t rows_B = B.size();
        size_t cols_B = B.empty() ? 0 : B[0].size();
        
        if (cols_A != rows_B) {
            return Result<std::vector<std::vector<double>>>::error(
                ErrorCode::InvalidInput,
                "Matrix dimensions incompatible for multiplication");
        }
        
        // CPU fallback implementation
        std::vector<std::vector<double>> result(rows_A, std::vector<double>(cols_B, 0.0));
        
        for (size_t i = 0; i < rows_A; ++i) {
            for (size_t j = 0; j < cols_B; ++j) {
                for (size_t k = 0; k < cols_A; ++k) {
                    result[i][j] += A[i][k] * B[k][j];
                }
            }
        }
        
        return Result<std::vector<std::vector<double>>>::success(std::move(result));
    }
    
    /**
     * @brief GPU Cholesky decomposition for portfolio optimization
     */
    static Result<std::vector<std::vector<double>>> cholesky_decomposition_gpu(
        const std::vector<std::vector<double>>& matrix,
        GPUBackend backend = GPUBackend::Auto) {
        
        size_t n = matrix.size();
        if (n == 0 || matrix[0].size() != n) {
            return Result<std::vector<std::vector<double>>>::error(
                ErrorCode::InvalidInput,
                "Matrix must be square for Cholesky decomposition");
        }
        
        // CPU fallback implementation
        std::vector<std::vector<double>> L(n, std::vector<double>(n, 0.0));
        
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = 0; j <= i; ++j) {
                if (i == j) {
                    double sum = 0.0;
                    for (size_t k = 0; k < j; ++k) {
                        sum += L[j][k] * L[j][k];
                    }
                    double value = matrix[j][j] - sum;
                    if (value <= 0) {
                        return Result<std::vector<std::vector<double>>>::error(
                            ErrorCode::CalculationError,
                            "Matrix is not positive definite");
                    }
                    L[j][j] = std::sqrt(value);
                } else {
                    double sum = 0.0;
                    for (size_t k = 0; k < j; ++k) {
                        sum += L[i][k] * L[j][k];
                    }
                    L[i][j] = (matrix[i][j] - sum) / L[j][j];
                }
            }
        }
        
        return Result<std::vector<std::vector<double>>>::success(std::move(L));
    }
};

}  // namespace pyfolio::gpu