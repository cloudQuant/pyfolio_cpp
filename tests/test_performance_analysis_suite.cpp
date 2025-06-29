#include <chrono>
#include <gtest/gtest.h>
#include <iomanip>
#include <pyfolio/analytics/performance_analysis_suite.h>
#include <pyfolio/core/time_series.h>
#include <random>
#include <vector>

using namespace pyfolio;
using namespace pyfolio::analytics;

class PerformanceAnalysisSuiteTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Generate realistic portfolio returns
        std::mt19937 gen(42);
        std::normal_distribution<double> returns_dist(0.0005, 0.012);    // ~12% annual vol, positive drift
        std::normal_distribution<double> benchmark_dist(0.0003, 0.010);  // Market benchmark

        DateTime base_date = DateTime::parse("2023-01-01").value();

        std::vector<DateTime> dates;
        std::vector<double> portfolio_returns, benchmark_returns;

        // Generate 252 trading days (1 year)
        for (size_t i = 0; i < 252; ++i) {
            dates.push_back(base_date.add_days(i));
            portfolio_returns.push_back(returns_dist(gen));
            benchmark_returns.push_back(benchmark_dist(gen));
        }

        portfolio_series_ = TimeSeries<double>(dates, portfolio_returns, "portfolio");
        benchmark_series_ = TimeSeries<double>(dates, benchmark_returns, "benchmark");

        // Configure analysis suite for testing
        AnalysisConfig config;
        config.risk_free_rate          = 0.02;
        config.periods_per_year        = 252;
        config.rolling_windows         = {30, 60, 90};
        config.min_sharpe_threshold    = 0.5;
        config.max_drawdown_threshold  = 0.15;
        config.enable_detailed_reports = true;

        suite_ = std::make_unique<PerformanceAnalysisSuite>(config);
    }

    TimeSeries<double> portfolio_series_;
    TimeSeries<double> benchmark_series_;
    std::unique_ptr<PerformanceAnalysisSuite> suite_;
};

TEST_F(PerformanceAnalysisSuiteTest, ComprehensiveAnalysis) {
    auto start_time = std::chrono::high_resolution_clock::now();

    auto result = suite_->analyze_performance(portfolio_series_, benchmark_series_);

    auto end_time         = std::chrono::high_resolution_clock::now();
    auto computation_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    ASSERT_TRUE(result.is_ok());
    const auto& report = result.value();

    std::cout << "\n=== Comprehensive Performance Analysis Report ===\n";
    std::cout << std::fixed << std::setprecision(4);

    // Basic Performance Metrics
    std::cout << "\n--- Basic Performance Metrics ---\n";
    std::cout << "Total Return:        " << (report.total_return * 100) << "%\n";
    std::cout << "Annual Return:       " << (report.annual_return * 100) << "%\n";
    std::cout << "Annual Volatility:   " << (report.annual_volatility * 100) << "%\n";
    std::cout << "Sharpe Ratio:        " << report.sharpe_ratio << "\n";
    std::cout << "Sortino Ratio:       " << report.sortino_ratio << "\n";
    std::cout << "Max Drawdown:        " << (report.max_drawdown * 100) << "%\n";
    std::cout << "Calmar Ratio:        " << report.calmar_ratio << "\n";

    // Risk Metrics
    std::cout << "\n--- Risk Metrics ---\n";
    std::cout << "VaR (95%):           " << (report.var_95 * 100) << "%\n";
    std::cout << "CVaR (95%):          " << (report.cvar_95 * 100) << "%\n";
    std::cout << "Downside Deviation:  " << (report.downside_deviation * 100) << "%\n";
    std::cout << "Skewness:            " << report.skewness << "\n";
    std::cout << "Kurtosis:            " << report.kurtosis << "\n";

    // Benchmark Comparison
    if (report.alpha && report.beta) {
        std::cout << "\n--- Benchmark Comparison ---\n";
        std::cout << "Alpha:               " << (report.alpha.value() * 100) << "%\n";
        std::cout << "Beta:                " << report.beta.value() << "\n";
        if (report.information_ratio) {
            std::cout << "Information Ratio:   " << report.information_ratio.value() << "\n";
        }
        if (report.tracking_error) {
            std::cout << "Tracking Error:      " << (report.tracking_error.value() * 100) << "%\n";
        }
    }

    // Rolling Metrics Summary
    std::cout << "\n--- Rolling Metrics Available ---\n";
    for (const auto& [window, series] : report.rolling_returns) {
        std::cout << "Rolling Returns (" << window << " days): " << series.size() << " data points\n";
    }
    for (const auto& [window, series] : report.rolling_volatility) {
        std::cout << "Rolling Volatility (" << window << " days): " << series.size() << " data points\n";
    }
    for (const auto& [window, series] : report.rolling_sharpe) {
        std::cout << "Rolling Sharpe (" << window << " days): " << series.size() << " data points\n";
    }

    // Cache Performance
    std::cout << "\n--- Cache Performance ---\n";
    std::cout << "Cache Hits:          " << report.cache_stats.total_hits << "\n";
    std::cout << "Cache Misses:        " << report.cache_stats.total_misses << "\n";
    std::cout << "Hit Rate:            " << (report.cache_stats.hit_rate * 100) << "%\n";
    std::cout << "Cache Size:          " << report.cache_stats.total_cache_size << " entries\n";

    // Risk Analysis
    std::cout << "\n--- Risk Analysis ---\n";
    std::cout << "Passed Risk Checks:  " << (report.passed_risk_checks ? "YES" : "NO") << "\n";

    if (!report.warnings.empty()) {
        std::cout << "\nWarnings:\n";
        for (const auto& warning : report.warnings) {
            std::cout << "  - " << warning << "\n";
        }
    }

    if (!report.recommendations.empty()) {
        std::cout << "\nRecommendations:\n";
        for (const auto& rec : report.recommendations) {
            std::cout << "  - " << rec << "\n";
        }
    }

    // Performance
    std::cout << "\n--- Computation Performance ---\n";
    std::cout << "Analysis Time:       " << report.computation_time.count() << " ms\n";
    std::cout << "Test Measured Time:  " << computation_time.count() << " ms\n";

    // Validate results
    EXPECT_GT(report.annual_return, -1.0);   // Reasonable return range
    EXPECT_LT(report.annual_return, 5.0);    // Not unreasonably high
    EXPECT_GT(report.annual_volatility, 0);  // Positive volatility
    EXPECT_GE(report.max_drawdown, 0);       // Non-negative drawdown
    EXPECT_LE(report.max_drawdown, 1.0);     // Drawdown <= 100%

    // Validate rolling metrics were calculated
    EXPECT_FALSE(report.rolling_returns.empty());
    EXPECT_FALSE(report.rolling_volatility.empty());
    EXPECT_FALSE(report.rolling_sharpe.empty());

    // Validate benchmark comparison
    EXPECT_TRUE(report.alpha.has_value());
    EXPECT_TRUE(report.beta.has_value());
    EXPECT_TRUE(report.information_ratio.has_value());
    EXPECT_TRUE(report.tracking_error.has_value());
}

