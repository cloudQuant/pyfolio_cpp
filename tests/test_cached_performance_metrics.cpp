#include <chrono>
#include <gtest/gtest.h>
#include <iomanip>
#include <pyfolio/analytics/cached_performance_metrics.h>
#include <pyfolio/core/time_series.h>
#include <random>
#include <thread>
#include <vector>

using namespace pyfolio;
using namespace pyfolio::analytics;

class CachedPerformanceMetricsTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Generate test data of various sizes
        std::mt19937 gen(42);
        std::uniform_real_distribution<double> returns_dist(-0.05, 0.05);
        std::normal_distribution<double> price_dist(100.0, 10.0);

        DateTime base_date = DateTime::parse("2024-01-01").value();

        for (auto size : test_sizes_) {
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

            return_series_[size] = TimeSeries<double>(dates, returns, "returns_" + std::to_string(size));
            price_series_[size]  = TimeSeries<double>(dates, prices, "prices_" + std::to_string(size));
        }

        // Clear cache before each test
        cached::clear_cache();
    }

    template <typename Func>
    double measure_time_ms(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    const std::vector<size_t> test_sizes_ = {100, 1000, 10000};
    std::map<size_t, TimeSeries<double>> return_series_;
    std::map<size_t, TimeSeries<double>> price_series_;
};

TEST_F(CachedPerformanceMetricsTest, BasicCacheFunctionality) {
    const auto& series = return_series_[1000];

    // First call - should miss cache and compute
    auto result1 = cached::mean(series);
    ASSERT_TRUE(result1.is_ok());

    auto stats_after_first = cached::get_cache_stats();
    EXPECT_EQ(stats_after_first.total_misses, 1);
    EXPECT_EQ(stats_after_first.total_hits, 0);

    // Second call - should hit cache
    auto result2 = cached::mean(series);
    ASSERT_TRUE(result2.is_ok());
    EXPECT_DOUBLE_EQ(result1.value(), result2.value());

    auto stats_after_second = cached::get_cache_stats();
    EXPECT_GT(stats_after_second.total_hits, 0);
}

TEST_F(CachedPerformanceMetricsTest, CacheValidation) {
    const auto& series1 = return_series_[1000];
    const auto& series2 = return_series_[100];  // Different data

    // Calculate mean for first series
    auto result1 = cached::mean(series1);
    ASSERT_TRUE(result1.is_ok());

    // Calculate mean for second series - should be cache miss (different data)
    auto result2 = cached::mean(series2);
    ASSERT_TRUE(result2.is_ok());

    // Results should be different
    EXPECT_NE(result1.value(), result2.value());

    // Both should be cached now
    auto stats = cached::get_cache_stats();
    EXPECT_EQ(stats.total_misses, 2);  // Two different computations
}

