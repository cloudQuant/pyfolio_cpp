#include <gtest/gtest.h>
#include <pyfolio/analytics/performance_metrics.h>
#include <pyfolio/core/time_series.h>

using namespace pyfolio;

class PerformanceMetricsTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Create realistic return data
        dates = {DateTime::parse("2024-01-01").value(), DateTime::parse("2024-01-02").value(),
                 DateTime::parse("2024-01-03").value(), DateTime::parse("2024-01-04").value(),
                 DateTime::parse("2024-01-05").value(), DateTime::parse("2024-01-08").value(),
                 DateTime::parse("2024-01-09").value(), DateTime::parse("2024-01-10").value(),
                 DateTime::parse("2024-01-11").value(), DateTime::parse("2024-01-12").value()};

        // Sample returns with some volatility
        returns = {0.015, -0.022, 0.018, -0.008, 0.025, -0.015, 0.012, 0.008, -0.018, 0.022};

        returns_ts = TimeSeries<Return>(dates, returns);

        // Benchmark returns
        benchmark_returns = {0.010, -0.018, 0.015, -0.005, 0.020, -0.012, 0.008, 0.006, -0.015, 0.018};
        benchmark_ts      = TimeSeries<Return>(dates, benchmark_returns);

        risk_free_rate = 0.02;  // 2% annual

        // Create positions data
        position_dates = dates;
        // Sample positions (cash + some positions)
        positions_data = {
            {{"AAPL", 10000.0}, {"MSFT", 8000.0}, {"cash", 2000.0}},
            {{"AAPL", 10200.0}, {"MSFT", 7800.0}, {"cash", 2000.0}},
            {{"AAPL", 10150.0}, {"MSFT", 7900.0}, {"cash", 1950.0}},
            {{"AAPL", 10080.0}, {"MSFT", 7950.0}, {"cash", 1970.0}},
            {{"AAPL", 10300.0}, {"MSFT", 8100.0}, {"cash", 1600.0}},
            {{"AAPL", 10150.0}, {"MSFT", 8000.0}, {"cash", 1850.0}},
            {{"AAPL", 10280.0}, {"MSFT", 8050.0}, {"cash", 1670.0}},
            {{"AAPL", 10320.0}, {"MSFT", 8080.0}, {"cash", 1600.0}},
            {{"AAPL", 10100.0}, {"MSFT", 7950.0}, {"cash", 1950.0}},
            {{"AAPL", 10400.0}, {"MSFT", 8200.0}, {"cash", 1400.0}}
        };
    }

    std::vector<DateTime> dates;
    std::vector<Return> returns;
    std::vector<Return> benchmark_returns;
    TimeSeries<Return> returns_ts;
    TimeSeries<Return> benchmark_ts;
    double risk_free_rate;

    std::vector<DateTime> position_dates;
    std::vector<std::map<std::string, double>> positions_data;
};

TEST_F(PerformanceMetricsTest, AnnualReturn) {
    auto annual_ret = PerformanceMetrics::annual_return(returns_ts);
    ASSERT_TRUE(annual_ret.is_ok());

    auto result = annual_ret.value();
    // Should be reasonable annualized return
    EXPECT_GE(result, -1.0);  // Not less than -100%
    EXPECT_LE(result, 5.0);   // Not more than 500% (reasonable upper bound)
}

TEST_F(PerformanceMetricsTest, AnnualVolatility) {
    auto annual_vol = PerformanceMetrics::annual_volatility(returns_ts);
    ASSERT_TRUE(annual_vol.is_ok());

    auto result = annual_vol.value();
    EXPECT_GT(result, 0.0);
    EXPECT_LT(result, 2.0);  // Less than 200% volatility
}

TEST_F(PerformanceMetricsTest, SharpeRatioCalculation) {
    auto sharpe = PerformanceMetrics::sharpe_ratio(returns_ts, risk_free_rate);
    ASSERT_TRUE(sharpe.is_ok());

    auto result = sharpe.value();
    // Sharpe ratio should be finite
    EXPECT_TRUE(std::isfinite(result));
    EXPECT_GE(result, -5.0);
    EXPECT_LE(result, 5.0);
}

