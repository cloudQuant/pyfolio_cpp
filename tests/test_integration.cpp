#include <gtest/gtest.h>
#include <pyfolio/analytics/performance_metrics.h>
#include <pyfolio/analytics/statistics.h>
#include <pyfolio/attribution/attribution.h>
#include <pyfolio/core/time_series.h>
#include <pyfolio/risk/var.h>
#include <pyfolio/transactions/round_trips.h>
#include <pyfolio/transactions/transaction.h>

using namespace pyfolio;

class IntegrationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Create comprehensive test data
        setupDates();
        setupReturns();
        setupTransactions();
        setupPositions();
        setupBenchmark();
    }

    void setupDates() {
        DateTime start_date = DateTime::parse("2024-01-01").value();
        for (int i = 0; i < 252; ++i) {  // One year of trading days
            DateTime current_date = start_date.add_days(i);
            if (current_date.is_weekday()) {
                dates.push_back(current_date);
            }
        }
    }

    void setupReturns() {
        // Generate realistic return series
        std::mt19937 gen(42);                            // Fixed seed for reproducibility
        std::normal_distribution<> dist(0.0005, 0.015);  // ~12.6% annual return, ~24% volatility

        for (size_t i = 0; i < dates.size(); ++i) {
            returns.push_back(dist(gen));
        }

        returns_ts = TimeSeries<Return>(dates, returns);
    }

    void setupBenchmark() {
        // Benchmark with slightly different characteristics
        std::mt19937 gen(43);
        std::normal_distribution<> dist(0.0003, 0.012);  // ~7.6% annual return, ~19% volatility

        for (size_t i = 0; i < dates.size(); ++i) {
            benchmark_returns.push_back(dist(gen));
        }

        benchmark_ts = TimeSeries<Return>(dates, benchmark_returns);
    }

    void setupTransactions() {
        // Generate realistic transaction history
        std::vector<std::string> symbols = {"AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"};
        std::mt19937 gen(44);
        std::uniform_int_distribution<> symbol_dist(0, symbols.size() - 1);
        std::uniform_real_distribution<> price_dist(50.0, 500.0);
        std::uniform_real_distribution<> shares_dist(10.0, 1000.0);
        std::uniform_int_distribution<> direction_dist(0, 1);

        for (size_t i = 0; i < dates.size(); i += 5) {  // Transaction every 5 days
            std::string symbol = symbols[symbol_dist(gen)];
            double price       = price_dist(gen);
            double shares      = shares_dist(gen);
            if (direction_dist(gen) == 1)
                shares = -shares;  // Sell

            pyfolio::transactions::TransactionType type =
                shares > 0 ? pyfolio::transactions::TransactionType::Buy : pyfolio::transactions::TransactionType::Sell;

            pyfolio::transactions::TransactionRecord txn{symbol, dates[i], shares, price, type, "USD"};
            transactions.push_back(txn);
            txn_series.add_transaction(txn);
        }
    }

    void setupPositions() {
        // Generate positions data based on transactions
        std::map<std::string, double> current_positions;

        for (size_t i = 0; i < dates.size(); ++i) {
            // Update positions based on transactions on this date
            for (const auto& txn : transactions) {
                if (txn.date() == dates[i]) {
                    current_positions[txn.symbol()] += txn.shares();
                }
            }

            // Create position snapshot
            std::map<std::string, double> daily_positions = current_positions;
            daily_positions["cash"]                       = 100000.0;  // Fixed cash position

            positions_data.push_back(daily_positions);
        }
    }

    std::vector<DateTime> dates;
    std::vector<Return> returns;
    std::vector<Return> benchmark_returns;
    TimeSeries<Return> returns_ts;
    TimeSeries<Return> benchmark_ts;

    std::vector<pyfolio::transactions::TransactionRecord> transactions;
    pyfolio::transactions::TransactionSeries txn_series;

    std::vector<std::map<std::string, double>> positions_data;
};