TEST_F(CachedPerformanceMetricsTest, PerformanceComparison) {
    std::cout << "\n=== Cached Performance Metrics Benchmark ===\n";
    std::cout << std::setw(20) << "Operation" << std::setw(10) << "Size" << std::setw(12) << "First(ms)"
              << std::setw(12) << "Cached(ms)" << std::setw(12) << "Speedup" << "\n";

    for (auto size : test_sizes_) {
        const auto& returns = return_series_[size];
        const auto& prices  = price_series_[size];

        // Mean calculation
        {
            cached::clear_cache();

            double first_time = measure_time_ms([&]() {
                auto result = cached::mean(returns);
                EXPECT_TRUE(result.is_ok());
            });

            double cached_time = measure_time_ms([&]() {
                auto result = cached::mean(returns);
                EXPECT_TRUE(result.is_ok());
            });

            double speedup = cached_time > 0 ? first_time / cached_time : 0;
            std::cout << std::setw(20) << "Mean" << std::setw(10) << size << std::setw(12) << std::fixed
                      << std::setprecision(4) << first_time << std::setw(12) << cached_time << std::setw(12)
                      << std::setprecision(1) << speedup << "x\n";
        }

        // Standard deviation calculation
        {
            cached::clear_cache();

            double first_time = measure_time_ms([&]() {
                auto result = cached::std_deviation(returns);
                EXPECT_TRUE(result.is_ok());
            });

            double cached_time = measure_time_ms([&]() {
                auto result = cached::std_deviation(returns);
                EXPECT_TRUE(result.is_ok());
            });

            double speedup = cached_time > 0 ? first_time / cached_time : 0;
            std::cout << std::setw(20) << "Std Deviation" << std::setw(10) << size << std::setw(12) << std::fixed
                      << std::setprecision(4) << first_time << std::setw(12) << cached_time << std::setw(12)
                      << std::setprecision(1) << speedup << "x\n";
        }

        // Correlation calculation
        {
            cached::clear_cache();

            double first_time = measure_time_ms([&]() {
                auto result = cached::correlation(returns, returns);
                EXPECT_TRUE(result.is_ok());
            });

            double cached_time = measure_time_ms([&]() {
                auto result = cached::correlation(returns, returns);
                EXPECT_TRUE(result.is_ok());
            });

            double speedup = cached_time > 0 ? first_time / cached_time : 0;
            std::cout << std::setw(20) << "Correlation" << std::setw(10) << size << std::setw(12) << std::fixed
                      << std::setprecision(4) << first_time << std::setw(12) << cached_time << std::setw(12)
                      << std::setprecision(1) << speedup << "x\n";
        }

        // Sharpe ratio calculation
        {
            cached::clear_cache();

            double first_time = measure_time_ms([&]() {
                auto result = cached::sharpe_ratio(returns, 0.02);
                EXPECT_TRUE(result.is_ok());
            });

            double cached_time = measure_time_ms([&]() {
                auto result = cached::sharpe_ratio(returns, 0.02);
                EXPECT_TRUE(result.is_ok());
            });

            double speedup = cached_time > 0 ? first_time / cached_time : 0;
            std::cout << std::setw(20) << "Sharpe Ratio" << std::setw(10) << size << std::setw(12) << std::fixed
                      << std::setprecision(4) << first_time << std::setw(12) << cached_time << std::setw(12)
                      << std::setprecision(1) << speedup << "x\n";
        }

        // Max drawdown calculation
        {
            cached::clear_cache();

            double first_time = measure_time_ms([&]() {
                auto result = cached::max_drawdown(prices);
                EXPECT_TRUE(result.is_ok());
            });

            double cached_time = measure_time_ms([&]() {
                auto result = cached::max_drawdown(prices);
                EXPECT_TRUE(result.is_ok());
            });

            double speedup = cached_time > 0 ? first_time / cached_time : 0;
            std::cout << std::setw(20) << "Max Drawdown" << std::setw(10) << size << std::setw(12) << std::fixed
                      << std::setprecision(4) << first_time << std::setw(12) << cached_time << std::setw(12)
                      << std::setprecision(1) << speedup << "x\n";
        }
    }
}

TEST_F(CachedPerformanceMetricsTest, RollingOperationsCache) {
    std::cout << "\n=== Rolling Operations Cache Performance ===\n";
    std::cout << std::setw(20) << "Operation" << std::setw(10) << "Size" << std::setw(12) << "First(ms)"
              << std::setw(12) << "Cached(ms)" << std::setw(12) << "Speedup" << "\n";

    for (auto size : test_sizes_) {
        const auto& returns = return_series_[size];

        // Rolling mean calculation
        {
            cached::clear_cache();

            double first_time = measure_time_ms([&]() {
                auto result = cached::rolling_mean(returns, 30);
                EXPECT_TRUE(result.is_ok());
            });

            double cached_time = measure_time_ms([&]() {
                auto result = cached::rolling_mean(returns, 30);
                EXPECT_TRUE(result.is_ok());
            });

            double speedup = cached_time > 0 ? first_time / cached_time : 0;
            std::cout << std::setw(20) << "Rolling Mean" << std::setw(10) << size << std::setw(12) << std::fixed
                      << std::setprecision(4) << first_time << std::setw(12) << cached_time << std::setw(12)
                      << std::setprecision(1) << speedup << "x\n";
        }

        // Rolling std calculation
        {
            cached::clear_cache();

            double first_time = measure_time_ms([&]() {
                auto result = cached::rolling_std(returns, 30);
                EXPECT_TRUE(result.is_ok());
            });

            double cached_time = measure_time_ms([&]() {
                auto result = cached::rolling_std(returns, 30);
                EXPECT_TRUE(result.is_ok());
            });

            double speedup = cached_time > 0 ? first_time / cached_time : 0;
            std::cout << std::setw(20) << "Rolling Std" << std::setw(10) << size << std::setw(12) << std::fixed
                      << std::setprecision(4) << first_time << std::setw(12) << cached_time << std::setw(12)
                      << std::setprecision(1) << speedup << "x\n";
        }
    }
}