TEST_F(PerformanceMetricsTest, SortinoRatioCalculation) {
    auto sortino = PerformanceMetrics::sortino_ratio(returns_ts, risk_free_rate);
    ASSERT_TRUE(sortino.is_ok());

    auto result = sortino.value();
    EXPECT_TRUE(std::isfinite(result));

    // Sortino is typically higher than Sharpe for same data
    auto sharpe = PerformanceMetrics::sharpe_ratio(returns_ts, risk_free_rate);
    ASSERT_TRUE(sharpe.is_ok());

    // Allow for cases where they might be close
    EXPECT_GE(result, sharpe.value() * 0.8);
}

TEST_F(PerformanceMetricsTest, CalmarRatioCalculation) {
    auto calmar = PerformanceMetrics::calmar_ratio(returns_ts);
    ASSERT_TRUE(calmar.is_ok());

    auto result = calmar.value();
    EXPECT_TRUE(std::isfinite(result));
}

TEST_F(PerformanceMetricsTest, MaxDrawdownAnalysis) {
    auto max_dd = PerformanceMetrics::max_drawdown(returns_ts);
    ASSERT_TRUE(max_dd.is_ok());

    auto result = max_dd.value();
    EXPECT_LE(result.max_drawdown, 0.0);   // Should be negative
    EXPECT_GE(result.max_drawdown, -1.0);  // Not worse than -100%
    EXPECT_GT(result.duration_days, 0);    // Should have some duration
    EXPECT_GE(result.peak_date, dates.front());
    EXPECT_LE(result.valley_date, dates.back());
}

TEST_F(PerformanceMetricsTest, AlphaBetaAnalysis) {
    auto alpha_beta = PerformanceMetrics::alpha_beta(returns_ts, benchmark_ts, risk_free_rate);
    ASSERT_TRUE(alpha_beta.is_ok());

    auto result = alpha_beta.value();
    EXPECT_TRUE(std::isfinite(result.alpha));
    EXPECT_TRUE(std::isfinite(result.beta));
    EXPECT_GT(result.r_squared, 0.0);
    EXPECT_LE(result.r_squared, 1.0);

    // Beta should be in reasonable range
    EXPECT_GE(result.beta, -3.0);
    EXPECT_LE(result.beta, 3.0);
}

TEST_F(PerformanceMetricsTest, InformationRatioCalculation) {
    auto info_ratio = PerformanceMetrics::information_ratio(returns_ts, benchmark_ts);
    ASSERT_TRUE(info_ratio.is_ok());

    auto result = info_ratio.value();
    EXPECT_TRUE(std::isfinite(result));
}

TEST_F(PerformanceMetricsTest, TrackingErrorCalculation) {
    auto tracking = PerformanceMetrics::tracking_error(returns_ts, benchmark_ts);
    ASSERT_TRUE(tracking.is_ok());

    auto result = tracking.value();
    EXPECT_GT(result, 0.0);
    EXPECT_LT(result, 1.0);  // Less than 100% tracking error
}

TEST_F(PerformanceMetricsTest, UpDownCaptureRatio) {
    auto capture = PerformanceMetrics::up_down_capture_ratio(returns_ts, benchmark_ts);
    ASSERT_TRUE(capture.is_ok());

    auto result = capture.value();
    EXPECT_GT(result.up_capture, 0.0);
    EXPECT_GT(result.down_capture, 0.0);

    // Ratios should be reasonable
    EXPECT_LT(result.up_capture, 5.0);
    EXPECT_LT(result.down_capture, 5.0);
}

TEST_F(PerformanceMetricsTest, TailRatio) {
    auto tail = PerformanceMetrics::tail_ratio(returns_ts, 0.05);
    ASSERT_TRUE(tail.is_ok());

    auto result = tail.value();
    EXPECT_GT(result, 0.0);
}

