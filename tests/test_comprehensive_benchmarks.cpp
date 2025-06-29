#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>
#include <pyfolio/analytics/performance_metrics.h>
#include <pyfolio/analytics/statistics.h>
#include <pyfolio/core/time_series.h>
#include <pyfolio/math/simd_math.h>
#include <pyfolio/memory/pool_allocator.h>
#include <random>
#include <sstream>
#include <vector>

using namespace pyfolio;
using namespace pyfolio::analytics;
using namespace pyfolio::memory;
using namespace std::chrono;

/**
 * @brief Comprehensive benchmarking suite for pyfolio_cpp
 *
 * This suite measures performance across all major components:
 * - Core data structures (TimeSeries)
 * - Mathematical operations (SIMD vs scalar)
 * - Financial analytics (metrics, statistics, risk)
 * - Memory management (pool vs standard allocators)
 * - Real-world portfolio analysis scenarios
 */
class ComprehensiveBenchmarkTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Initialize test data of various sizes
        std::mt19937 gen(42);
        std::uniform_real_distribution<double> returns_dist(-0.1, 0.1);
        std::uniform_real_distribution<double> price_dist(50.0, 200.0);

        DateTime base_date = DateTime::parse("2020-01-01").value();

        for (auto size : test_sizes_) {
            // Generate return series
            std::vector<DateTime> dates;
            std::vector<double> returns, prices;

            double current_price = 100.0;
            for (size_t i = 0; i < size; ++i) {
                dates.push_back(base_date.add_days(i));
                double ret = returns_dist(gen);
                returns.push_back(ret);
                current_price *= (1.0 + ret);
                prices.push_back(current_price);
            }

            return_series_[size] = TimeSeries<double>(dates, returns, "returns");
            price_series_[size]  = TimeSeries<double>(dates, prices, "prices");
        }

        // Initialize benchmark results storage
        results_.clear();
    }

    struct BenchmarkResult {
        std::string test_name;
        size_t data_size;
        double execution_time_ms;
        double operations_per_second;
        std::string notes;
    };

    template <typename Func>
    double measure_time_ms(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    void record_result(const std::string& test_name, size_t data_size, double time_ms, const std::string& notes = "") {
        BenchmarkResult result;
        result.test_name             = test_name;
        result.data_size             = data_size;
        result.execution_time_ms     = time_ms;
        result.operations_per_second = (data_size * 1000.0) / time_ms;
        result.notes                 = notes;
        results_.push_back(result);
    }

    const std::vector<size_t> test_sizes_ = {100, 1000, 10000, 100000};
    std::map<size_t, TimeSeries<double>> return_series_;
    std::map<size_t, TimeSeries<double>> price_series_;
    std::vector<BenchmarkResult> results_;
};

TEST_F(ComprehensiveBenchmarkTest, TimeSeriesOperationsBenchmark) {
    std::cout << "\n=== TimeSeries Operations Benchmark ===\n";
    std::cout << std::setw(15) << "Operation" << std::setw(10) << "Size" << std::setw(15) << "Time (ms)"
              << std::setw(15) << "Ops/sec" << "\n";

    for (auto size : test_sizes_) {
        const auto& ts = return_series_[size];

        // Test basic arithmetic operations
        {
            auto time_ms = measure_time_ms([&]() {
                auto result = ts + ts;
                EXPECT_TRUE(result.is_ok());
            });
            record_result("Addition", size, time_ms);
            std::cout << std::setw(15) << "Addition" << std::setw(10) << size << std::setw(15) << std::fixed
                      << std::setprecision(3) << time_ms << std::setw(15) << std::setprecision(0)
                      << (size * 1000.0 / time_ms) << "\n";
        }

        // Test statistical operations
        {
            auto time_ms = measure_time_ms([&]() {
                auto mean_result = ts.mean();
                auto std_result  = ts.std();
                EXPECT_TRUE(mean_result.is_ok());
                EXPECT_TRUE(std_result.is_ok());
            });
            record_result("Stats", size, time_ms);
            std::cout << std::setw(15) << "Stats" << std::setw(10) << size << std::setw(15) << std::fixed
                      << std::setprecision(3) << time_ms << std::setw(15) << std::setprecision(0)
                      << (size * 1000.0 / time_ms) << "\n";
        }

        // Test correlation
        {
            auto time_ms = measure_time_ms([&]() {
                auto corr_result = ts.correlation(ts);
                EXPECT_TRUE(corr_result.is_ok());
            });
            record_result("Correlation", size, time_ms);
            std::cout << std::setw(15) << "Correlation" << std::setw(10) << size << std::setw(15) << std::fixed
                      << std::setprecision(3) << time_ms << std::setw(15) << std::setprecision(0)
                      << (size * 1000.0 / time_ms) << "\n";
        }
    }
}

