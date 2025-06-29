#include <chrono>
#include <gtest/gtest.h>
#include <pyfolio/analytics/performance_metrics.h>
#include <pyfolio/analytics/statistics.h>
#include <pyfolio/risk/var.h>
#include <pyfolio/transactions/round_trips.h>

using namespace pyfolio;

class PerformanceBenchmark : public ::testing::Test {
  protected:
    void SetUp() override {
        // Create large dataset for performance testing
        DateTime base_date = DateTime::parse("2020-01-01").value();

        std::mt19937 gen(42);
        std::normal_distribution<> dist(0.0005, 0.015);

        // Generate 5 years of daily returns (1260 trading days)
        for (int i = 0; i < 1800; ++i) {
            DateTime current_date = base_date.add_days(i);
            if (current_date.is_weekday()) {
                dates.push_back(current_date);
                returns.push_back(dist(gen));
                benchmark_returns.push_back(dist(gen) * 0.8 + 0.0002);
            }
        }

        returns_ts   = TimeSeries<Return>(dates, returns);
        benchmark_ts = TimeSeries<Return>(dates, benchmark_returns);

        // Generate large transaction dataset
        generateLargeTransactionSet();
    }

    void generateLargeTransactionSet() {
        std::mt19937 gen(43);
        std::vector<std::string> symbols = {"AAPL", "MSFT", "GOOGL", "AMZN", "TSLA",
                                            "META", "NVDA", "NFLX",  "ADBE", "CRM"};
        std::uniform_int_distribution<> symbol_dist(0, symbols.size() - 1);
        std::uniform_real_distribution<> price_dist(50.0, 500.0);
        std::uniform_real_distribution<> shares_dist(10.0, 1000.0);
        std::uniform_int_distribution<> direction_dist(0, 1);

        // Generate 10,000 transactions
        for (int i = 0; i < 10000; ++i) {
            size_t date_idx    = i % dates.size();
            std::string symbol = symbols[symbol_dist(gen)];
            double price       = price_dist(gen);
            double shares      = shares_dist(gen);
            if (direction_dist(gen) == 1)
                shares = -shares;

            TransactionType type = shares > 0 ? TransactionType::Buy : TransactionType::Sell;

            TransactionRecord txn{symbol, dates[date_idx], shares, price, type, "USD"};
            large_txn_series.add_transaction(txn);
        }
    }

    std::vector<DateTime> dates;
    std::vector<Return> returns;
    std::vector<Return> benchmark_returns;
    TimeSeries<Return> returns_ts;
    TimeSeries<Return> benchmark_ts;
    TransactionSeries large_txn_series;
};

TEST_F(PerformanceBenchmark, ComprehensiveMetricsPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    // Run comprehensive metrics calculation 100 times
    for (int i = 0; i < 100; ++i) {
        auto metrics = PerformanceMetrics::calculate_comprehensive_metrics(returns_ts, benchmark_ts, 0.02);
        ASSERT_TRUE(metrics.is_ok());
    }

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 100 iterations in less than 1 second
    EXPECT_LT(duration.count(), 1000);

    std::cout << "Comprehensive metrics (100 iterations): " << duration.count() << "ms" << std::endl;
}

TEST_F(PerformanceBenchmark, StatisticsCalculationPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    // Run statistics calculations 1000 times
    for (int i = 0; i < 1000; ++i) {
        auto stats = Statistics::calculate_basic_stats(returns_ts);
        ASSERT_TRUE(stats.is_ok());

        auto sharpe = Statistics::sharpe_ratio(returns_ts, 0.02);
        ASSERT_TRUE(sharpe.is_ok());

        auto vol = Statistics::volatility(returns_ts);
        ASSERT_TRUE(vol.is_ok());
    }

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 1000 iterations in less than 500ms
    EXPECT_LT(duration.count(), 500);

    std::cout << "Basic statistics (1000 iterations): " << duration.count() << "ms" << std::endl;
}

TEST_F(PerformanceBenchmark, VaRCalculationPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    VaRCalculator var_calc;

    // Run VaR calculations 100 times
    for (int i = 0; i < 100; ++i) {
        auto hist_var = var_calc.historical_var(returns_ts, 0.05);
        ASSERT_TRUE(hist_var.is_ok());

        auto param_var = var_calc.parametric_var(returns_ts, 0.05);
        ASSERT_TRUE(param_var.is_ok());

        auto cf_var = var_calc.cornish_fisher_var(returns_ts, 0.05);
        ASSERT_TRUE(cf_var.is_ok());
    }

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 100 iterations in less than 200ms
    EXPECT_LT(duration.count(), 200);

    std::cout << "VaR calculations (100 iterations): " << duration.count() << "ms" << std::endl;
}

