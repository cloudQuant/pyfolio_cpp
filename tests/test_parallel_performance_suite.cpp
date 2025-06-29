#include <chrono>
#include <gtest/gtest.h>
#include <iomanip>
#include <pyfolio/analytics/parallel_performance_suite.h>
#include <pyfolio/core/time_series.h>
#include <random>
#include <vector>

using namespace pyfolio;
using namespace pyfolio::analytics;

class ParallelPerformanceSuiteTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Generate test data of various sizes for performance comparison
        std::mt19937 gen(42);
        std::normal_distribution<double> returns_dist(0.0008, 0.015);    // ~15% annual vol, positive drift
        std::normal_distribution<double> benchmark_dist(0.0005, 0.012);  // Market benchmark

        DateTime base_date = DateTime::parse("2023-01-01").value();

        // Create datasets of different sizes
        for (auto size : test_sizes_) {
            std::vector<DateTime> dates;
            std::vector<double> portfolio_returns, benchmark_returns;

            dates.reserve(size);
            portfolio_returns.reserve(size);
            benchmark_returns.reserve(size);

            for (size_t i = 0; i < size; ++i) {
                dates.push_back(base_date.add_days(i));
                portfolio_returns.push_back(returns_dist(gen));
                benchmark_returns.push_back(benchmark_dist(gen));
            }

            portfolio_series_map_[size] =
                TimeSeries<double>(dates, portfolio_returns, "portfolio_" + std::to_string(size));
            benchmark_series_map_[size] =
                TimeSeries<double>(dates, benchmark_returns, "benchmark_" + std::to_string(size));
        }

        // Initialize suites
        AnalysisConfig config;
        config.risk_free_rate          = 0.02;
        config.periods_per_year        = 252;
        config.rolling_windows         = {30, 60, 90};
        config.min_sharpe_threshold    = 0.5;
        config.max_drawdown_threshold  = 0.15;
        config.enable_detailed_reports = true;

        cached_suite_   = std::make_unique<PerformanceAnalysisSuite>(config);
        parallel_suite_ = std::make_unique<ParallelPerformanceAnalysisSuite>(config);
    }

    template <typename Func>
    double measure_time_ms(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    const std::vector<size_t> test_sizes_ = {1000, 10000, 50000, 100000};
    std::map<size_t, TimeSeries<double>> portfolio_series_map_;
    std::map<size_t, TimeSeries<double>> benchmark_series_map_;
    std::unique_ptr<PerformanceAnalysisSuite> cached_suite_;
    std::unique_ptr<ParallelPerformanceAnalysisSuite> parallel_suite_;
};

TEST_F(ParallelPerformanceSuiteTest, BasicParallelAnalysis) {
    const auto& portfolio = portfolio_series_map_[10000];
    const auto& benchmark = benchmark_series_map_[10000];

    // Test parallel analysis
    auto parallel_result = parallel_suite_->analyze_performance_parallel(portfolio, std::make_optional(benchmark));
    ASSERT_TRUE(parallel_result.is_ok());

    const auto& report = parallel_result.value();

    // Validate basic metrics
    EXPECT_GT(report.annual_return, -1.0);
    EXPECT_LT(report.annual_return, 5.0);
    EXPECT_GT(report.annual_volatility, 0);
    EXPECT_GE(report.max_drawdown, 0);
    EXPECT_LE(report.max_drawdown, 1.0);

    // Validate rolling metrics were calculated
    EXPECT_FALSE(report.rolling_returns.empty());
    EXPECT_FALSE(report.rolling_volatility.empty());
    EXPECT_FALSE(report.rolling_sharpe.empty());

    // Validate benchmark comparison
    EXPECT_TRUE(report.alpha.has_value());
    EXPECT_TRUE(report.beta.has_value());
    EXPECT_TRUE(report.information_ratio.has_value());
    EXPECT_TRUE(report.tracking_error.has_value());

    std::cout << "\\n=== Basic Parallel Analysis Results ===\\n";
    std::cout << "Annual Return:    " << std::fixed << std::setprecision(4) << (report.annual_return * 100) << "%\\n";
    std::cout << "Annual Volatility: " << (report.annual_volatility * 100) << "%\\n";
    std::cout << "Sharpe Ratio:     " << report.sharpe_ratio << "\\n";
    std::cout << "Max Drawdown:     " << (report.max_drawdown * 100) << "%\\n";
    std::cout << "Alpha:            " << (report.alpha.value() * 100) << "%\\n";
    std::cout << "Beta:             " << report.beta.value() << "\\n";
    std::cout << "Computation Time: " << report.computation_time.count() << "ms\\n";
}