TEST_F(ComprehensiveBenchmarkTest, PerformanceMetricsBenchmark) {
    std::cout << "\n=== Performance Metrics Benchmark ===\n";
    std::cout << std::setw(20) << "Metric" << std::setw(10) << "Size" << std::setw(15) << "Time (ms)" << std::setw(15)
              << "Ops/sec" << "\n";

    for (auto size : test_sizes_) {
        const auto& returns = return_series_[size];

        // Statistical summary calculation
        {
            auto time_ms = measure_time_ms([&]() {
                auto result = analytics::statistics::calculate_summary(returns);
                EXPECT_TRUE(result.is_ok());
            });
            record_result("Stats Summary", size, time_ms);
            std::cout << std::setw(20) << "Stats Summary" << std::setw(10) << size << std::setw(15) << std::fixed
                      << std::setprecision(3) << time_ms << std::setw(15) << std::setprecision(0)
                      << (size * 1000.0 / time_ms) << "\n";
        }

        // Rolling operations
        {
            auto time_ms = measure_time_ms([&]() {
                auto rolling_mean_result = returns.rolling_mean(30);
                auto rolling_std_result  = returns.rolling_std(30);
                EXPECT_TRUE(rolling_mean_result.is_ok());
                EXPECT_TRUE(rolling_std_result.is_ok());
            });
            record_result("Rolling Ops", size, time_ms);
            std::cout << std::setw(20) << "Rolling Ops" << std::setw(10) << size << std::setw(15) << std::fixed
                      << std::setprecision(3) << time_ms << std::setw(15) << std::setprecision(0)
                      << (size * 1000.0 / time_ms) << "\n";
        }

        // Cumulative operations
        {
            auto time_ms = measure_time_ms([&]() {
                auto cumsum_result  = returns.cumsum();
                auto cumprod_result = returns.cumprod();
                EXPECT_TRUE(cumsum_result.is_ok());
                EXPECT_TRUE(cumprod_result.is_ok());
            });
            record_result("Cumulative Ops", size, time_ms);
            std::cout << std::setw(20) << "Cumulative Ops" << std::setw(10) << size << std::setw(15) << std::fixed
                      << std::setprecision(3) << time_ms << std::setw(15) << std::setprecision(0)
                      << (size * 1000.0 / time_ms) << "\n";
        }
    }
}