TEST_F(PerformanceMetricsTest, CommonSenseRatio) {
    auto csr = PerformanceMetrics::common_sense_ratio(returns_ts);
    ASSERT_TRUE(csr.is_ok());

    auto result = csr.value();
    EXPECT_TRUE(std::isfinite(result));
}

TEST_F(PerformanceMetricsTest, StabilityOfTimeseries) {
    auto stability = PerformanceMetrics::stability_of_timeseries(returns_ts);
    ASSERT_TRUE(stability.is_ok());

    auto result = stability.value();
    EXPECT_GE(result, -1.0);
    EXPECT_LE(result, 1.0);
}

TEST_F(PerformanceMetricsTest, ComprehensiveMetricsSuite) {
    auto metrics = PerformanceMetrics::calculate_comprehensive_metrics(returns_ts, benchmark_ts, risk_free_rate);
    ASSERT_TRUE(metrics.is_ok());

    auto result = metrics.value();

    // Check all metrics are calculated
    EXPECT_TRUE(std::isfinite(result.annual_return));
    EXPECT_GT(result.annual_volatility, 0.0);
    EXPECT_TRUE(std::isfinite(result.sharpe_ratio));
    EXPECT_TRUE(std::isfinite(result.sortino_ratio));
    EXPECT_LE(result.max_drawdown, 0.0);
    EXPECT_TRUE(std::isfinite(result.alpha));
    EXPECT_TRUE(std::isfinite(result.beta));
    EXPECT_GT(result.tracking_error, 0.0);
}

TEST_F(PerformanceMetricsTest, CumulativeReturnsProfile) {
    auto cum_returns = PerformanceMetrics::cumulative_returns(returns_ts);
    ASSERT_TRUE(cum_returns.is_ok());

    auto result = cum_returns.value();
    EXPECT_EQ(result.size(), returns.size());

    // First cumulative return should equal first return
    EXPECT_NEAR(result[0], returns[0], 1e-10);

    // Cumulative returns should be monotonic in expectation (allowing for volatility)
    EXPECT_TRUE(result.back() != 0.0);  // Should have some final cumulative return
}

TEST_F(PerformanceMetricsTest, DrawdownSeries) {
    auto drawdowns = PerformanceMetrics::drawdown_series(returns_ts);
    ASSERT_TRUE(drawdowns.is_ok());

    auto result = drawdowns.value();
    EXPECT_EQ(result.size(), returns.size());

    // All drawdowns should be <= 0
    for (const auto& dd : result) {
        EXPECT_LE(dd, 0.0);
    }
}

TEST_F(PerformanceMetricsTest, RollingMetrics) {
    auto rolling_sharpe = PerformanceMetrics::rolling_sharpe(returns_ts, 5, risk_free_rate);
    ASSERT_TRUE(rolling_sharpe.is_ok());

    auto result = rolling_sharpe.value();
    EXPECT_EQ(result.size(), returns.size() - 4);  // 5-day window

    // Check that all values are finite
    for (const auto& val : result) {
        EXPECT_TRUE(std::isfinite(val));
    }
}

TEST_F(PerformanceMetricsTest, ErrorHandling) {
    TimeSeries<Return> empty_ts;

    // Empty time series should return errors
    auto annual_ret = PerformanceMetrics::annual_return(empty_ts);
    EXPECT_TRUE(annual_ret.is_error());

    auto sharpe = PerformanceMetrics::sharpe_ratio(empty_ts, risk_free_rate);
    EXPECT_TRUE(sharpe.is_error());

    // Single value time series
    std::vector<DateTime> single_date = {dates[0]};
    std::vector<Return> single_return = {0.01};
    TimeSeries<Return> single_ts(single_date, single_return);

    auto single_vol = PerformanceMetrics::annual_volatility(single_ts);
    EXPECT_TRUE(single_vol.is_error());  // Can't calculate volatility from single point
}