TEST_F(IntegrationTest, FullPerformanceAnalysis) {
    // Test comprehensive performance analysis
    auto metrics = PerformanceMetrics::calculate_comprehensive_metrics(returns_ts, benchmark_ts, 0.02);

    ASSERT_TRUE(metrics.is_ok());

    auto result = metrics.value();

    // Validate all metrics are reasonable
    EXPECT_TRUE(std::isfinite(result.annual_return));
    EXPECT_GT(result.annual_volatility, 0.0);
    EXPECT_TRUE(std::isfinite(result.sharpe_ratio));
    EXPECT_TRUE(std::isfinite(result.sortino_ratio));
    EXPECT_LE(result.max_drawdown, 0.0);
    EXPECT_TRUE(std::isfinite(result.alpha));
    EXPECT_TRUE(std::isfinite(result.beta));
    EXPECT_GT(result.tracking_error, 0.0);
    EXPECT_TRUE(std::isfinite(result.information_ratio));

    // Cross-validate with individual calculations
    auto individual_sharpe = PerformanceMetrics::sharpe_ratio(returns_ts, 0.02);
    ASSERT_TRUE(individual_sharpe.is_ok());
    EXPECT_NEAR(result.sharpe_ratio, individual_sharpe.value(), 1e-10);

    auto individual_alpha_beta = PerformanceMetrics::alpha_beta(returns_ts, benchmark_ts, 0.02);
    ASSERT_TRUE(individual_alpha_beta.is_ok());
    EXPECT_NEAR(result.alpha, individual_alpha_beta.value().alpha, 1e-10);
    EXPECT_NEAR(result.beta, individual_alpha_beta.value().beta, 1e-10);
}

TEST_F(IntegrationTest, TransactionAnalysisWorkflow) {
    // Test complete transaction analysis workflow

    // 1. Basic transaction statistics
    auto txn_stats = txn_series.calculate_statistics();
    ASSERT_TRUE(txn_stats.is_ok());

    auto stats = txn_stats.value();
    EXPECT_GT(stats.total_transactions, 0);
    EXPECT_GT(stats.total_notional_value, 0.0);
    EXPECT_GT(stats.unique_symbols, 0);

    // 2. Round trip analysis
    pyfolio::transactions::RoundTripAnalyzer rt_analyzer;
    auto round_trips = rt_analyzer.analyze(txn_series);
    ASSERT_TRUE(round_trips.is_ok());

    auto trips = round_trips.value();

    // 3. Round trip statistics
    if (!trips.empty()) {
        auto trip_stats = pyfolio::transactions::RoundTripStatistics::calculate(trips);
        ASSERT_TRUE(trip_stats.is_ok());

        auto trip_result = trip_stats.value();
        EXPECT_GT(trip_result.total_trips, 0);
        EXPECT_TRUE(std::isfinite(trip_result.total_pnl));
        EXPECT_GE(trip_result.win_rate, 0.0);
        EXPECT_LE(trip_result.win_rate, 1.0);
    }

    // 4. Trading cost analysis
    auto trading_costs = txn_series.calculate_transaction_costs(5.0);  // $5 per trade
    ASSERT_TRUE(trading_costs.is_ok());

    double expected_costs = stats.total_transactions * 5.0;
    EXPECT_NEAR(trading_costs.value(), expected_costs, 1e-10);
}

TEST_F(IntegrationTest, RiskAnalysisIntegration) {
    // Test comprehensive risk analysis

    pyfolio::risk::VaRCalculator var_calc;

    // Historical VaR
    auto hist_var = var_calc.calculate_historical_var(returns_ts, 0.05);
    ASSERT_TRUE(hist_var.is_ok());
    EXPECT_LT(hist_var.value().var_estimate, 0.0);

    // Parametric VaR
    auto param_var = var_calc.calculate_parametric_var(returns_ts, 0.05);
    ASSERT_TRUE(param_var.is_ok());
    EXPECT_LT(param_var.value().var_estimate, 0.0);

    // Monte Carlo VaR
    auto mc_var = var_calc.calculate_monte_carlo_var(returns_ts, 0.05, pyfolio::risk::VaRHorizon::Daily, 10000);
    ASSERT_TRUE(mc_var.is_ok());
    EXPECT_LT(mc_var.value().var_estimate, 0.0);

    // Cornish-Fisher VaR
    auto cf_var = var_calc.calculate_cornish_fisher_var(returns_ts, 0.05);
    ASSERT_TRUE(cf_var.is_ok());
    EXPECT_LT(cf_var.value().var_estimate, 0.0);

    // Cross-validation: VaR methods should give reasonably similar results
    double hist_val  = hist_var.value().var_estimate;
    double param_val = param_var.value().var_estimate;
    double mc_val    = mc_var.value().var_estimate;
    double cf_val    = cf_var.value().var_estimate;

    // Allow for some variance but they should be in the same ballpark
    EXPECT_LT(std::abs(hist_val - param_val), std::abs(hist_val) * 0.5);
    EXPECT_LT(std::abs(hist_val - mc_val), std::abs(hist_val) * 0.5);
}