TEST_F(ComprehensiveBenchmarkTest, StatisticsAnalyticsBenchmark) {
    std::cout << "\n=== Statistics Analytics Benchmark ===\n";
    std::cout << std::setw(20) << "Analysis" << std::setw(10) << "Size" << std::setw(15) << "Time (ms)" << std::setw(15)
              << "Ops/sec" << "\n";

    for (auto size : test_sizes_) {
        const auto& returns = return_series_[size];

        // Rolling window calculations
        {
            auto time_ms = measure_time_ms([&]() {
                auto rolling_min = returns.rolling_min(30);
                auto rolling_max = returns.rolling_max(30);
                EXPECT_TRUE(rolling_min.is_ok());
                EXPECT_TRUE(rolling_max.is_ok());
            });
            record_result("Rolling Min/Max", size, time_ms);
            std::cout << std::setw(20) << "Rolling Min/Max" << std::setw(10) << size << std::setw(15) << std::fixed
                      << std::setprecision(3) << time_ms << std::setw(15) << std::setprecision(0)
                      << (size * 1000.0 / time_ms) << "\n";
        }

        // Data transformation operations
        {
            auto time_ms = measure_time_ms([&]() {
                auto pct_change = returns.pct_change();
                auto shifted    = returns.shift(1);
                EXPECT_TRUE(pct_change.is_ok());
                EXPECT_TRUE(shifted.is_ok());
            });
            record_result("Transformations", size, time_ms);
            std::cout << std::setw(20) << "Transformations" << std::setw(10) << size << std::setw(15) << std::fixed
                      << std::setprecision(3) << time_ms << std::setw(15) << std::setprecision(0)
                      << (size * 1000.0 / time_ms) << "\n";
        }

        // Basic statistics aggregation
        {
            auto time_ms = measure_time_ms([&]() {
                auto mean    = returns.mean();
                auto std_dev = returns.std();
                auto corr    = returns.correlation(returns);
                EXPECT_TRUE(mean.is_ok());
                EXPECT_TRUE(std_dev.is_ok());
                EXPECT_TRUE(corr.is_ok());
            });
            record_result("Basic Stats", size, time_ms);
            std::cout << std::setw(20) << "Basic Stats" << std::setw(10) << size << std::setw(15) << std::fixed
                      << std::setprecision(3) << time_ms << std::setw(15) << std::setprecision(0)
                      << (size * 1000.0 / time_ms) << "\n";
        }
    }
}

TEST_F(ComprehensiveBenchmarkTest, SIMDPerformanceComparison) {
    std::cout << "\n=== SIMD vs Scalar Performance ===\n";
    std::cout << std::setw(15) << "Operation" << std::setw(10) << "Size" << std::setw(12) << "Scalar(ms)"
              << std::setw(12) << "SIMD(ms)" << std::setw(12) << "Speedup" << "\n";

    std::mt19937 gen(42);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);

    for (auto size : test_sizes_) {
        std::vector<double> a(size), b(size), result_scalar(size), result_simd(size);
        for (size_t i = 0; i < size; ++i) {
            a[i] = dist(gen);
            b[i] = dist(gen);
        }

        // Vector addition comparison
        {
            auto scalar_time = measure_time_ms([&]() {
                for (size_t i = 0; i < size; ++i) {
                    result_scalar[i] = a[i] + b[i];
                }
            });

            auto simd_time = measure_time_ms([&]() {
                simd::vector_add(std::span<const double>(a), std::span<const double>(b),
                                 std::span<double>(result_simd));
            });

            double speedup = scalar_time / simd_time;
            std::cout << std::setw(15) << "Addition" << std::setw(10) << size << std::setw(12) << std::fixed
                      << std::setprecision(3) << scalar_time << std::setw(12) << simd_time << std::setw(12)
                      << std::setprecision(2) << speedup << "x\n";

            record_result("SIMD Addition", size, simd_time, "vs scalar: " + std::to_string(speedup) + "x");
        }

        // Dot product comparison
        {
            double scalar_result = 0.0, simd_result = 0.0;

            auto scalar_time = measure_time_ms([&]() {
                double sum = 0.0;
                for (size_t i = 0; i < size; ++i) {
                    sum += a[i] * b[i];
                }
                scalar_result = sum;
            });

            auto simd_time = measure_time_ms(
                [&]() { simd_result = simd::dot_product(std::span<const double>(a), std::span<const double>(b)); });

            double speedup = scalar_time / simd_time;
            std::cout << std::setw(15) << "Dot Product" << std::setw(10) << size << std::setw(12) << std::fixed
                      << std::setprecision(3) << scalar_time << std::setw(12) << simd_time << std::setw(12)
                      << std::setprecision(2) << speedup << "x\n";

            record_result("SIMD Dot Product", size, simd_time, "vs scalar: " + std::to_string(speedup) + "x");
        }
    }
}

