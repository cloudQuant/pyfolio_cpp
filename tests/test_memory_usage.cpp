#include <gtest/gtest.h>
#include <pyfolio/analytics/performance_metrics.h>
#include <pyfolio/core/time_series.h>
#include <pyfolio/transactions/transaction.h>
#include <random>
#include <sys/resource.h>

using namespace pyfolio;

class MemoryUsageTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Record initial memory usage
        initial_memory = getCurrentMemoryUsage();
    }

    void TearDown() override {
        // Check for memory leaks
        size_t final_memory    = getCurrentMemoryUsage();
        size_t memory_increase = final_memory > initial_memory ? final_memory - initial_memory : 0;

        // Allow for some memory increase but flag excessive growth
        EXPECT_LT(memory_increase, 50 * 1024 * 1024);  // Less than 50MB growth per test
    }

    size_t getCurrentMemoryUsage() {
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        return usage.ru_maxrss;  // Maximum resident set size
    }

    size_t initial_memory;
};

TEST_F(MemoryUsageTest, LargeTimeSeriesMemoryUsage) {
    std::vector<TimeSeries<Return>> large_series_collection;

    // Create many large time series
    DateTime base_date = DateTime::parse("2020-01-01").value();

    for (int series_num = 0; series_num < 100; ++series_num) {
        std::vector<DateTime> dates;
        std::vector<Return> returns;

        // Generate 2000 data points per series
        std::mt19937 gen(42 + series_num);
        std::normal_distribution<> dist(0.0005, 0.015);

        for (int i = 0; i < 2000; ++i) {
            DateTime current_date = base_date.add_days(i + series_num * 10);
            if (current_date.is_weekday()) {
                dates.push_back(current_date);
                returns.push_back(dist(gen));
            }
        }

        large_series_collection.emplace_back(dates, returns);

        // Perform operations to ensure memory is actually used
        auto mean = large_series_collection.back().mean();
        ASSERT_TRUE(mean.is_ok());
    }

    size_t peak_memory = getCurrentMemoryUsage();
    size_t memory_used = peak_memory > initial_memory ? peak_memory - initial_memory : 0;

    std::cout << "Memory used for 100 large time series: " << memory_used / (1024 * 1024) << " MB" << std::endl;

    // Should use reasonable amount of memory (less than 200MB for this test)
    EXPECT_LT(memory_used, 200 * 1024 * 1024);

    // Clean up explicitly
    large_series_collection.clear();

    // Memory should be freed (allow some fragmentation)
    size_t post_cleanup_memory = getCurrentMemoryUsage();
    size_t remaining_memory    = post_cleanup_memory > initial_memory ? post_cleanup_memory - initial_memory : 0;

    EXPECT_LT(remaining_memory, memory_used / 2);  // At least 50% should be freed
}

TEST_F(MemoryUsageTest, TransactionSeriesMemoryUsage) {
    std::vector<TransactionSeries> transaction_collections;

    // Create multiple transaction series with many transactions
    std::vector<std::string> symbols = {"AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"};
    DateTime base_date               = DateTime::parse("2024-01-01").value();

    for (int collection = 0; collection < 10; ++collection) {
        TransactionSeries series;

        std::mt19937 gen(43 + collection);
        std::uniform_int_distribution<> symbol_dist(0, symbols.size() - 1);
        std::uniform_real_distribution<> price_dist(50.0, 500.0);
        std::uniform_real_distribution<> shares_dist(10.0, 1000.0);
        std::uniform_int_distribution<> direction_dist(0, 1);

        // Generate 50,000 transactions per series
        for (int i = 0; i < 50000; ++i) {
            DateTime txn_date  = base_date.add_days(i / 100);
            std::string symbol = symbols[symbol_dist(gen)];
            double price       = price_dist(gen);
            double shares      = shares_dist(gen);
            if (direction_dist(gen) == 1)
                shares = -shares;

            TransactionType type = shares > 0 ? TransactionType::Buy : TransactionType::Sell;

            TransactionRecord txn{symbol, txn_date, shares, price, type, "USD"};
            auto result = series.add_transaction(txn);
            ASSERT_TRUE(result.is_ok());
        }

        transaction_collections.push_back(std::move(series));

        // Perform operations
        auto stats = transaction_collections.back().calculate_statistics();
        ASSERT_TRUE(stats.is_ok());
    }

    size_t peak_memory = getCurrentMemoryUsage();
    size_t memory_used = peak_memory > initial_memory ? peak_memory - initial_memory : 0;

    std::cout << "Memory used for 500k transactions: " << memory_used / (1024 * 1024) << " MB" << std::endl;

    // Should handle large transaction datasets efficiently
    EXPECT_LT(memory_used, 500 * 1024 * 1024);  // Less than 500MB

    transaction_collections.clear();
}