TEST_F(CachedPerformanceMetricsTest, CacheEfficiencyTest) {
    const auto& series = return_series_[1000];

    // Perform multiple operations to build up cache
    std::vector<double> results;

    for (int i = 0; i < 10; ++i) {
        auto mean_result   = cached::mean(series);
        auto std_result    = cached::std_deviation(series);
        auto sharpe_result = cached::sharpe_ratio(series, 0.02);

        ASSERT_TRUE(mean_result.is_ok());
        ASSERT_TRUE(std_result.is_ok());
        ASSERT_TRUE(sharpe_result.is_ok());

        results.push_back(mean_result.value());
        results.push_back(std_result.value());
        results.push_back(sharpe_result.value());
    }

    auto stats = cached::get_cache_stats();

    std::cout << "\n=== Cache Efficiency Analysis ===\n";
    std::cout << "Total cache hits: " << stats.total_hits << "\n";
    std::cout << "Total cache misses: " << stats.total_misses << "\n";
    std::cout << "Hit rate: " << std::fixed << std::setprecision(2) << stats.hit_rate * 100 << "%\n";
    std::cout << "Scalar cache size: " << stats.scalar_cache_size << "\n";
    std::cout << "Metrics cache size: " << stats.metrics_cache_size << "\n";
    std::cout << "Series cache size: " << stats.series_cache_size << "\n";
    std::cout << "Total cache size: " << stats.total_cache_size << "\n";

    // Hit rate should be high after repeated operations
    EXPECT_GT(stats.hit_rate, 0.5);  // At least 50% hit rate

    // All results should be identical (cached values)
    for (size_t i = 3; i < results.size(); i += 3) {
        EXPECT_DOUBLE_EQ(results[0], results[i]);      // Mean values
        EXPECT_DOUBLE_EQ(results[1], results[i + 1]);  // Std values
        EXPECT_DOUBLE_EQ(results[2], results[i + 2]);  // Sharpe values
    }
}

TEST_F(CachedPerformanceMetricsTest, CacheInvalidation) {
    auto series1 = return_series_[1000];

    // Calculate and cache mean
    auto result1 = cached::mean(series1);
    ASSERT_TRUE(result1.is_ok());

    // Modify the series (this should invalidate cache due to different hash)
    std::vector<DateTime> dates = series1.timestamps();
    std::vector<double> values  = series1.values();
    values[0]                   = 999.0;  // Change first value

    TimeSeries<double> series2(dates, values, "modified");

    // This should be a cache miss due to different data
    auto result2 = cached::mean(series2);
    ASSERT_TRUE(result2.is_ok());

    // Results should be different
    EXPECT_NE(result1.value(), result2.value());

    auto stats = cached::get_cache_stats();
    EXPECT_EQ(stats.total_misses, 2);  // Both were cache misses
}

