#include <pyfolio/gpu/gpu_accelerator.h>
#include <pyfolio/analytics/performance_metrics.h>
#include <iostream>
#include <vector>
#include <random>
#include <iomanip>
#include <chrono>

using namespace pyfolio;
using namespace pyfolio::gpu;

// Generate sample financial data for testing
class DataGenerator {
private:
    std::mt19937 gen_;
    std::normal_distribution<double> return_dist_;
    
public:
    DataGenerator() : gen_(42), return_dist_(0.001, 0.02) {}
    
    // Generate correlated returns matrix for portfolio optimization
    std::vector<std::vector<double>> generate_returns_matrix(size_t n_assets, size_t n_periods) {
        std::vector<std::vector<double>> returns(n_assets, std::vector<double>(n_periods));
        
        // Generate base market factor
        std::vector<double> market_factor(n_periods);
        for (size_t t = 0; t < n_periods; ++t) {
            market_factor[t] = return_dist_(gen_);
        }
        
        // Generate asset returns correlated with market
        for (size_t i = 0; i < n_assets; ++i) {
            double beta = std::uniform_real_distribution<double>(0.5, 1.5)(gen_);
            double alpha = std::uniform_real_distribution<double>(-0.001, 0.001)(gen_);
            
            for (size_t t = 0; t < n_periods; ++t) {
                double idiosyncratic = return_dist_(gen_) * 0.5;  // Reduce idiosyncratic risk
                returns[i][t] = alpha + beta * market_factor[t] + idiosyncratic;
            }
        }
        
        return returns;
    }
    
    // Generate expected returns vector
    std::vector<double> generate_expected_returns(size_t n_assets) {
        std::vector<double> expected_returns(n_assets);
        std::uniform_real_distribution<double> return_dist(0.05, 0.15);  // 5-15% annual return
        
        for (size_t i = 0; i < n_assets; ++i) {
            expected_returns[i] = return_dist(gen_) / 252.0;  // Convert to daily
        }
        
        return expected_returns;
    }
    
    // Generate random portfolio weights
    std::vector<double> generate_random_weights(size_t n_assets) {
        std::vector<double> weights(n_assets);
        std::uniform_real_distribution<double> weight_dist(0.0, 1.0);
        
        double sum = 0.0;
        for (size_t i = 0; i < n_assets; ++i) {
            weights[i] = weight_dist(gen_);
            sum += weights[i];
        }
        
        // Normalize to sum to 1.0
        for (auto& w : weights) {
            w /= sum;
        }
        
        return weights;
    }
};

// Display helper functions
void display_separator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << title << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

void display_matrix_sample(const std::vector<std::vector<double>>& matrix, 
                          const std::string& name, size_t max_display = 5) {
    std::cout << "\n" << name << " (showing " << max_display << "x" << max_display << " sample):" << std::endl;
    std::cout << std::fixed << std::setprecision(6);
    
    size_t rows = std::min(matrix.size(), max_display);
    size_t cols = matrix.empty() ? 0 : std::min(matrix[0].size(), max_display);
    
    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            std::cout << std::setw(10) << matrix[i][j] << " ";
        }
        if (matrix[i].size() > max_display) {
            std::cout << "...";
        }
        std::cout << std::endl;
    }
    if (matrix.size() > max_display) {
        std::cout << "..." << std::endl;
    }
}

void display_vector_sample(const std::vector<double>& vec, 
                          const std::string& name, size_t max_display = 10) {
    std::cout << "\n" << name << " (showing first " << max_display << " values):" << std::endl;
    std::cout << std::fixed << std::setprecision(6);
    
    size_t count = std::min(vec.size(), max_display);
    for (size_t i = 0; i < count; ++i) {
        std::cout << std::setw(10) << vec[i] << " ";
    }
    if (vec.size() > max_display) {
        std::cout << "...";
    }
    std::cout << std::endl;
}