TEST_F(MemoryUsageTest, RepeatedOperationsMemoryStability) {
    // Test that repeated operations don't cause memory leaks
    DateTime base_date = DateTime::parse("2024-01-01").value();

    std::vector<DateTime> dates;
    std::vector<Return> returns;

    std::mt19937 gen(42);
    std::normal_distribution<> dist(0.0005, 0.015);

    for (int i = 0; i < 1000; ++i) {
        DateTime current_date = base_date.add_days(i);
        if (current_date.is_weekday()) {
            dates.push_back(current_date);
            returns.push_back(dist(gen));
        }
    }

    TimeSeries<Return> test_series(dates, returns);

    // Record memory after setup
    size_t setup_memory = getCurrentMemoryUsage();

    // Perform many repeated operations
    for (int iteration = 0; iteration < 1000; ++iteration) {
        // Various performance metrics
        auto annual_return = PerformanceMetrics::annual_return(test_series);
        auto sharpe        = PerformanceMetrics::sharpe_ratio(test_series, 0.02);
        auto vol           = PerformanceMetrics::annual_volatility(test_series);
        auto max_dd        = PerformanceMetrics::max_drawdown(test_series);

        ASSERT_TRUE(annual_return.is_ok());
        ASSERT_TRUE(sharpe.is_ok());
        ASSERT_TRUE(vol.is_ok());
        ASSERT_TRUE(max_dd.is_ok());

        // Time series operations
        auto rolling_mean = test_series.rolling_mean(21);
        auto cum_returns  = test_series.cumulative_returns();

        ASSERT_TRUE(rolling_mean.is_ok());
        ASSERT_TRUE(cum_returns.is_ok());

        // Check memory every 100 iterations
        if (iteration % 100 == 99) {
            size_t current_memory = getCurrentMemoryUsage();
            size_t memory_growth  = current_memory > setup_memory ? current_memory - setup_memory : 0;

            // Memory growth should be minimal for repeated operations
            EXPECT_LT(memory_growth, 10 * 1024 * 1024);  // Less than 10MB growth
        }
    }

    size_t final_memory = getCurrentMemoryUsage();
    size_t total_growth = final_memory > setup_memory ? final_memory - setup_memory : 0;

    std::cout << "Memory growth after 1000 iterations: " << total_growth / 1024 << " KB" << std::endl;

    // Total memory growth should be minimal
    EXPECT_LT(total_growth, 5 * 1024 * 1024);  // Less than 5MB total growth
}

TEST_F(MemoryUsageTest, TemporaryObjectMemoryManagement) {
    // Test that temporary objects are properly cleaned up
    DateTime base_date = DateTime::parse("2024-01-01").value();

    size_t pre_test_memory = getCurrentMemoryUsage();

    // Create and destroy many temporary objects
    for (int i = 0; i < 1000; ++i) {
        std::vector<DateTime> dates;
        std::vector<Return> returns;

        std::mt19937 gen(42 + i);
        std::normal_distribution<> dist(0.0005, 0.015);

        for (int j = 0; j < 500; ++j) {
            dates.push_back(base_date.add_days(j + i * 1000));
            returns.push_back(dist(gen));
        }

        // Create temporary time series
        TimeSeries<Return> temp_series(dates, returns);

        // Perform operations that create more temporaries
        auto metrics = PerformanceMetrics::calculate_comprehensive_metrics(temp_series, temp_series, 0.02);
        ASSERT_TRUE(metrics.is_ok());

        // temp_series and all temporaries should be destroyed here
    }

    size_t post_test_memory = getCurrentMemoryUsage();
    size_t memory_growth    = post_test_memory > pre_test_memory ? post_test_memory - pre_test_memory : 0;

    std::cout << "Memory growth from temporary objects: " << memory_growth / 1024 << " KB" << std::endl;

    // Should not have significant memory growth from temporaries
    EXPECT_LT(memory_growth, 20 * 1024 * 1024);  // Less than 20MB
}