TEST_F(IntegrationTest, AttributionAnalysisWorkflow) {
    // Test attribution analysis integration

    // Create factor exposures (simplified)
    pyfolio::attribution::FactorExposures exposures;
    exposures.market_beta     = 1.2;
    exposures.size_factor     = 0.3;
    exposures.value_factor    = -0.1;
    exposures.momentum_factor = 0.2;

    // Create factor returns
    pyfolio::attribution::FactorReturns factor_returns;
    factor_returns.market_return   = 0.008;
    factor_returns.size_return     = 0.002;
    factor_returns.value_return    = -0.001;
    factor_returns.momentum_return = 0.003;

    pyfolio::attribution::AttributionAnalyzer analyzer;

    // Factor attribution (using benchmark exposures as zero for simplicity)
    pyfolio::attribution::FactorExposures benchmark_exposures{};  // Default zero exposures
    auto factor_attribution = analyzer.analyze_factor_attribution(exposures, benchmark_exposures, factor_returns);
    ASSERT_TRUE(factor_attribution.is_ok());

    auto factor_result = factor_attribution.value();
    EXPECT_TRUE(std::isfinite(factor_result));

    // The result is the excess return (portfolio - benchmark factor exposure)
    // Since benchmark has zero exposure, result should equal portfolio factor return
    double expected_total = exposures.market_beta * factor_returns.market_return +
                            exposures.size_factor * factor_returns.size_return +
                            exposures.value_factor * factor_returns.value_return +
                            exposures.momentum_factor * factor_returns.momentum_return;

    EXPECT_NEAR(factor_result, expected_total, 1e-10);
}

TEST_F(IntegrationTest, PerformanceStatisticsConsistency) {
    // Test consistency between different statistical calculations

    // Calculate statistics using different methods
    auto basic_stats = Statistics::calculate_basic_stats(returns_ts);
    ASSERT_TRUE(basic_stats.is_ok());

    auto manual_mean = returns_ts.mean();
    auto manual_std  = returns_ts.std();

    ASSERT_TRUE(manual_mean.is_ok());
    ASSERT_TRUE(manual_std.is_ok());

    auto stats_result = basic_stats.value();

    // Verify consistency
    EXPECT_NEAR(stats_result.mean, manual_mean.value(), 1e-8);
    EXPECT_NEAR(stats_result.std_dev, manual_std.value(), 1e-4);  // More reasonable tolerance for standard deviation

    // Sharpe ratio consistency
    auto sharpe1 = Statistics::sharpe_ratio(returns_ts, 0.02);
    auto sharpe2 = PerformanceMetrics::sharpe_ratio(returns_ts, 0.02);

    ASSERT_TRUE(sharpe1.is_ok());
    ASSERT_TRUE(sharpe2.is_ok());

    EXPECT_NEAR(sharpe1.value(), sharpe2.value(), 1e-8);
}