TEST_F(ComprehensiveBenchmarkTest, MemoryAllocationBenchmark) {
    std::cout << "\n=== Memory Allocation Performance ===\n";
    std::cout << std::setw(20) << "Allocator" << std::setw(15) << "Allocations" << std::setw(15) << "Time (ms)"
              << std::setw(15) << "Allocs/sec" << "\n";

    const int num_allocations = 10000;

    // Standard allocator
    {
        auto time_ms = measure_time_ms([&]() {
            std::vector<int*> ptrs;
            ptrs.reserve(num_allocations);

            for (int i = 0; i < num_allocations; ++i) {
                ptrs.push_back(new int(i));
            }

            for (int* ptr : ptrs) {
                delete ptr;
            }
        });

        std::cout << std::setw(20) << "Standard new/delete" << std::setw(15) << num_allocations << std::setw(15)
                  << std::fixed << std::setprecision(3) << time_ms << std::setw(15) << std::setprecision(0)
                  << (num_allocations * 1000.0 / time_ms) << "\n";

        record_result("Standard Allocator", num_allocations, time_ms);
    }

    // Pool allocator
    {
        auto time_ms = measure_time_ms([&]() {
            FixedBlockAllocator<int, 15000> allocator;
            std::vector<int*> ptrs;
            ptrs.reserve(num_allocations);

            for (int i = 0; i < num_allocations; ++i) {
                int* ptr = allocator.allocate();
                if (ptr) {
                    *ptr = i;
                    ptrs.push_back(ptr);
                }
            }

            for (int* ptr : ptrs) {
                allocator.deallocate(ptr);
            }
        });

        std::cout << std::setw(20) << "Pool allocator" << std::setw(15) << num_allocations << std::setw(15)
                  << std::fixed << std::setprecision(3) << time_ms << std::setw(15) << std::setprecision(0)
                  << (num_allocations * 1000.0 / time_ms) << "\n";

        record_result("Pool Allocator", num_allocations, time_ms);
    }

    // STL with pool allocator
    {
        auto time_ms = measure_time_ms([&]() {
            pool_vector<int> vec;
            for (int i = 0; i < num_allocations; ++i) {
                vec.push_back(i);
            }
        });

        std::cout << std::setw(20) << "STL pool vector" << std::setw(15) << num_allocations << std::setw(15)
                  << std::fixed << std::setprecision(3) << time_ms << std::setw(15) << std::setprecision(0)
                  << (num_allocations * 1000.0 / time_ms) << "\n";

        record_result("STL Pool Vector", num_allocations, time_ms);
    }
}