TEST_F(ParallelPerformanceSuiteTest, PerformanceComparison) {
    std::cout << "\\n=== Cached vs Parallel Performance Comparison ===\\n";
    std::cout << std::setw(12) << "Size" << std::setw(15) << "Cached(ms)" << std::setw(15) << "Parallel(ms)"
              << std::setw(12) << "Speedup" << std::setw(15) << "Efficiency" << "\\n";

    for (auto size : test_sizes_) {
        const auto& portfolio = portfolio_series_map_[size];
        const auto& benchmark = benchmark_series_map_[size];

        // Measure cached suite performance
        double cached_time = measure_time_ms([&]() {
            auto result = cached_suite_->analyze_performance(portfolio, std::make_optional(benchmark));
            EXPECT_TRUE(result.is_ok());
        });

        // Measure parallel suite performance
        double parallel_time = measure_time_ms([&]() {
            auto result = parallel_suite_->analyze_performance_parallel(portfolio, std::make_optional(benchmark));
            EXPECT_TRUE(result.is_ok());
        });

        double speedup    = cached_time > 0 ? cached_time / parallel_time : 0;
        double efficiency = speedup / std::thread::hardware_concurrency() * 100;

        std::cout << std::setw(12) << size << std::setw(15) << std::fixed << std::setprecision(3) << cached_time
                  << std::setw(15) << parallel_time << std::setw(12) << std::setprecision(2) << speedup << "x"
                  << std::setw(15) << std::setprecision(1) << efficiency << "%\\n";

        // For large datasets, parallel should be competitive or faster
        if (size >= 50000) {
            EXPECT_LE(parallel_time, cached_time * 2.0);  // Allow some overhead
        }
    }
}

TEST_F(ParallelPerformanceSuiteTest, AccuracyVerification) {
    // Compare results between cached and parallel implementations
    const auto& portfolio = portfolio_series_map_[10000];
    const auto& benchmark = benchmark_series_map_[10000];

    auto cached_result   = cached_suite_->analyze_performance(portfolio, std::make_optional(benchmark));
    auto parallel_result = parallel_suite_->analyze_performance_parallel(portfolio, std::make_optional(benchmark));

    ASSERT_TRUE(cached_result.is_ok());
    ASSERT_TRUE(parallel_result.is_ok());

    const auto& cached_report   = cached_result.value();
    const auto& parallel_report = parallel_result.value();

    std::cout << "\\n=== Accuracy Verification ===\\n";
    std::cout << std::setw(20) << "Metric" << std::setw(15) << "Cached" << std::setw(15) << "Parallel" << std::setw(15)
              << "Diff %" << "\\n";

    // Compare basic metrics
    auto compare_metric = [&](const std::string& name, double cached_val, double parallel_val) {
        double diff_pct = cached_val != 0 ? std::abs(cached_val - parallel_val) / std::abs(cached_val) * 100 : 0;
        std::cout << std::setw(20) << name << std::setw(15) << std::fixed << std::setprecision(6) << cached_val
                  << std::setw(15) << parallel_val << std::setw(15) << std::setprecision(4) << diff_pct << "%\\n";

        // Allow small numerical differences (within 0.1%)
        EXPECT_LT(diff_pct, 0.1) << "Large difference in " << name;
    };

    compare_metric("Annual Return", cached_report.annual_return, parallel_report.annual_return);
    compare_metric("Annual Volatility", cached_report.annual_volatility, parallel_report.annual_volatility);
    compare_metric("Sharpe Ratio", cached_report.sharpe_ratio, parallel_report.sharpe_ratio);
    compare_metric("Max Drawdown", cached_report.max_drawdown, parallel_report.max_drawdown);
    compare_metric("Sortino Ratio", cached_report.sortino_ratio, parallel_report.sortino_ratio);

    if (cached_report.alpha && parallel_report.alpha) {
        compare_metric("Alpha", cached_report.alpha.value(), parallel_report.alpha.value());
    }

    if (cached_report.beta && parallel_report.beta) {
        compare_metric("Beta", cached_report.beta.value(), parallel_report.beta.value());
    }

    // Compare rolling metrics sizes
    EXPECT_EQ(cached_report.rolling_returns.size(), parallel_report.rolling_returns.size());
    EXPECT_EQ(cached_report.rolling_volatility.size(), parallel_report.rolling_volatility.size());
    EXPECT_EQ(cached_report.rolling_sharpe.size(), parallel_report.rolling_sharpe.size());
}