TEST_F(PerformanceBenchmark, TransactionProcessingPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    // Process large transaction series 10 times
    for (int i = 0; i < 10; ++i) {
        auto stats = large_txn_series.calculate_statistics();
        ASSERT_TRUE(stats.is_ok());

        auto total_notional = large_txn_series.total_notional_value();
        ASSERT_TRUE(total_notional.is_ok());

        auto net_shares = large_txn_series.net_shares_by_symbol();
        ASSERT_TRUE(net_shares.is_ok());
    }

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should process 10,000 transactions 10 times in less than 100ms
    EXPECT_LT(duration.count(), 100);

    std::cout << "Transaction processing (10 iterations, 10k txns each): " << duration.count() << "ms" << std::endl;
}

TEST_F(PerformanceBenchmark, RoundTripExtractionPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    // Extract round trips 10 times
    for (int i = 0; i < 10; ++i) {
        auto round_trips = RoundTripAnalyzer::extract_round_trips(large_txn_series);
        ASSERT_TRUE(round_trips.is_ok());

        auto trips = round_trips.value();
        if (!trips.empty()) {
            auto trip_stats = RoundTripAnalyzer::calculate_statistics(trips);
            ASSERT_TRUE(trip_stats.is_ok());
        }
    }

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should extract round trips 10 times in less than 500ms
    EXPECT_LT(duration.count(), 500);

    std::cout << "Round trip extraction (10 iterations): " << duration.count() << "ms" << std::endl;
}

TEST_F(PerformanceBenchmark, TimeSeriesOperationsPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    // Run time series operations 100 times
    for (int i = 0; i < 100; ++i) {
        auto rolling_mean = returns_ts.rolling_mean(21);
        ASSERT_TRUE(rolling_mean.is_ok());

        auto returns_calc = returns_ts.returns();
        ASSERT_TRUE(returns_calc.is_ok());

        auto cum_returns = returns_ts.cumulative_returns();
        ASSERT_TRUE(cum_returns.is_ok());

        auto mean = returns_ts.mean();
        ASSERT_TRUE(mean.is_ok());

        auto std_dev = returns_ts.std();
        ASSERT_TRUE(std_dev.is_ok());
    }

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 100 iterations in less than 200ms
    EXPECT_LT(duration.count(), 200);

    std::cout << "Time series operations (100 iterations): " << duration.count() << "ms" << std::endl;
}

TEST_F(PerformanceBenchmark, MemoryEfficiencyTest) {
    // Test that we can handle large datasets without excessive memory usage
    std::vector<TimeSeries<Return>> large_datasets;

    auto start = std::chrono::high_resolution_clock::now();

    // Create 100 copies of our large dataset
    for (int i = 0; i < 100; ++i) {
        large_datasets.push_back(returns_ts);

        // Perform operations on each
        auto metrics = PerformanceMetrics::annual_return(large_datasets.back());
        ASSERT_TRUE(metrics.is_ok());
    }

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should handle 100 large datasets in reasonable time
    EXPECT_LT(duration.count(), 1000);

    std::cout << "Memory efficiency test (100 large datasets): " << duration.count() << "ms" << std::endl;

    // Clean up explicitly
    large_datasets.clear();
}

TEST_F(PerformanceBenchmark, ScalabilityTest) {
    // Test performance with different dataset sizes
    std::vector<size_t> sizes = {100, 500, 1000, 2500, 5000};

    for (size_t size : sizes) {
        // Create subset of data
        std::vector<DateTime> subset_dates(dates.begin(), dates.begin() + size);
        std::vector<Return> subset_returns(returns.begin(), returns.begin() + size);
        TimeSeries<Return> subset_ts(subset_dates, subset_returns);

        auto start = std::chrono::high_resolution_clock::now();

        // Run comprehensive analysis
        auto metrics = PerformanceMetrics::calculate_comprehensive_metrics(subset_ts, subset_ts, 0.02);
        ASSERT_TRUE(metrics.is_ok());

        auto end      = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "Dataset size " << size << ": " << duration.count() << "μs" << std::endl;

        // Performance should scale reasonably (allow for some variance)
        EXPECT_LT(duration.count(), size * 10);  // Less than 10μs per data point
    }
}