TEST_F(PerformanceAnalysisSuiteTest, PerformanceComparison) {
    std::cout << "\n=== Performance Analysis Speed Test ===\n";

    const int num_iterations = 10;
    std::vector<double> analysis_times;

    // Warm up cache
    auto warmup_result = suite_->analyze_performance(portfolio_series_, benchmark_series_);
    ASSERT_TRUE(warmup_result.is_ok());

    // Measure performance over multiple iterations
    for (int i = 0; i < num_iterations; ++i) {
        auto start_time = std::chrono::high_resolution_clock::now();

        auto result = suite_->analyze_performance(portfolio_series_, benchmark_series_);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(end_time - start_time).count();

        ASSERT_TRUE(result.is_ok());
        analysis_times.push_back(duration);
    }

    // Calculate statistics
    double total_time = 0;
    double min_time   = analysis_times[0];
    double max_time   = analysis_times[0];

    for (double time : analysis_times) {
        total_time += time;
        min_time = std::min(min_time, time);
        max_time = std::max(max_time, time);
    }

    double avg_time = total_time / num_iterations;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Iterations:          " << num_iterations << "\n";
    std::cout << "Average Time:        " << avg_time << " ms\n";
    std::cout << "Min Time:            " << min_time << " ms\n";
    std::cout << "Max Time:            " << max_time << " ms\n";
    std::cout << "Total Time:          " << total_time << " ms\n";

    auto suite_stats = suite_->get_performance_stats();
    std::cout << "\n--- Suite Performance Statistics ---\n";
    std::cout << "Total Analyses:      " << suite_stats.total_analyses << "\n";
    std::cout << "Suite Avg Time:      " << suite_stats.average_analysis_time_ms << " ms\n";
    std::cout << "Cache Hit Rate:      " << (suite_stats.cache_stats.hit_rate * 100) << "%\n";

    // Performance should be reasonable
    EXPECT_LT(avg_time, 100.0);                        // Should complete in less than 100ms on average
    EXPECT_GT(suite_stats.cache_stats.hit_rate, 0.5);  // Should have decent cache hit rate
}