TEST_F(ParallelPerformanceSuiteTest, LargeDatasetStressTest) {
    // Create a very large dataset
    std::mt19937 gen(12345);
    std::normal_distribution<double> dist(0.001, 0.02);

    const size_t large_size = 500000;  // 500k data points
    DateTime base_date      = DateTime::parse("2020-01-01").value();

    std::vector<DateTime> dates;
    std::vector<double> portfolio_values, benchmark_values;

    dates.reserve(large_size);
    portfolio_values.reserve(large_size);
    benchmark_values.reserve(large_size);

    for (size_t i = 0; i < large_size; ++i) {
        dates.push_back(base_date.add_days(i));
        portfolio_values.push_back(dist(gen));
        benchmark_values.push_back(dist(gen) * 0.8);  // Correlated benchmark
    }

    TimeSeries<double> large_portfolio(dates, portfolio_values, "stress_test_portfolio");
    TimeSeries<double> large_benchmark(dates, benchmark_values, "stress_test_benchmark");

    std::cout << "\\n=== Large Dataset Stress Test ===\\n";
    std::cout << "Dataset size: " << large_size << " data points\\n";

    // Test parallel analysis on large dataset
    auto start_time = std::chrono::high_resolution_clock::now();

    auto result = parallel_suite_->analyze_performance_parallel(large_portfolio, std::make_optional(large_benchmark));

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end_time - start_time);

    ASSERT_TRUE(result.is_ok());

    const auto& report = result.value();

    std::cout << "Total computation time: " << duration.count() << "ms\\n";
    std::cout << "Annual Return:          " << (report.annual_return * 100) << "%\\n";
    std::cout << "Annual Volatility:      " << (report.annual_volatility * 100) << "%\\n";
    std::cout << "Sharpe Ratio:           " << report.sharpe_ratio << "\\n";
    std::cout << "Rolling metrics count:  " << report.rolling_returns.size() << "\\n";

    // Verify results are reasonable
    EXPECT_GT(report.annual_return, -0.1);
    EXPECT_LT(report.annual_return, 0.2);
    EXPECT_GT(report.annual_volatility, 0.1);
    EXPECT_LT(report.annual_volatility, 0.5);
    EXPECT_FALSE(report.rolling_returns.empty());

    // Performance should be reasonable for large dataset
    EXPECT_LT(duration.count(), 10000);  // Should complete in less than 10 seconds
}

TEST_F(ParallelPerformanceSuiteTest, ParallelConfigurationTesting) {
    const auto& portfolio = portfolio_series_map_[50000];

    // Test with different parallel configurations
    std::vector<parallel::ParallelConfig> configs;

    // High parallelism
    parallel::ParallelConfig high_parallel;
    high_parallel.max_threads        = std::thread::hardware_concurrency();
    high_parallel.parallel_threshold = 1000;
    high_parallel.adaptive_chunking  = true;
    configs.push_back(high_parallel);

    // Conservative parallelism
    parallel::ParallelConfig conservative;
    conservative.max_threads        = 2;
    conservative.parallel_threshold = 10000;
    conservative.adaptive_chunking  = false;
    configs.push_back(conservative);

    // Serial execution (disabled parallelism)
    parallel::ParallelConfig serial;
    serial.max_threads        = 1;
    serial.parallel_threshold = 1000000;
    configs.push_back(serial);

    std::cout << "\\n=== Parallel Configuration Testing ===\\n";
    std::cout << std::setw(20) << "Config" << std::setw(12) << "Threads" << std::setw(15) << "Time(ms)" << std::setw(15)
              << "Result" << "\\n";

    std::vector<std::string> config_names = {"High Parallel", "Conservative", "Serial"};

    for (size_t i = 0; i < configs.size(); ++i) {
        parallel_suite_->update_parallel_config(configs[i]);

        double analysis_time = measure_time_ms([&]() {
            auto result = parallel_suite_->analyze_performance_parallel(portfolio);
            EXPECT_TRUE(result.is_ok());
        });

        auto stats = parallel_suite_->get_parallel_stats();

        std::cout << std::setw(20) << config_names[i] << std::setw(12) << stats.available_threads << std::setw(15)
                  << std::fixed << std::setprecision(3) << analysis_time << std::setw(15) << "Success" << "\\n";
    }
}