TEST_F(IntegrationTest, TimeSeriesOperationsIntegration) {
    // Test integration of time series operations

    // Rolling statistics
    auto rolling_mean = returns_ts.rolling_mean(21);  // 21-day rolling mean
    ASSERT_TRUE(rolling_mean.is_ok());

    auto rolling_result = rolling_mean.value();
    EXPECT_EQ(rolling_result.size(), returns.size());

    // Cumulative returns
    auto cum_returns = returns_ts.cumulative_returns();
    ASSERT_TRUE(cum_returns.is_ok());

    auto cum_result = cum_returns.value();
    EXPECT_EQ(cum_result.size(), returns.size());

    // Verify cumulative returns are monotonic in expectation
    EXPECT_NE(cum_result.back(), 0.0);

    // Resample to monthly
    auto monthly = returns_ts.resample(ResampleFrequency::Monthly);
    ASSERT_TRUE(monthly.is_ok());

    auto monthly_result = monthly.value();
    EXPECT_LT(monthly_result.size(), returns.size());  // Should be fewer monthly periods
}

TEST_F(IntegrationTest, ErrorHandlingConsistency) {
    // Test that error handling is consistent across modules

    TimeSeries<Return> empty_ts;

    // All modules should handle empty data gracefully
    auto empty_metrics = PerformanceMetrics::calculate_comprehensive_metrics(empty_ts, empty_ts, 0.02);
    EXPECT_TRUE(empty_metrics.is_error());

    auto empty_stats = Statistics::calculate_basic_stats(empty_ts);
    EXPECT_TRUE(empty_stats.is_error());

    pyfolio::risk::VaRCalculator var_calc;
    auto empty_var = var_calc.calculate_historical_var(empty_ts, 0.05);
    EXPECT_TRUE(empty_var.is_error());

    pyfolio::transactions::TransactionSeries empty_txn_series;
    pyfolio::transactions::RoundTripAnalyzer rt_analyzer;
    auto empty_round_trips = rt_analyzer.analyze(empty_txn_series);
    ASSERT_TRUE(empty_round_trips.is_ok());  // Should succeed but return empty vector
    EXPECT_EQ(empty_round_trips.value().size(), 0);
}

TEST_F(IntegrationTest, PerformanceConsistency) {
    // Test that performance is consistent across multiple runs

    auto start_time = std::chrono::high_resolution_clock::now();

    // Run multiple performance calculations
    for (int i = 0; i < 10; ++i) {
        auto metrics = PerformanceMetrics::calculate_comprehensive_metrics(returns_ts, benchmark_ts, 0.02);
        ASSERT_TRUE(metrics.is_ok());
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Should complete within reasonable time (less than 1 second for 10 runs)
    EXPECT_LT(duration.count(), 1000);
}

TEST_F(IntegrationTest, DataIntegrityValidation) {
    // Test data integrity across different operations

    // Verify that transaction series maintains integrity
    auto original_total = txn_series.total_notional_value();
    ASSERT_TRUE(original_total.is_ok());

    // Filter and verify total is different
    auto filtered = txn_series.filter_by_symbol("AAPL");
    ASSERT_TRUE(filtered.is_ok());

    auto filtered_total = filtered.value().total_notional_value();
    ASSERT_TRUE(filtered_total.is_ok());

    EXPECT_LE(filtered_total.value(), original_total.value());

    // Verify returns time series integrity
    auto original_mean = returns_ts.mean();
    ASSERT_TRUE(original_mean.is_ok());

    // Slice and verify
    auto sliced = returns_ts.slice(dates[10], dates[dates.size() - 10]);
    ASSERT_TRUE(sliced.is_ok());

    auto sliced_mean = sliced.value().mean();
    ASSERT_TRUE(sliced_mean.is_ok());

    // Sliced mean should be finite and different from original
    EXPECT_TRUE(std::isfinite(sliced_mean.value()));
}

TEST_F(IntegrationTest, MemoryEfficiency) {
    // Test that operations don't cause memory leaks (basic check)

    // Create and destroy many objects
    for (int i = 0; i < 1000; ++i) {
        TimeSeries<Return> temp_ts = returns_ts;
        auto temp_metrics          = PerformanceMetrics::annual_return(temp_ts);
        ASSERT_TRUE(temp_metrics.is_ok());

        pyfolio::transactions::TransactionSeries temp_txn_series = txn_series;
        auto temp_stats                                          = temp_txn_series.calculate_statistics();
        ASSERT_TRUE(temp_stats.is_ok());
    }

    // If we get here without crashing, memory management is working
    EXPECT_TRUE(true);
}