TEST_F(PerformanceAnalysisSuiteTest, CacheEfficiency) {
    std::cout << "\n=== Cache Efficiency Analysis ===\n";

    // Clear cache and perform initial analysis
    suite_->clear_cache();

    auto result1 = suite_->analyze_performance(portfolio_series_);
    ASSERT_TRUE(result1.is_ok());

    auto stats1 = suite_->get_performance_stats();
    std::cout << "After first analysis:\n";
    std::cout << "  Cache hits:        " << stats1.cache_stats.total_hits << "\n";
    std::cout << "  Cache misses:      " << stats1.cache_stats.total_misses << "\n";
    std::cout << "  Hit rate:          " << (stats1.cache_stats.hit_rate * 100) << "%\n";

    // Perform second analysis (should benefit from cache)
    auto result2 = suite_->analyze_performance(portfolio_series_);
    ASSERT_TRUE(result2.is_ok());

    auto stats2 = suite_->get_performance_stats();
    std::cout << "\nAfter second analysis:\n";
    std::cout << "  Cache hits:        " << stats2.cache_stats.total_hits << "\n";
    std::cout << "  Cache misses:      " << stats2.cache_stats.total_misses << "\n";
    std::cout << "  Hit rate:          " << (stats2.cache_stats.hit_rate * 100) << "%\n";

    // Perform multiple analyses to show cache efficiency
    for (int i = 0; i < 5; ++i) {
        auto result = suite_->analyze_performance(portfolio_series_);
        ASSERT_TRUE(result.is_ok());
    }

    auto final_stats = suite_->get_performance_stats();
    std::cout << "\nAfter multiple analyses:\n";
    std::cout << "  Total analyses:    " << final_stats.total_analyses << "\n";
    std::cout << "  Cache hits:        " << final_stats.cache_stats.total_hits << "\n";
    std::cout << "  Cache misses:      " << final_stats.cache_stats.total_misses << "\n";
    std::cout << "  Final hit rate:    " << (final_stats.cache_stats.hit_rate * 100) << "%\n";
    std::cout << "  Avg analysis time: " << final_stats.average_analysis_time_ms << " ms\n";

    // Cache efficiency should improve over time
    EXPECT_GT(final_stats.cache_stats.hit_rate, stats1.cache_stats.hit_rate);
    EXPECT_GT(final_stats.cache_stats.total_hits, stats2.cache_stats.total_hits);
}

TEST_F(PerformanceAnalysisSuiteTest, RiskAnalysisValidation) {
    // Create a portfolio with known risk characteristics
    std::mt19937 gen(123);
    std::normal_distribution<double> high_vol_dist(0.001, 0.030);  // High volatility

    DateTime base_date = DateTime::parse("2023-01-01").value();
    std::vector<DateTime> dates;
    std::vector<double> risky_returns;

    for (size_t i = 0; i < 252; ++i) {
        dates.push_back(base_date.add_days(i));
        double ret = high_vol_dist(gen);
        // Add some negative tail events
        if (i % 20 == 0)
            ret -= 0.05;  // 5% loss every 20 days
        risky_returns.push_back(ret);
    }

    TimeSeries<double> risky_portfolio(dates, risky_returns, "risky_portfolio");

    auto result = suite_->analyze_performance(risky_portfolio);
    ASSERT_TRUE(result.is_ok());

    const auto& report = result.value();

    std::cout << "\n=== Risk Analysis Validation ===\n";
    std::cout << "Risk Checks Passed: " << (report.passed_risk_checks ? "YES" : "NO") << "\n";
    std::cout << "Number of Warnings: " << report.warnings.size() << "\n";
    std::cout << "Number of Recommendations: " << report.recommendations.size() << "\n";

    std::cout << "\nKey Risk Metrics:\n";
    std::cout << "  Sharpe Ratio:      " << report.sharpe_ratio << "\n";
    std::cout << "  Max Drawdown:      " << (report.max_drawdown * 100) << "%\n";
    std::cout << "  Annual Volatility: " << (report.annual_volatility * 100) << "%\n";
    std::cout << "  Skewness:          " << report.skewness << "\n";
    std::cout << "  Kurtosis:          " << report.kurtosis << "\n";

    if (!report.warnings.empty()) {
        std::cout << "\nWarnings Generated:\n";
        for (const auto& warning : report.warnings) {
            std::cout << "  - " << warning << "\n";
        }
    }

    // Risky portfolio should trigger warnings
    EXPECT_FALSE(report.passed_risk_checks);
    EXPECT_FALSE(report.warnings.empty());
    EXPECT_FALSE(report.recommendations.empty());
    EXPECT_GT(report.annual_volatility, 0.20);  // Should be high volatility
}

TEST_F(PerformanceAnalysisSuiteTest, GlobalConvenienceFunction) {
    // Test the global convenience function
    auto result = analyze_portfolio_performance(portfolio_series_, benchmark_series_);

    ASSERT_TRUE(result.is_ok());
    const auto& report = result.value();

    std::cout << "\n=== Global Function Test ===\n";
    std::cout << "Analysis completed successfully using global function\n";
    std::cout << "Annual Return: " << (report.annual_return * 100) << "%\n";
    std::cout << "Sharpe Ratio:  " << report.sharpe_ratio << "\n";
    std::cout << "Cache Stats:   " << report.cache_stats.total_hits << " hits, " << report.cache_stats.total_misses
              << " misses\n";

    // Should have valid results
    EXPECT_GT(report.annual_volatility, 0);
    EXPECT_TRUE(report.alpha.has_value());
    EXPECT_TRUE(report.beta.has_value());
}