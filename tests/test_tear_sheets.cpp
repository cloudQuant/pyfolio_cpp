#include <gtest/gtest.h>
#include <pyfolio/core/time_series.h>
#include <pyfolio/core/types.h>
#include <pyfolio/io/data_loader.h>
#include <pyfolio/reporting/interesting_periods.h>
#include <pyfolio/reporting/tears.h>

using namespace pyfolio;
using namespace pyfolio::reporting;
using namespace pyfolio::io;

class TearSheetTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Generate sample data for testing
        auto start_date    = DateTime(2020, 1, 1);
        returns_           = sample_data::generate_random_returns(start_date, 252, 0.10, 0.20);
        benchmark_returns_ = sample_data::generate_random_returns(start_date, 252, 0.08, 0.15, 123);

        std::vector<std::string> symbols = {"AAPL", "GOOGL", "MSFT", "TSLA", "AMZN"};
        positions_                       = sample_data::generate_sample_positions(start_date, 252, symbols);
        transactions_ = sample_data::generate_sample_transactions(start_date, start_date.add_days(252), symbols, 50);
    }

    TimeSeries<Return> returns_;
    TimeSeries<Return> benchmark_returns_;
    TimeSeries<std::unordered_map<std::string, Position>> positions_;
    std::vector<Transaction> transactions_;
};

TEST_F(TearSheetTest, TestTearSheetConfig) {
    TearSheetConfig config;
    EXPECT_TRUE(config.show_plots);
    EXPECT_FALSE(config.save_plots);
    EXPECT_EQ(config.periods_per_year, 252);
    EXPECT_EQ(config.var_confidence_level, 0.95);
    EXPECT_FALSE(config.include_bayesian);
}

TEST_F(TearSheetTest, TestSimpleTearSheet) {
    TearSheetConfig config;
    config.show_plots = false;  // Don't display plots in tests
    config.verbose    = false;

    auto result = create_simple_tear_sheet(returns_, benchmark_returns_, config);

    ASSERT_TRUE(result.has_value());

    const auto& tear_sheet = result.value();

    // Check that performance metrics are calculated
    EXPECT_GT(tear_sheet.performance.total_return, -1.0);  // Should be > -100%
    EXPECT_NE(tear_sheet.performance.annual_return, 0.0);
    EXPECT_GT(tear_sheet.performance.annual_volatility, 0.0);
    EXPECT_NE(tear_sheet.performance.sharpe_ratio, 0.0);
    EXPECT_GT(tear_sheet.performance.max_drawdown, 0.0);

    // Check timing
    EXPECT_GT(tear_sheet.generation_time_seconds, 0.0);
}

TEST_F(TearSheetTest, TestReturnsTearSheet) {
    TearSheetConfig config;
    config.show_plots = false;
    config.verbose    = false;

    auto result = create_returns_tear_sheet(returns_, benchmark_returns_, config);

    ASSERT_TRUE(result.has_value());

    const auto& tear_sheet = result.value();

    // Check that basic statistics are present
    EXPECT_NE(tear_sheet.performance.skewness, 0.0);
    EXPECT_NE(tear_sheet.performance.kurtosis, 0.0);
    EXPECT_GT(tear_sheet.performance.value_at_risk, 0.0);
}

TEST_F(TearSheetTest, TestPositionTearSheet) {
    TearSheetConfig config;
    config.show_plots = false;
    config.verbose    = false;

    auto result = create_position_tear_sheet(returns_, positions_, config);

    ASSERT_TRUE(result.has_value());

    const auto& tear_sheet = result.value();

    // Should have generated successfully
    EXPECT_GT(tear_sheet.generation_time_seconds, 0.0);
    EXPECT_EQ(tear_sheet.warnings.size(), 0);  // No warnings for valid data
}

TEST_F(TearSheetTest, TestTransactionTearSheet) {
    TearSheetConfig config;
    config.show_plots = false;
    config.verbose    = false;

    auto result = create_txn_tear_sheet(returns_, positions_, transactions_, config);

    ASSERT_TRUE(result.has_value());

    const auto& tear_sheet = result.value();

    // Should complete without errors
    EXPECT_GT(tear_sheet.generation_time_seconds, 0.0);
}

TEST_F(TearSheetTest, TestRoundTripTearSheet) {
    TearSheetConfig config;
    config.show_plots = false;
    config.verbose    = false;

    auto result = create_round_trip_tear_sheet(returns_, positions_, transactions_, config);

    ASSERT_TRUE(result.has_value());

    const auto& tear_sheet = result.value();

    // Should complete without errors
    EXPECT_GT(tear_sheet.generation_time_seconds, 0.0);
}

TEST_F(TearSheetTest, TestInterestingTimesTearSheet) {
    TearSheetConfig config;
    config.show_plots = false;
    config.verbose    = false;

    auto result = create_interesting_times_tear_sheet(returns_, benchmark_returns_, config);

    ASSERT_TRUE(result.has_value());

    const auto& tear_sheet = result.value();

    // Should complete without errors
    EXPECT_GT(tear_sheet.generation_time_seconds, 0.0);
}

