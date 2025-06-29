#include <cmath>
#include <gtest/gtest.h>
#include <pyfolio/core/time_series.h>
#include <pyfolio/core/types.h>
#include <pyfolio/performance/rolling_metrics.h>

using namespace pyfolio;
using namespace pyfolio::performance;

class RollingMetricsTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Generate deterministic test data
        auto start_date = DateTime(2020, 1, 1);

        // Create simple return series: alternating +1% and -0.5%
        returns_.clear();
        benchmark_returns_.clear();

        for (int i = 0; i < 100; ++i) {
            auto date        = DateTime(2020, 1, 1 + i);       // Simple date increment
            double ret       = (i % 2 == 0) ? 0.01 : -0.005;   // +1%, -0.5%
            double bench_ret = (i % 3 == 0) ? 0.008 : -0.003;  // Different pattern

            returns_.push_back(date, ret);
            benchmark_returns_.push_back(date, bench_ret);
        }

        // Create a more volatile series for some tests
        volatile_returns_.clear();
        for (int i = 0; i < 50; ++i) {
            auto date  = DateTime(2020, 1, 1 + i);
            double ret = (i % 2 == 0) ? 0.05 : -0.03;  // +5%, -3%
            volatile_returns_.push_back(date, ret);
        }
    }

    TimeSeries<Return> returns_;
    TimeSeries<Return> benchmark_returns_;
    TimeSeries<Return> volatile_returns_;
};

TEST_F(RollingMetricsTest, TestRollingVolatility) {
    size_t window    = 10;
    auto rolling_vol = calculate_rolling_volatility(returns_, window);

    EXPECT_GT(rolling_vol.size(), 0);
    EXPECT_LE(rolling_vol.size(), returns_.size());

    // All volatility values should be positive
    for (size_t i = 0; i < rolling_vol.size(); ++i) {
        EXPECT_GT(rolling_vol[i], 0.0);
        EXPECT_LT(rolling_vol[i], 10.0);  // Reasonable upper bound
    }

    // Volatile series should have higher volatility
    auto volatile_vol = calculate_rolling_volatility(volatile_returns_, window);

    if (rolling_vol.size() > 0 && volatile_vol.size() > 0) {
        // Get average volatilities
        double avg_vol = 0.0, avg_volatile_vol = 0.0;
        for (size_t i = 0; i < rolling_vol.size(); ++i) {
            avg_vol += rolling_vol[i];
        }
        avg_vol /= rolling_vol.size();

        for (size_t i = 0; i < volatile_vol.size(); ++i) {
            avg_volatile_vol += volatile_vol[i];
        }
        avg_volatile_vol /= volatile_vol.size();

        EXPECT_GT(avg_volatile_vol, avg_vol);
    }
}

TEST_F(RollingMetricsTest, TestRollingSharpe) {
    size_t window         = 20;
    double risk_free_rate = 0.02;  // 2% annual

    auto rolling_sharpe = calculate_rolling_sharpe(returns_, window, risk_free_rate, 252);

    EXPECT_GT(rolling_sharpe.size(), 0);
    EXPECT_LE(rolling_sharpe.size(), returns_.size());

    // Sharpe ratios should be reasonable (between -5 and 5 typically)
    for (size_t i = 0; i < rolling_sharpe.size(); ++i) {
        EXPECT_GT(rolling_sharpe[i], -10.0);
        EXPECT_LT(rolling_sharpe[i], 10.0);
    }
}

TEST_F(RollingMetricsTest, TestRollingBeta) {
    size_t window = 15;

    auto rolling_beta = calculate_rolling_beta(returns_, benchmark_returns_, window);

    EXPECT_GT(rolling_beta.size(), 0);
    EXPECT_LE(rolling_beta.size(), std::min(returns_.size(), benchmark_returns_.size()));

    // Beta values should be reasonable
    for (size_t i = 0; i < rolling_beta.size(); ++i) {
        EXPECT_GT(rolling_beta[i], -5.0);
        EXPECT_LT(rolling_beta[i], 5.0);
    }
}

