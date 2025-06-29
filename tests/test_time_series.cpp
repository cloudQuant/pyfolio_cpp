#include <gtest/gtest.h>
#include <pyfolio/core/datetime.h>
#include <pyfolio/core/time_series.h>

using namespace pyfolio;

class TimeSeriesTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Create sample data
        dates = {DateTime::parse("2024-01-01").value(), DateTime::parse("2024-01-02").value(),
                 DateTime::parse("2024-01-03").value(), DateTime::parse("2024-01-04").value(),
                 DateTime::parse("2024-01-05").value()};

        values = {1.0, 1.01, 0.99, 1.02, 1.03};
    }

    std::vector<DateTime> dates;
    std::vector<double> values;
};

TEST_F(TimeSeriesTest, BasicConstruction) {
    TimeSeries<double> ts(dates, values);

    EXPECT_EQ(ts.size(), 5);
    EXPECT_FALSE(ts.empty());
    EXPECT_DOUBLE_EQ(ts[0], 1.0);
    EXPECT_DOUBLE_EQ(ts[4], 1.03);
}

TEST_F(TimeSeriesTest, DateIndexing) {
    TimeSeries<double> ts(dates, values);

    auto value_at_date = ts.at(dates[2]);
    ASSERT_TRUE(value_at_date.is_ok());
    EXPECT_DOUBLE_EQ(value_at_date.value(), 0.99);
}

TEST_F(TimeSeriesTest, SlicingOperations) {
    TimeSeries<double> ts(dates, values);

    auto slice = ts.slice(dates[1], dates[3]);
    ASSERT_TRUE(slice.is_ok());
    EXPECT_EQ(slice.value().size(), 3);
    EXPECT_DOUBLE_EQ(slice.value()[0], 1.01);
    EXPECT_DOUBLE_EQ(slice.value()[2], 1.02);
}

TEST_F(TimeSeriesTest, ResamplingDaily) {
    TimeSeries<double> ts(dates, values);

    auto resampled = ts.resample(ResampleFrequency::Daily);
    ASSERT_TRUE(resampled.is_ok());
    EXPECT_EQ(resampled.value().size(), ts.size());
}

TEST_F(TimeSeriesTest, RollingWindowOperations) {
    TimeSeries<double> ts(dates, values);

    auto rolling_mean = ts.rolling_mean(3);
    ASSERT_TRUE(rolling_mean.is_ok());

    // First 2 values should be NaN, then we get means
    auto result = rolling_mean.value();
    EXPECT_EQ(result.size(), 5);
    // Third value should be mean of first 3: (1.0 + 1.01 + 0.99) / 3 = 1.0
    EXPECT_NEAR(result[2], 1.0, 1e-10);
}

TEST_F(TimeSeriesTest, StatisticalOperations) {
    TimeSeries<double> ts(dates, values);

    auto mean_result = ts.mean();
    ASSERT_TRUE(mean_result.is_ok());
    double expected_mean = (1.0 + 1.01 + 0.99 + 1.02 + 1.03) / 5.0;
    EXPECT_NEAR(mean_result.value(), expected_mean, 1e-10);

    auto std_result = ts.std();
    ASSERT_TRUE(std_result.is_ok());
    EXPECT_GT(std_result.value(), 0.0);
}

TEST_F(TimeSeriesTest, ReturnCalculations) {
    TimeSeries<double> ts(dates, values);

    auto returns = ts.returns();
    ASSERT_TRUE(returns.is_ok());

    auto result = returns.value();
    EXPECT_EQ(result.size(), 4);  // One less than original

    // Check first return: (1.01 - 1.0) / 1.0 = 0.01
    EXPECT_NEAR(result[0], 0.01, 1e-10);

    // Check second return: (0.99 - 1.01) / 1.01 ≈ -0.0198
    EXPECT_NEAR(result[1], -0.0198019802, 1e-10);
}

TEST_F(TimeSeriesTest, CumulativeReturns) {
    std::vector<double> return_values  = {0.01, -0.02, 0.03, 0.01};
    std::vector<DateTime> return_dates = {dates[1], dates[2], dates[3], dates[4]};

    TimeSeries<double> returns_ts(return_dates, return_values);

    auto cum_returns = returns_ts.cumulative_returns();
    ASSERT_TRUE(cum_returns.is_ok());

    auto result = cum_returns.value();
    EXPECT_EQ(result.size(), 4);

    // First cumulative return should be 0.01
    EXPECT_NEAR(result[0], 0.01, 1e-10);

    // Fourth should be (1.01 * 0.98 * 1.03 * 1.01) - 1
    double expected = 1.01 * 0.98 * 1.03 * 1.01 - 1.0;
    EXPECT_NEAR(result[3], expected, 1e-10);
}

TEST_F(TimeSeriesTest, EmptyTimeSeries) {
    TimeSeries<double> empty_ts;

    EXPECT_TRUE(empty_ts.empty());
    EXPECT_EQ(empty_ts.size(), 0);

    auto mean_result = empty_ts.mean();
    EXPECT_TRUE(mean_result.is_error());
}

TEST_F(TimeSeriesTest, MismatchedSizes) {
    std::vector<DateTime> short_dates = {dates[0], dates[1]};

    // Should handle mismatched sizes gracefully
    TimeSeries<double> ts;
    auto result = ts.initialize(short_dates, values);
    EXPECT_TRUE(result.is_error());
}

TEST_F(TimeSeriesTest, Alignment) {
    TimeSeries<double> ts1(dates, values);

    std::vector<DateTime> dates2 = {dates[1], dates[2], dates[3]};
    std::vector<double> values2  = {2.0, 2.1, 2.2};
    TimeSeries<double> ts2(dates2, values2);

    auto aligned = ts1.align(ts2);
    ASSERT_TRUE(aligned.is_ok());

    auto [aligned1, aligned2] = aligned.value();
    EXPECT_EQ(aligned1.size(), aligned2.size());
    EXPECT_EQ(aligned1.size(), 3);  // Common dates
}

TEST_F(TimeSeriesTest, FillMissingValues) {
    // Create time series with missing data points
    std::vector<DateTime> sparse_dates = {dates[0], dates[2], dates[4]};
    std::vector<double> sparse_values  = {1.0, 0.99, 1.03};

    TimeSeries<double> sparse_ts(sparse_dates, sparse_values);

    auto filled = sparse_ts.fill_missing(dates, FillMethod::Forward);
    ASSERT_TRUE(filled.is_ok());

    auto result = filled.value();
    EXPECT_EQ(result.size(), 5);

    // Check forward fill worked
    EXPECT_DOUBLE_EQ(result[0], 1.0);   // Original
    EXPECT_DOUBLE_EQ(result[1], 1.0);   // Forward filled
    EXPECT_DOUBLE_EQ(result[2], 0.99);  // Original
}