TEST_F(ParallelPerformanceSuiteTest, MemoryUsageTest) {
    // Test memory efficiency with large datasets
    const auto& portfolio = portfolio_series_map_[100000];
    const auto& benchmark = benchmark_series_map_[100000];

    std::cout << "\\n=== Memory Usage Test ===\\n";
    std::cout << "Dataset size: " << portfolio.size() << " data points\\n";

    // Multiple analyses to test memory stability
    for (int i = 0; i < 5; ++i) {
        auto result = parallel_suite_->analyze_performance_parallel(portfolio, std::make_optional(benchmark));
        ASSERT_TRUE(result.is_ok());

        const auto& report = result.value();
        std::cout << "Analysis " << (i + 1) << ": " << report.computation_time.count() << "ms, "
                  << "Rolling metrics: " << report.rolling_returns.size() << "\\n";
    }

    std::cout << "Memory usage test completed successfully\\n";
}

TEST_F(ParallelPerformanceSuiteTest, ConvenienceFunctionTest) {
    const auto& portfolio = portfolio_series_map_[25000];
    const auto& benchmark = benchmark_series_map_[25000];

    // Test global convenience function
    auto result = analyze_portfolio_performance_parallel(portfolio, std::make_optional(benchmark));

    ASSERT_TRUE(result.is_ok());
    const auto& report = result.value();

    std::cout << "\\n=== Convenience Function Test ===\\n";
    std::cout << "Analysis completed successfully using global function\\n";
    std::cout << "Annual Return: " << (report.annual_return * 100) << "%\\n";
    std::cout << "Sharpe Ratio:  " << report.sharpe_ratio << "\\n";
    std::cout << "Computation Time: " << report.computation_time.count() << "ms\\n";

    // Should have valid results
    EXPECT_GT(report.annual_volatility, 0);
    EXPECT_TRUE(report.alpha.has_value());
    EXPECT_TRUE(report.beta.has_value());
}

TEST_F(ParallelPerformanceSuiteTest, ErrorHandlingTest) {
    // Test with empty series
    std::vector<DateTime> empty_dates;
    std::vector<double> empty_values;
    TimeSeries<double> empty_series(empty_dates, empty_values, "empty");

    auto result = parallel_suite_->analyze_performance_parallel(empty_series);
    EXPECT_TRUE(result.is_error());

    // Test with mismatched benchmark size
    const auto& portfolio       = portfolio_series_map_[10000];
    const auto& small_benchmark = portfolio_series_map_[1000];

    auto result2 = parallel_suite_->analyze_performance_parallel(portfolio, std::make_optional(small_benchmark));
    // This should still work as the algorithm handles size differences
    EXPECT_TRUE(result2.is_ok());
}

TEST_F(ParallelPerformanceSuiteTest, ThreadSafetyTest) {
    const auto& portfolio = portfolio_series_map_[25000];

    std::vector<std::thread> threads;
    std::vector<bool> results(4, false);

    // Launch multiple threads analyzing concurrently
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&, i]() {
            auto result = parallel_suite_->analyze_performance_parallel(portfolio);
            results[i]  = result.is_ok();
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // All analyses should succeed
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(results[i]) << "Thread " << i << " failed";
    }

    std::cout << "\\n=== Thread Safety Test ===\\n";
    std::cout << "All concurrent analyses completed successfully\\n";
}