TEST_F(MemoryUsageTest, ConcurrentMemoryUsage) {
    // Test memory usage under concurrent operations
    DateTime base_date = DateTime::parse("2024-01-01").value();

    std::vector<DateTime> dates;
    std::vector<Return> returns;

    std::mt19937 gen(42);
    std::normal_distribution<> dist(0.0005, 0.015);

    for (int i = 0; i < 1000; ++i) {
        DateTime current_date = base_date.add_days(i);
        if (current_date.is_weekday()) {
            dates.push_back(current_date);
            returns.push_back(dist(gen));
        }
    }

    TimeSeries<Return> test_series(dates, returns);

    size_t pre_concurrent_memory = getCurrentMemoryUsage();

    // Run concurrent operations
    const int num_threads           = 4;
    const int operations_per_thread = 100;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&test_series, operations_per_thread]() {
            for (int i = 0; i < operations_per_thread; ++i) {
                auto sharpe  = PerformanceMetrics::sharpe_ratio(test_series, 0.02);
                auto vol     = PerformanceMetrics::annual_volatility(test_series);
                auto mean    = test_series.mean();
                auto std_dev = test_series.std();

                // Ensure operations succeed
                ASSERT_TRUE(sharpe.is_ok());
                ASSERT_TRUE(vol.is_ok());
                ASSERT_TRUE(mean.is_ok());
                ASSERT_TRUE(std_dev.is_ok());
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    size_t post_concurrent_memory = getCurrentMemoryUsage();
    size_t memory_growth =
        post_concurrent_memory > pre_concurrent_memory ? post_concurrent_memory - pre_concurrent_memory : 0;

    std::cout << "Memory growth from concurrent operations: " << memory_growth / 1024 << " KB" << std::endl;

    // Concurrent operations should not cause excessive memory growth
    EXPECT_LT(memory_growth, 15 * 1024 * 1024);  // Less than 15MB
}

TEST_F(MemoryUsageTest, LargeResultSetMemoryUsage) {
    // Test memory usage when operations return large result sets
    DateTime base_date = DateTime::parse("2020-01-01").value();

    std::vector<DateTime> dates;
    std::vector<Return> returns;

    std::mt19937 gen(42);
    std::normal_distribution<> dist(0.0005, 0.015);

    // Create 5 years of daily data
    for (int i = 0; i < 1800; ++i) {
        DateTime current_date = base_date.add_days(i);
        if (current_date.is_weekday()) {
            dates.push_back(current_date);
            returns.push_back(dist(gen));
        }
    }

    TimeSeries<Return> large_series(dates, returns);

    size_t pre_operations_memory = getCurrentMemoryUsage();

    // Operations that return large result sets
    std::vector<std::vector<double>> large_results;

    for (int i = 0; i < 50; ++i) {
        auto rolling_sharpe = PerformanceMetrics::rolling_sharpe(large_series, 21, 0.02);
        ASSERT_TRUE(rolling_sharpe.is_ok());
        large_results.push_back(rolling_sharpe.value());

        auto rolling_vol = large_series.rolling_std(63);
        ASSERT_TRUE(rolling_vol.is_ok());
        large_results.push_back(rolling_vol.value());

        auto cum_returns = large_series.cumulative_returns();
        ASSERT_TRUE(cum_returns.is_ok());
        large_results.push_back(cum_returns.value());
    }

    size_t peak_memory = getCurrentMemoryUsage();
    size_t memory_used = peak_memory > pre_operations_memory ? peak_memory - pre_operations_memory : 0;

    std::cout << "Memory used for large result sets: " << memory_used / (1024 * 1024) << " MB" << std::endl;

    // Should handle large result sets efficiently
    EXPECT_LT(memory_used, 100 * 1024 * 1024);  // Less than 100MB

    // Clean up
    large_results.clear();

    size_t post_cleanup_memory = getCurrentMemoryUsage();
    size_t remaining_memory =
        post_cleanup_memory > pre_operations_memory ? post_cleanup_memory - pre_operations_memory : 0;

    // Most memory should be freed
    EXPECT_LT(remaining_memory, memory_used / 3);  // At least 66% should be freed
}

TEST_F(MemoryUsageTest, ErrorHandlingMemoryUsage) {
    // Test that error conditions don't cause memory leaks
    size_t pre_error_memory = getCurrentMemoryUsage();

    // Create scenarios that should cause errors
    TimeSeries<Return> empty_series;

    for (int i = 0; i < 1000; ++i) {
        // Operations on empty series should fail gracefully
        auto empty_sharpe = PerformanceMetrics::sharpe_ratio(empty_series, 0.02);
        EXPECT_TRUE(empty_sharpe.is_error());

        auto empty_vol = PerformanceMetrics::annual_volatility(empty_series);
        EXPECT_TRUE(empty_vol.is_error());

        auto empty_mean = empty_series.mean();
        EXPECT_TRUE(empty_mean.is_error());

        // Invalid transactions
        auto invalid_txn = TransactionRecord::create("", DateTime::parse("2024-01-01").value(), 0.0, -100.0,
                                                     TransactionType::Buy, "USD");
        EXPECT_TRUE(invalid_txn.is_error());
    }

    size_t post_error_memory = getCurrentMemoryUsage();
    size_t memory_growth     = post_error_memory > pre_error_memory ? post_error_memory - pre_error_memory : 0;

    std::cout << "Memory growth from error handling: " << memory_growth / 1024 << " KB" << std::endl;

    // Error handling should not cause memory leaks
    EXPECT_LT(memory_growth, 1024 * 1024);  // Less than 1MB growth
}