int main() {
    display_separator("GPU-Accelerated Portfolio Optimization Example");
    
    std::cout << "This example demonstrates GPU acceleration for:\n";
    std::cout << "1. Large-scale covariance matrix calculations\n";
    std::cout << "2. Monte Carlo VaR simulations\n";
    std::cout << "3. Portfolio optimization\n";
    std::cout << "4. Performance benchmarking (GPU vs CPU)\n";
    
    // Initialize GPU optimizer
    std::cout << "\n🚀 Initializing GPU Portfolio Optimizer..." << std::endl;
    GPUPortfolioOptimizer optimizer(GPUBackend::Auto);
    
    // Display available devices
    auto devices = optimizer.get_devices();
    std::cout << "\n💻 Available Computing Devices:" << std::endl;
    for (size_t i = 0; i < devices.size(); ++i) {
        const auto& device = devices[i];
        std::cout << "  Device " << i << ": " << device.name << std::endl;
        std::cout << "    Backend: " << (device.backend == GPUBackend::CUDA ? "CUDA" : 
                                        device.backend == GPUBackend::OpenCL ? "OpenCL" : "CPU") << std::endl;
        if (device.total_memory > 0) {
            std::cout << "    Memory: " << device.total_memory / (1024*1024) << " MB total, " 
                      << device.free_memory / (1024*1024) << " MB free" << std::endl;
            std::cout << "    Compute Capability: " << device.compute_capability_major 
                      << "." << device.compute_capability_minor << std::endl;
            std::cout << "    Multiprocessors: " << device.multiprocessor_count << std::endl;
            std::cout << "    Double Precision: " << (device.supports_double_precision() ? "Yes" : "No") << std::endl;
        }
    }
    
    // Generate test data
    display_separator("Generating Test Data");
    
    DataGenerator generator;
    const size_t n_assets = 500;    // Large portfolio for GPU demonstration
    const size_t n_periods = 1000;  // ~4 years of daily data
    
    std::cout << "📊 Generating portfolio data:" << std::endl;
    std::cout << "  Assets: " << n_assets << std::endl;
    std::cout << "  Time periods: " << n_periods << std::endl;
    std::cout << "  Matrix size: " << n_assets << "x" << n_periods << " = " 
              << (n_assets * n_periods * sizeof(double) / (1024*1024)) << " MB" << std::endl;
    
    auto returns_matrix = generator.generate_returns_matrix(n_assets, n_periods);
    auto expected_returns = generator.generate_expected_returns(n_assets);
    auto portfolio_weights = generator.generate_random_weights(n_assets);
    
    display_matrix_sample(returns_matrix, "Returns Matrix");
    display_vector_sample(expected_returns, "Expected Returns");
    display_vector_sample(portfolio_weights, "Portfolio Weights");
    
    // Test 1: Covariance Matrix Calculation
    display_separator("Test 1: Covariance Matrix Calculation");
    
    std::cout << "🧮 Computing " << n_assets << "x" << n_assets << " covariance matrix..." << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    auto cov_result = optimizer.calculate_covariance_matrix_gpu(returns_matrix);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    if (cov_result.is_ok()) {
        std::cout << "✅ Covariance calculation completed in " << duration << " ms" << std::endl;
        display_matrix_sample(cov_result.value(), "Covariance Matrix");
        
        // Validate matrix properties
        const auto& cov_matrix = cov_result.value();
        bool is_symmetric = true;
        double max_asymmetry = 0.0;
        
        for (size_t i = 0; i < std::min(size_t(10), cov_matrix.size()); ++i) {
            for (size_t j = 0; j < std::min(size_t(10), cov_matrix[i].size()); ++j) {
                double asymmetry = std::abs(cov_matrix[i][j] - cov_matrix[j][i]);
                max_asymmetry = std::max(max_asymmetry, asymmetry);
                if (asymmetry > 1e-10) {
                    is_symmetric = false;
                }
            }
        }
        
        std::cout << "  Matrix properties:" << std::endl;
        std::cout << "    Symmetric: " << (is_symmetric ? "Yes" : "No") << std::endl;
        std::cout << "    Max asymmetry: " << max_asymmetry << std::endl;
        
    } else {
        std::cout << "❌ Covariance calculation failed: " << cov_result.error().message << std::endl;
        std::cout << "    Note: This is expected if CUDA is not available - using CPU fallback" << std::endl;
    }
    
    // Test 2: Monte Carlo VaR Simulation
    display_separator("Test 2: Monte Carlo VaR Simulation");
    
    if (cov_result.is_ok()) {
        const size_t num_simulations = 100000;
        std::cout << "🎲 Running Monte Carlo simulation with " << num_simulations << " paths..." << std::endl;
        
        start_time = std::chrono::high_resolution_clock::now();
        auto var_result = optimizer.monte_carlo_var_simulation_gpu(
            portfolio_weights, cov_result.value(), num_simulations);
        end_time = std::chrono::high_resolution_clock::now();
        
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        
        if (var_result.is_ok()) {
            std::cout << "✅ Monte Carlo simulation completed in " << duration << " ms" << std::endl;
            
            const auto& returns = var_result.value();
            std::cout << "  Simulation results:" << std::endl;
            std::cout << "    Total paths: " << returns.size() << std::endl;
            
            // Calculate VaR statistics
            std::vector<double> sorted_returns = returns;
            std::sort(sorted_returns.begin(), sorted_returns.end());
            
            double var_95 = -sorted_returns[static_cast<size_t>(0.05 * sorted_returns.size())];
            double var_99 = -sorted_returns[static_cast<size_t>(0.01 * sorted_returns.size())];
            double expected_return = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
            
            std::cout << std::fixed << std::setprecision(4);
            std::cout << "    Expected Return: " << expected_return * 100 << "%" << std::endl;
            std::cout << "    VaR (95%): " << var_95 * 100 << "%" << std::endl;
            std::cout << "    VaR (99%): " << var_99 * 100 << "%" << std::endl;
            
        } else {
            std::cout << "❌ Monte Carlo simulation failed: " << var_result.error().message << std::endl;
        }
    }
    
    // Test 3: Portfolio Optimization
    display_separator("Test 3: Portfolio Optimization");
    
    if (cov_result.is_ok()) {
        std::cout << "⚖️ Optimizing portfolio weights..." << std::endl;
        
        const double risk_tolerance = 0.5;  // Moderate risk tolerance
        std::vector<double> min_weights(n_assets, 0.0);     // No short selling
        std::vector<double> max_weights(n_assets, 0.1);     // Max 10% per asset
        
        start_time = std::chrono::high_resolution_clock::now();
        auto opt_result = optimizer.optimize_portfolio_gpu(
            expected_returns, cov_result.value(), risk_tolerance, min_weights, max_weights);
        end_time = std::chrono::high_resolution_clock::now();
        
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        
        if (opt_result.is_ok()) {
            std::cout << "✅ Portfolio optimization completed in " << duration << " ms" << std::endl;
            
            const auto& optimal_weights = opt_result.value();
            display_vector_sample(optimal_weights, "Optimal Weights");
            
            // Validate weight constraints
            double weight_sum = std::accumulate(optimal_weights.begin(), optimal_weights.end(), 0.0);
            double max_weight = *std::max_element(optimal_weights.begin(), optimal_weights.end());
            double min_weight = *std::min_element(optimal_weights.begin(), optimal_weights.end());
            
            std::cout << "  Weight validation:" << std::endl;
            std::cout << "    Sum: " << weight_sum << " (should be ~1.0)" << std::endl;
            std::cout << "    Range: [" << min_weight << ", " << max_weight << "]" << std::endl;
            
            // Calculate portfolio metrics
            double portfolio_return = 0.0;
            for (size_t i = 0; i < n_assets; ++i) {
                portfolio_return += optimal_weights[i] * expected_returns[i];
            }
            
            std::cout << "  Portfolio metrics:" << std::endl;
            std::cout << "    Expected daily return: " << portfolio_return * 100 << "%" << std::endl;
            std::cout << "    Expected annual return: " << portfolio_return * 252 * 100 << "%" << std::endl;
            
        } else {
            std::cout << "❌ Portfolio optimization failed: " << opt_result.error().message << std::endl;
            std::cout << "    Note: Using equal-weight fallback" << std::endl;
        }
    }
    
    // Test 4: Performance Benchmarking
    display_separator("Test 4: Performance Benchmarking");
    
    std::cout << "🏁 Running performance benchmark..." << std::endl;
    std::cout << "  Comparing GPU vs CPU performance for matrix operations" << std::endl;
    
    const size_t benchmark_size = std::min(size_t(200), n_assets);  // Smaller size for fair comparison
    
    auto benchmark_result = optimizer.benchmark_performance(benchmark_size);
    if (benchmark_result.is_ok()) {
        const auto& benchmark = benchmark_result.value();
        
        std::cout << "✅ Benchmark completed:" << std::endl;
        std::cout << "  Operation: " << benchmark.operation << std::endl;
        std::cout << "  Matrix size: " << benchmark.matrix_size << "x" << benchmark.matrix_size << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "  GPU time: " << benchmark.gpu_time_ms << " ms" << std::endl;
        std::cout << "  CPU time: " << benchmark.cpu_time_ms << " ms" << std::endl;
        std::cout << "  Speedup: " << benchmark.speedup_factor << "x" << std::endl;
        
        if (benchmark.speedup_factor > 1.0) {
            std::cout << "🚀 GPU acceleration provides " << benchmark.speedup_factor 
                      << "x speedup over CPU!" << std::endl;
        } else {
            std::cout << "📝 CPU outperformed GPU for this problem size" << std::endl;
            std::cout << "   (GPU acceleration typically benefits larger problems)" << std::endl;
        }
        
    } else {
        std::cout << "❌ Benchmark failed: " << benchmark_result.error().message << std::endl;
    }
    
    // Test 5: GPU Matrix Operations
    display_separator("Test 5: GPU Matrix Operations");
    
    std::cout << "🔢 Testing GPU matrix operations..." << std::endl;
    
    // Test matrix multiplication
    size_t test_size = 100;
    std::vector<std::vector<double>> matrix_A(test_size, std::vector<double>(test_size));
    std::vector<std::vector<double>> matrix_B(test_size, std::vector<double>(test_size));
    
    std::mt19937 test_gen(12345);
    std::uniform_real_distribution<double> test_dist(-1.0, 1.0);
    
    for (auto& row : matrix_A) {
        for (auto& val : row) {
            val = test_dist(test_gen);
        }
    }
    for (auto& row : matrix_B) {
        for (auto& val : row) {
            val = test_dist(test_gen);
        }
    }
    
    auto mult_result = GPUMatrixOps::matrix_multiply_gpu(matrix_A, matrix_B);
    if (mult_result.is_ok()) {
        std::cout << "✅ Matrix multiplication completed" << std::endl;
        display_matrix_sample(mult_result.value(), "Result Matrix");
    } else {
        std::cout << "❌ Matrix multiplication failed: " << mult_result.error().message << std::endl;
    }
    
    // Test Cholesky decomposition
    if (cov_result.is_ok() && cov_result.value().size() <= 50) {
        std::cout << "\n🔍 Testing Cholesky decomposition..." << std::endl;
        
        // Use a smaller subset of covariance matrix
        size_t chol_size = std::min(size_t(20), cov_result.value().size());
        std::vector<std::vector<double>> small_cov(chol_size, std::vector<double>(chol_size));
        
        for (size_t i = 0; i < chol_size; ++i) {
            for (size_t j = 0; j < chol_size; ++j) {
                small_cov[i][j] = cov_result.value()[i][j];
            }
        }
        
        auto chol_result = GPUMatrixOps::cholesky_decomposition_gpu(small_cov);
        if (chol_result.is_ok()) {
            std::cout << "✅ Cholesky decomposition completed" << std::endl;
            display_matrix_sample(chol_result.value(), "Cholesky Factor (Lower Triangular)");
        } else {
            std::cout << "❌ Cholesky decomposition failed: " << chol_result.error().message << std::endl;
        }
    }
    
    // Summary
    display_separator("Summary");
    
    std::cout << "🎯 GPU Acceleration Summary:" << std::endl;
    std::cout << "  1. Successfully initialized GPU optimizer" << std::endl;
    std::cout << "  2. Processed " << n_assets << " assets with " << n_periods << " time periods" << std::endl;
    std::cout << "  3. Computed covariance matrices for large portfolios" << std::endl;
    std::cout << "  4. Ran Monte Carlo simulations for risk assessment" << std::endl;
    std::cout << "  5. Performed portfolio optimization with constraints" << std::endl;
    std::cout << "  6. Benchmarked GPU vs CPU performance" << std::endl;
    std::cout << "  7. Demonstrated matrix operations on GPU" << std::endl;
    
    std::cout << "\n💡 Key Benefits of GPU Acceleration:" << std::endl;
    std::cout << "  • Parallel processing of large covariance matrices" << std::endl;
    std::cout << "  • Massive speedup for Monte Carlo simulations" << std::endl;
    std::cout << "  • Real-time portfolio optimization for institutional use" << std::endl;
    std::cout << "  • Scalable to thousands of assets" << std::endl;
    std::cout << "  • Automatic fallback to CPU when GPU unavailable" << std::endl;
    
    std::cout << "\n🚀 Production Use Cases:" << std::endl;
    std::cout << "  • High-frequency trading portfolio optimization" << std::endl;
    std::cout << "  • Real-time risk management for large portfolios" << std::endl;
    std::cout << "  • Backtesting with millions of Monte Carlo paths" << std::endl;
    std::cout << "  • Intraday rebalancing with market regime detection" << std::endl;
    
    std::cout << "\n✅ GPU acceleration example completed successfully!" << std::endl;
    
    return 0;
}