TEST_F(TearSheetTest, TestFullTearSheet) {
    TearSheetConfig config;
    config.show_plots       = false;
    config.verbose          = false;
    config.include_bayesian = false;  // Skip Bayesian for now

    auto result = create_full_tear_sheet(returns_, positions_, transactions_, benchmark_returns_, config);

    ASSERT_TRUE(result.has_value());

    const auto& tear_sheet = result.value();

    // Full tear sheet should have comprehensive metrics
    EXPECT_GT(tear_sheet.performance.total_return, -1.0);
    EXPECT_GT(tear_sheet.performance.annual_volatility, 0.0);
    EXPECT_NE(tear_sheet.performance.sharpe_ratio, 0.0);
    EXPECT_GT(tear_sheet.performance.max_drawdown, 0.0);
    EXPECT_NE(tear_sheet.performance.skewness, 0.0);
    EXPECT_NE(tear_sheet.performance.kurtosis, 0.0);
    EXPECT_GT(tear_sheet.performance.value_at_risk, 0.0);

    // Should have reasonable generation time
    EXPECT_GT(tear_sheet.generation_time_seconds, 0.0);
    EXPECT_LT(tear_sheet.generation_time_seconds, 60.0);  // Should complete in under 1 minute
}

TEST_F(TearSheetTest, TestCreateAllTearSheets) {
    TearSheetConfig config;
    config.show_plots       = false;
    config.verbose          = false;
    config.include_bayesian = false;

    auto results = create_all_tear_sheets(returns_, positions_, transactions_, benchmark_returns_,
                                          std::nullopt,  // factor_returns
                                          std::nullopt,  // market_data
                                          config);

    // Should have created multiple tear sheets
    EXPECT_GT(results.size(), 3);

    // All should be successful
    for (const auto& result : results) {
        EXPECT_TRUE(result.has_value());
        if (result.has_value()) {
            EXPECT_GT(result.value().generation_time_seconds, 0.0);
        }
    }
}

TEST_F(TearSheetTest, TestInvalidInputs) {
    TearSheetConfig config;
    config.show_plots = false;
    config.verbose    = false;

    // Test with empty returns
    TimeSeries<Return> empty_returns;
    auto result = create_simple_tear_sheet(empty_returns, std::nullopt, config);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidInput);
}

TEST_F(TearSheetTest, TestConfigValidation) {
    TearSheetConfig config;
    config.periods_per_year = 0;  // Invalid
    config.show_plots       = false;
    config.verbose          = false;

    auto result = create_simple_tear_sheet(returns_, std::nullopt, config);

    // Should either fail or use default value
    // Implementation should handle this gracefully
    if (result.has_value()) {
        // If it succeeds, metrics should still be reasonable
        EXPECT_GT(result.value().performance.annual_volatility, 0.0);
    }
}

class InterestingPeriodsTest : public ::testing::Test {};

TEST_F(InterestingPeriodsTest, TestGetAllPeriods) {
    auto periods = InterestingPeriods::get_all_periods();

    EXPECT_GT(periods.size(), 10);  // Should have many periods

    // Check some specific periods exist
    bool found_gfc   = false;
    bool found_covid = false;

    for (const auto& period : periods) {
        if (period.name.find("Financial Crisis") != std::string::npos) {
            found_gfc = true;
        }
        if (period.name.find("COVID") != std::string::npos) {
            found_covid = true;
        }

        // All periods should have valid dates
        EXPECT_LT(period.start, period.end);
        EXPECT_FALSE(period.name.empty());
    }

    EXPECT_TRUE(found_gfc);
    EXPECT_TRUE(found_covid);
}

TEST_F(InterestingPeriodsTest, TestGetPeriodsByCategory) {
    auto categorized = InterestingPeriods::get_periods_by_category();

    EXPECT_GT(categorized.size(), 3);  // Should have multiple categories

    // Check that categories exist
    EXPECT_TRUE(categorized.find("Crises") != categorized.end());
    EXPECT_TRUE(categorized.find("Volatility Events") != categorized.end());

    // Each category should have periods
    for (const auto& [category, periods] : categorized) {
        EXPECT_GT(periods.size(), 0);
        EXPECT_FALSE(category.empty());
    }
}

TEST_F(InterestingPeriodsTest, TestGetRecentPeriods) {
    auto recent = InterestingPeriods::get_recent_periods(5);

    // Should have some recent periods
    EXPECT_GT(recent.size(), 0);

    auto cutoff = DateTime::now().add_years(-5);
    for (const auto& period : recent) {
        EXPECT_GE(period.start, cutoff);
    }
}

TEST_F(InterestingPeriodsTest, TestGetOverlappingPeriods) {
    auto start = DateTime(2020, 1, 1);
    auto end   = DateTime(2020, 12, 31);

    auto overlapping = InterestingPeriods::get_overlapping_periods(start, end);

    // Should find COVID-related periods
    bool found_covid = false;
    for (const auto& period : overlapping) {
        if (period.name.find("COVID") != std::string::npos) {
            found_covid = true;
        }

        // All returned periods should actually overlap
        EXPECT_TRUE(period.start <= end && period.end >= start);
    }

    EXPECT_TRUE(found_covid);
}

TEST_F(InterestingPeriodsTest, TestCustomPeriods) {
    // Clear any existing custom periods
    InterestingPeriods::clear_custom_periods();

    // Add a custom period
    InterestingPeriod custom("Test Period", DateTime(2023, 1, 1), DateTime(2023, 12, 31), "Test description");

    InterestingPeriods::add_custom_period(custom);

    auto all_periods = InterestingPeriods::get_all_including_custom();

    // Should include the custom period
    bool found_custom = false;
    for (const auto& period : all_periods) {
        if (period.name == "Test Period") {
            found_custom = true;
            EXPECT_EQ(period.description, "Test description");
        }
    }

    EXPECT_TRUE(found_custom);

    // Clean up
    InterestingPeriods::clear_custom_periods();
}

// Main function removed - using shared Google Test main from CMake