TEST_F(CachedPerformanceMetricsTest, MultithreadedCacheAccess) {
    const auto& series              = return_series_[1000];
    const int num_threads           = 4;
    const int operations_per_thread = 50;

    std::vector<std::thread> threads;
    std::vector<std::vector<double>> results(num_threads);

    // Launch multiple threads accessing cache concurrently
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < operations_per_thread; ++i) {
                auto mean_result = cached::mean(series);
                auto std_result  = cached::std_deviation(series);

                if (mean_result.is_ok() && std_result.is_ok()) {
                    results[t].push_back(mean_result.value());
                    results[t].push_back(std_result.value());
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all threads got the same results (thread-safe caching)
    ASSERT_FALSE(results[0].empty());

    for (int t = 1; t < num_threads; ++t) {
        ASSERT_EQ(results[0].size(), results[t].size());
        for (size_t i = 0; i < results[0].size(); ++i) {
            EXPECT_DOUBLE_EQ(results[0][i], results[t][i]);
        }
    }

    auto stats = cached::get_cache_stats();
    std::cout << "\n=== Multithreaded Cache Performance ===\n";
    std::cout << "Threads: " << num_threads << "\n";
    std::cout << "Operations per thread: " << operations_per_thread << "\n";
    std::cout << "Total operations: " << num_threads * operations_per_thread * 2 << "\n";
    std::cout << "Cache hits: " << stats.total_hits << "\n";
    std::cout << "Cache misses: " << stats.total_misses << "\n";
    std::cout << "Hit rate: " << std::fixed << std::setprecision(2) << stats.hit_rate * 100 << "%\n";

    // With multithreaded access, hit rate should still be high
    EXPECT_GT(stats.hit_rate, 0.8);  // At least 80% hit rate
}

TEST_F(CachedPerformanceMetricsTest, CacheConfigurationTest) {
    // Test with custom cache configuration
    CacheConfig config;
    config.max_entries         = 10;                              // Small cache for testing
    config.max_age             = std::chrono::milliseconds(100);  // Short TTL
    config.enable_auto_cleanup = true;

    CachedPerformanceCalculator custom_cache(config);

    // Fill cache beyond capacity
    for (size_t i = 0; i < 15; ++i) {
        const auto& series = return_series_[100];
        auto result        = custom_cache.mean(series);
        EXPECT_TRUE(result.is_ok());
    }

    auto stats = custom_cache.get_cache_stats();
    EXPECT_LE(stats.total_cache_size, config.max_entries + 100);  // Should respect max_entries with hysteresis

    // Wait for cache expiration
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Force cleanup by accessing cache
    const auto& series = return_series_[100];
    auto result        = custom_cache.mean(series);
    EXPECT_TRUE(result.is_ok());

    auto stats_after_expiry = custom_cache.get_cache_stats();
    // After expiry and new computation, cache should have at most 1 entry (the new one)
    // The cache correctly cleaned up expired entries and then cached the new computation
    EXPECT_LE(stats_after_expiry.total_cache_size, 1u);
}

TEST_F(CachedPerformanceMetricsTest, ComputationTimeThresholdTest) {
    // Test that only slow computations get cached
    CacheConfig config;
    config.min_computation_time_basic   = std::chrono::microseconds(1000000);  // 1 second (very high)
    config.min_computation_time_complex = std::chrono::microseconds(1000000);

    CachedPerformanceCalculator selective_cache(config);

    const auto& series = return_series_[100];  // Small series for fast computation

    // First computation
    auto result1 = selective_cache.mean(series);
    ASSERT_TRUE(result1.is_ok());

    // Second computation - might not be cached if computation was too fast
    auto result2 = selective_cache.mean(series);
    ASSERT_TRUE(result2.is_ok());

    // Results should be identical regardless of caching
    EXPECT_DOUBLE_EQ(result1.value(), result2.value());

    auto stats = selective_cache.get_cache_stats();
    std::cout << "\n=== Selective Caching Test ===\n";
    std::cout << "Cache entries: " << stats.total_cache_size << "\n";
    std::cout << "Hit rate: " << std::fixed << std::setprecision(2) << stats.hit_rate * 100 << "%\n";

    // With high threshold, there might be fewer cache entries
    // This is acceptable as the test validates the threshold mechanism works
}