TEST_F(PerformanceBenchmark, ConcurrentOperationsTest) {
    // Test thread safety and concurrent performance
    const int num_threads           = 4;
    const int iterations_per_thread = 25;

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    std::atomic<int> completed_operations{0};

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < iterations_per_thread; ++i) {
                // Different operations on different threads
                if (t % 4 == 0) {
                    auto sharpe = Statistics::sharpe_ratio(returns_ts, 0.02);
                    ASSERT_TRUE(sharpe.is_ok());
                } else if (t % 4 == 1) {
                    auto vol = Statistics::volatility(returns_ts);
                    ASSERT_TRUE(vol.is_ok());
                } else if (t % 4 == 2) {
                    auto annual_ret = PerformanceMetrics::annual_return(returns_ts);
                    ASSERT_TRUE(annual_ret.is_ok());
                } else {
                    VaRCalculator var_calc;
                    auto var_result = var_calc.historical_var(returns_ts, 0.05);
                    ASSERT_TRUE(var_result.is_ok());
                }

                completed_operations.fetch_add(1);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(completed_operations.load(), num_threads * iterations_per_thread);

    // Concurrent operations should complete efficiently
    EXPECT_LT(duration.count(), 500);

    std::cout << "Concurrent operations (" << num_threads << " threads, " << iterations_per_thread
              << " ops each): " << duration.count() << "ms" << std::endl;
}

TEST_F(PerformanceBenchmark, RealWorldWorkflowPerformance) {
    // Simulate a complete real-world analysis workflow
    auto start = std::chrono::high_resolution_clock::now();

    // 1. Basic performance metrics
    auto annual_return = PerformanceMetrics::annual_return(returns_ts);
    auto annual_vol    = PerformanceMetrics::annual_volatility(returns_ts);
    auto sharpe        = PerformanceMetrics::sharpe_ratio(returns_ts, 0.02);
    auto max_dd        = PerformanceMetrics::max_drawdown(returns_ts);

    ASSERT_TRUE(annual_return.is_ok());
    ASSERT_TRUE(annual_vol.is_ok());
    ASSERT_TRUE(sharpe.is_ok());
    ASSERT_TRUE(max_dd.is_ok());

    // 2. Risk analysis
    VaRCalculator var_calc;
    auto var_95 = var_calc.historical_var(returns_ts, 0.05);
    auto var_99 = var_calc.historical_var(returns_ts, 0.01);
    auto cvar   = var_calc.conditional_var(returns_ts, 0.05);

    ASSERT_TRUE(var_95.is_ok());
    ASSERT_TRUE(var_99.is_ok());
    ASSERT_TRUE(cvar.is_ok());

    // 3. Alpha/beta analysis
    auto alpha_beta     = PerformanceMetrics::alpha_beta(returns_ts, benchmark_ts, 0.02);
    auto tracking_error = PerformanceMetrics::tracking_error(returns_ts, benchmark_ts);
    auto info_ratio     = PerformanceMetrics::information_ratio(returns_ts, benchmark_ts);

    ASSERT_TRUE(alpha_beta.is_ok());
    ASSERT_TRUE(tracking_error.is_ok());
    ASSERT_TRUE(info_ratio.is_ok());

    // 4. Transaction analysis
    auto txn_stats   = large_txn_series.calculate_statistics();
    auto round_trips = RoundTripAnalyzer::extract_round_trips(large_txn_series);

    ASSERT_TRUE(txn_stats.is_ok());
    ASSERT_TRUE(round_trips.is_ok());

    if (!round_trips.value().empty()) {
        auto trip_stats = RoundTripAnalyzer::calculate_statistics(round_trips.value());
        ASSERT_TRUE(trip_stats.is_ok());
    }

    // 5. Time series analysis
    auto rolling_sharpe  = PerformanceMetrics::rolling_sharpe(returns_ts, 63, 0.02);  // Quarterly
    auto cum_returns     = PerformanceMetrics::cumulative_returns(returns_ts);
    auto drawdown_series = PerformanceMetrics::drawdown_series(returns_ts);

    ASSERT_TRUE(rolling_sharpe.is_ok());
    ASSERT_TRUE(cum_returns.is_ok());
    ASSERT_TRUE(drawdown_series.is_ok());

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Complete workflow should finish in less than 100ms
    EXPECT_LT(duration.count(), 100);

    std::cout << "Complete real-world workflow: " << duration.count() << "ms" << std::endl;
}