TEST_F(RollingMetricsTest, TestRollingCorrelation) {
    size_t window = 20;

    auto rolling_corr = calculate_rolling_correlation(returns_, benchmark_returns_, window);

    EXPECT_GT(rolling_corr.size(), 0);
    EXPECT_LE(rolling_corr.size(), std::min(returns_.size(), benchmark_returns_.size()));

    // Correlation values should be between -1 and 1
    for (size_t i = 0; i < rolling_corr.size(); ++i) {
        EXPECT_GE(rolling_corr[i], -1.0);
        EXPECT_LE(rolling_corr[i], 1.0);
    }
}

TEST_F(RollingMetricsTest, TestRollingMaxDrawdown) {
    size_t window = 25;

    auto rolling_dd = calculate_rolling_max_drawdown(returns_, window);

    EXPECT_GT(rolling_dd.size(), 0);
    EXPECT_LE(rolling_dd.size(), returns_.size());

    // Drawdown values should be between 0 and 1
    for (size_t i = 0; i < rolling_dd.size(); ++i) {
        EXPECT_GE(rolling_dd[i], 0.0);
        EXPECT_LE(rolling_dd[i], 1.0);
    }

    // Volatile series should have larger drawdowns
    auto volatile_dd = calculate_rolling_max_drawdown(volatile_returns_, window);

    if (rolling_dd.size() > 0 && volatile_dd.size() > 0) {
        // Get maximum drawdowns
        double max_dd = 0.0, max_volatile_dd = 0.0;
        for (size_t i = 0; i < rolling_dd.size(); ++i) {
            max_dd = std::max(max_dd, rolling_dd[i]);
        }
        for (size_t i = 0; i < volatile_dd.size(); ++i) {
            max_volatile_dd = std::max(max_volatile_dd, volatile_dd[i]);
        }

        EXPECT_GE(max_volatile_dd, max_dd);
    }
}

TEST_F(RollingMetricsTest, TestRollingSortino) {
    size_t window         = 30;
    double risk_free_rate = 0.02;

    auto rolling_sortino = calculate_rolling_sortino(returns_, window, risk_free_rate, 252);

    EXPECT_GT(rolling_sortino.size(), 0);
    EXPECT_LE(rolling_sortino.size(), returns_.size());

    // Sortino ratios should be reasonable
    for (size_t i = 0; i < rolling_sortino.size(); ++i) {
        EXPECT_GT(rolling_sortino[i], -10.0);
        EXPECT_LT(rolling_sortino[i], 10.0);
    }
}

TEST_F(RollingMetricsTest, TestRollingDownsideDeviation) {
    size_t window = 20;
    double mar    = 0.0;  // Minimum acceptable return

    auto rolling_dd = calculate_rolling_downside_deviation(returns_, window, mar, 252);

    EXPECT_GT(rolling_dd.size(), 0);
    EXPECT_LE(rolling_dd.size(), returns_.size());

    // Downside deviation should be non-negative
    for (size_t i = 0; i < rolling_dd.size(); ++i) {
        EXPECT_GE(rolling_dd[i], 0.0);
        EXPECT_LT(rolling_dd[i], 5.0);  // Reasonable upper bound
    }
}

TEST_F(RollingMetricsTest, TestWindowSizeEffects) {
    // Test different window sizes
    std::vector<size_t> windows = {5, 10, 20, 30};

    for (size_t window : windows) {
        auto rolling_vol = calculate_rolling_volatility(returns_, window);

        // Larger windows should produce fewer data points
        EXPECT_GT(rolling_vol.size(), 0);
        EXPECT_LE(rolling_vol.size(), returns_.size());

        // All values should be reasonable
        for (size_t i = 0; i < rolling_vol.size(); ++i) {
            EXPECT_GT(rolling_vol[i], 0.0);
            EXPECT_LT(rolling_vol[i], 10.0);
        }
    }
}