TEST_F(ComprehensiveBenchmarkTest, RealWorldPortfolioAnalysis) {
    std::cout << "\n=== Real-World Portfolio Analysis Benchmark ===\n";
    std::cout << std::setw(25) << "Analysis Type" << std::setw(15) << "Time (ms)" << std::setw(20)
              << "Memory Usage (KB)" << "\n";

    // Simulate a realistic portfolio with multiple assets
    const size_t portfolio_size = 10000;  // 10k daily observations
    const size_t num_assets     = 50;

    std::mt19937 gen(42);
    std::uniform_real_distribution<double> returns_dist(-0.05, 0.05);

    std::vector<TimeSeries<double>> asset_returns;
    DateTime base_date = DateTime::parse("2020-01-01").value();

    // Generate multi-asset portfolio data
    for (size_t asset = 0; asset < num_assets; ++asset) {
        std::vector<DateTime> dates;
        std::vector<double> returns;

        for (size_t i = 0; i < portfolio_size; ++i) {
            dates.push_back(base_date.add_days(i));
            returns.push_back(returns_dist(gen));
        }

        asset_returns.emplace_back(dates, returns, "asset_" + std::to_string(asset));
    }

    // Portfolio-level analysis
    {
        auto time_ms = measure_time_ms([&]() {
            // Calculate portfolio metrics for each asset
            for (const auto& asset : asset_returns) {
                auto summary = analytics::statistics::calculate_summary(asset);
                auto mean    = asset.mean();
                EXPECT_TRUE(summary.is_ok());
                EXPECT_TRUE(mean.is_ok());
            }

            // Cross-asset correlation matrix (partial)
            for (size_t i = 0; i < std::min(size_t(10), num_assets); ++i) {
                for (size_t j = i + 1; j < std::min(size_t(10), num_assets); ++j) {
                    auto corr = asset_returns[i].correlation(asset_returns[j]);
                    EXPECT_TRUE(corr.is_ok());
                }
            }
        });

        std::cout << std::setw(25) << "Portfolio Analysis" << std::setw(15) << std::fixed << std::setprecision(3)
                  << time_ms << std::setw(20) << "N/A" << "\n";

        record_result("Portfolio Analysis", portfolio_size * num_assets, time_ms,
                      std::to_string(num_assets) + " assets");
    }

    // Statistical analysis across portfolio
    {
        auto time_ms = measure_time_ms([&]() {
            for (const auto& asset : asset_returns) {
                auto mean       = asset.mean();
                auto std_dev    = asset.std();
                auto cumulative = asset.cumsum();
                EXPECT_TRUE(mean.is_ok());
                EXPECT_TRUE(std_dev.is_ok());
                EXPECT_TRUE(cumulative.is_ok());
            }
        });

        std::cout << std::setw(25) << "Statistical Analysis" << std::setw(15) << std::fixed << std::setprecision(3)
                  << time_ms << std::setw(20) << "N/A" << "\n";

        record_result("Statistical Analysis", portfolio_size * num_assets, time_ms,
                      std::to_string(num_assets) + " assets");
    }
}

TEST_F(ComprehensiveBenchmarkTest, GenerateBenchmarkReport) {
    // Generate comprehensive benchmark report
    std::stringstream report;

    report << "# Pyfolio C++ Comprehensive Benchmark Report\n\n";
    report << "## Test Environment\n";
    report << "- Test Date: " << std::chrono::system_clock::now().time_since_epoch().count() << "\n";
    report << "- Compiler: " << __VERSION__ << "\n";
    report << "- Platform: " <<
#ifdef __APPLE__
        "macOS"
#elif __linux__
        "Linux"
#elif _WIN32
        "Windows"
#else
        "Unknown"
#endif
           << "\n\n";

    // Group results by test type
    std::map<std::string, std::vector<BenchmarkResult>> grouped_results;
    for (const auto& result : results_) {
        grouped_results[result.test_name].push_back(result);
    }

    report << "## Performance Summary\n\n";
    for (const auto& [test_name, test_results] : grouped_results) {
        if (test_results.empty())
            continue;

        report << "### " << test_name << "\n";
        report << "| Data Size | Execution Time (ms) | Operations/sec | Notes |\n";
        report << "|-----------|-------------------|----------------|-------|\n";

        for (const auto& result : test_results) {
            report << "| " << result.data_size << " | " << std::fixed << std::setprecision(3)
                   << result.execution_time_ms << " | " << std::setprecision(0) << result.operations_per_second << " | "
                   << result.notes << " |\n";
        }
        report << "\n";
    }

    // Write report to file
    std::ofstream report_file("/Users/yunjinqi/Documents/source_code/pyfolio_cpp/BENCHMARK_REPORT.md");
    if (report_file.is_open()) {
        report_file << report.str();
        report_file.close();
        std::cout << "\n=== Benchmark Report Generated ===\n";
        std::cout << "Report saved to: BENCHMARK_REPORT.md\n";
    } else {
        std::cout << "\n=== Benchmark Report (Console Output) ===\n";
        std::cout << report.str();
    }

    EXPECT_FALSE(results_.empty());
}