TEST_F(RollingMetricsTest, TestMinPeriods) {
    size_t window      = 20;
    size_t min_periods = 10;

    auto rolling_vol = calculate_rolling_volatility(returns_, window, min_periods);

    // Should start later due to min_periods requirement
    EXPECT_LE(rolling_vol.size(), returns_.size());

    // All volatility values should be positive
    for (size_t i = 0; i < rolling_vol.size(); ++i) {
        EXPECT_GT(rolling_vol[i], 0.0);
    }
}

TEST_F(RollingMetricsTest, TestEmptyInput) {
    TimeSeries<Return> empty_returns;
    size_t window = 10;

    auto rolling_vol = calculate_rolling_volatility(empty_returns, window);
    EXPECT_EQ(rolling_vol.size(), 0);

    auto rolling_sharpe = calculate_rolling_sharpe(empty_returns, window);
    EXPECT_EQ(rolling_sharpe.size(), 0);

    auto rolling_dd = calculate_rolling_max_drawdown(empty_returns, window);
    EXPECT_EQ(rolling_dd.size(), 0);
}

TEST_F(RollingMetricsTest, TestInsufficientData) {
    // Create very short time series
    TimeSeries<Return> short_returns;
    auto start_date = DateTime(2020, 1, 1);

    for (int i = 0; i < 3; ++i) {
        short_returns.push_back(DateTime(2020, 1, 1 + i), 0.01);
    }

    size_t window      = 10;
    size_t min_periods = 5;

    auto rolling_vol = calculate_rolling_volatility(short_returns, window, min_periods);
    EXPECT_EQ(rolling_vol.size(), 0);  // Not enough data

    // But should work with smaller min_periods
    auto rolling_vol2 = calculate_rolling_volatility(short_returns, window, 2);
    EXPECT_GT(rolling_vol2.size(), 0);
}

TEST_F(RollingMetricsTest, TestAnnualizationFactor) {
    size_t window = 15;

    // Test different annualization factors
    auto daily_vol   = calculate_rolling_volatility(returns_, window, 1, 252.0);
    auto weekly_vol  = calculate_rolling_volatility(returns_, window, 1, 52.0);
    auto monthly_vol = calculate_rolling_volatility(returns_, window, 1, 12.0);

    EXPECT_GT(daily_vol.size(), 0);
    EXPECT_GT(weekly_vol.size(), 0);
    EXPECT_GT(monthly_vol.size(), 0);

    // Should have same number of points
    EXPECT_EQ(daily_vol.size(), weekly_vol.size());
    EXPECT_EQ(daily_vol.size(), monthly_vol.size());

    // Daily should have highest values (most annualization)
    if (daily_vol.size() > 0) {
        double avg_daily = 0.0, avg_weekly = 0.0, avg_monthly = 0.0;

        for (size_t i = 0; i < daily_vol.size(); ++i) {
            avg_daily += daily_vol[i];
            avg_weekly += weekly_vol[i];
            avg_monthly += monthly_vol[i];
        }

        avg_daily /= daily_vol.size();
        avg_weekly /= weekly_vol.size();
        avg_monthly /= monthly_vol.size();

        EXPECT_GT(avg_daily, avg_weekly);
        EXPECT_GT(avg_weekly, avg_monthly);
    }
}

TEST_F(RollingMetricsTest, TestConsistentTimestamps) {
    size_t window = 10;

    auto rolling_vol    = calculate_rolling_volatility(returns_, window);
    auto rolling_sharpe = calculate_rolling_sharpe(returns_, window);

    EXPECT_GT(rolling_vol.size(), 0);
    EXPECT_GT(rolling_sharpe.size(), 0);

    // Timestamps should be consistent with input data
    const auto& original_timestamps = returns_.timestamps();
    const auto& vol_timestamps      = rolling_vol.timestamps();

    for (size_t i = 0; i < vol_timestamps.size(); ++i) {
        // Each timestamp should exist in original data
        bool found = false;
        for (size_t j = 0; j < original_timestamps.size(); ++j) {
            if (original_timestamps[j] == vol_timestamps[i]) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found);
    }
}

// Main function removed - using shared Google Test main from CMake
