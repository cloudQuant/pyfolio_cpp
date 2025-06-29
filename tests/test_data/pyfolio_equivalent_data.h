#pragma once

#include <map>
#include <pyfolio/core/datetime.h>
#include <pyfolio/core/time_series.h>
#include <pyfolio/core/types.h>
#include <string>
#include <vector>

namespace pyfolio::test_data {

// Simple position representation for testing
struct TestPosition {
    DateTime date;
    std::map<std::string, double> positions;  // symbol -> value

    TestPosition(const DateTime& d, const std::map<std::string, double>& pos) : date(d), positions(pos) {}
};

using TestPositionSeries = std::vector<TestPosition>;

// Simple transaction representation for testing
struct TestTransaction {
    DateTime date;
    std::string symbol;
    double shares;
    double price;
    double amount;
    double commission;

    TestTransaction(const DateTime& d, const std::string& sym, double sh, double pr, double amt, double comm = 0.0)
        : date(d), symbol(sym), shares(sh), price(pr), amount(amt), commission(comm) {}
};

using TestTransactionSeries = std::vector<TestTransaction>;

/**
 * Test data extracted directly from Python pyfolio test cases
 * Maintains exact same input data and expected results for validation
 */
class PyfolioEquivalentTestData {
  public:
    // Test data from test_capacity.py
    struct CapacityTestData {
        TestPositionSeries positions;
        TestTransactionSeries transactions;
        std::map<std::string, TimeSeries<double>> volume_data;
        std::map<std::string, TimeSeries<double>> price_data;

        // Expected results from Python tests
        std::vector<std::vector<double>> expected_days_to_liquidate = {
            {       0.0, 0.5 / 3.0},
            {0.75 / 2.0,       0.0}
        };

        std::vector<double> expected_slippage_returns = {0.9995, 0.9999375, 0.99998611};

        // Test parameters from Python
        double max_bar_consumption         = 0.2;
        double capital_base                = 1e6;
        int mean_volume_window             = 5;
        std::vector<double> impact_factors = {0.001, 0.0001, 0.00005};
    };

    // Test data from test_perf_attrib.py
    struct AttributionTestData {
        TimeSeries<Return> returns;
        TestPositionSeries positions;
        std::map<std::string, TimeSeries<double>> factor_returns;
        std::map<std::string, std::map<DateTime, double>> factor_loadings;
        TimeSeries<double> residuals;
        TimeSeries<double> intercepts;

        // Expected results from Python tests
        std::map<std::string, std::vector<double>> expected_factor_attribution = {
            {"risk_factor1", {0.025, 0.025}},
            {"risk_factor2", {0.025, 0.025}}
        };

        double expected_total_return    = 0.05;
        double expected_specific_return = 0.0;
    };

    // Test data from test_pos.py
    struct PositionTestData {
        TestPositionSeries positions;
        std::map<std::string, std::string> sector_map;
        TestTransactionSeries transactions;
        TimeSeries<Return> returns;

        // Expected results from Python tests - exact DataFrame structure
        struct ExpectedSectorExposures {
            DateTime date                           = DateTime::parse("2004-01-02").value();
            std::map<std::string, double> exposures = {
                {   "A",  4.0},
                {   "B",  2.0},
                {"cash", 10.0}
            };
        } expected_sector_exposures;

        struct ExpectedAllocation {
            std::vector<std::vector<double>> allocations = {
                {0.2, 0.2, 0.6}, // First day allocation percentages
                {0.0, 0.5, 0.5}, // Second day
                {0.6, 0.0, 0.4}  // Third day
            };
        } expected_allocation;
    };

    // Test data from test_round_trips.py
    struct RoundTripTestData {
        TestTransactionSeries transactions;

        // Expected results from Python tests
        struct ExpectedRoundTrip {
            double pnl         = 10.0;  // (15 - 10) * 2 = 10
            double returns     = 0.5;   // 10 / (10 * 2) = 0.5 (50%)
            int duration_days  = 1;
            std::string symbol = "A";
            double shares      = 2.0;
            double buy_price   = 10.0;
            double sell_price  = 15.0;
        } expected_round_trip;

        // Multiple round trips test case
        std::vector<ExpectedRoundTrip> expected_multiple_round_trips;
    };

    // Test data from test_timeseries.py
    struct TimeSeriesTestData {
        // Price series for drawdown testing
        std::vector<double> simple_price_series  = {100, 90, 75};
        std::vector<double> complex_price_series = {100, 120, 100, 80, 70, 110, 180, 150};

        std::vector<DateTime> dates;
        TimeSeries<Return> returns;
        TimeSeries<Return> benchmark_returns;

        // Expected results from Python tests
        double expected_max_drawdown = 0.25;  // 25% drawdown

        struct ExpectedDrawdownInfo {
            DateTime peak_date     = DateTime::parse("2000-01-08").value();
            DateTime valley_date   = DateTime::parse("2000-01-09").value();
            DateTime recovery_date = DateTime::parse("2000-01-13").value();
            double drawdown        = -0.25;
            int duration           = 5;
        } expected_drawdown_info;

        // Rolling Sharpe ratio expected results
        std::vector<double> expected_rolling_sharpe = {
            std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity(), 11.224972160321, std::numeric_limits<double>::quiet_NaN()};

        // Bootstrap expected results
        int bootstrap_samples                    = 1000;
        double expected_bootstrap_mean_tolerance = 0.1;
    };

    // Test data from test_txn.py
    struct TurnoverTestData {
        TestPositionSeries positions;
        TestTransactionSeries transactions;

        // Test cases
        struct TurnoverCase {
            TestPositionSeries positions;
            TestTransactionSeries transactions;
            std::vector<double> expected_turnover;
        };

        TurnoverCase no_txn_case;
        TurnoverCase with_txn_case;

        // Expected results from Python tests
        std::vector<double> expected_turnover;  // Will be calculated based on input
        double expected_first_turnover      = 1.0;
        double expected_subsequent_turnover = 0.8;

        // Slippage test data
        double slippage_bps                      = 10.0;  // 10 basis points
        double original_return                   = 0.05;
        double expected_slippage_adjusted_return = 0.049;
    };

    // Factory methods to create test data
    static CapacityTestData create_capacity_test_data();
    static AttributionTestData create_attribution_test_data();
    static PositionTestData create_position_test_data();
    static RoundTripTestData create_round_trip_test_data();
    static TimeSeriesTestData create_timeseries_test_data();
    static TurnoverTestData create_turnover_test_data();

    // Helper methods to load compressed test data (equivalent to Python .csv.gz files)
    static Result<TimeSeries<Return>> load_test_returns();
    static Result<TestPositionSeries> load_test_positions();
    static Result<TestTransactionSeries> load_test_transactions();

  private:
    // Private helper methods for data construction
    static std::vector<DateTime> create_test_date_range(const std::string& start_date, int num_days,
                                                        bool business_days_only = true);

    static TestPositionSeries create_test_positions_from_matrix(const std::vector<DateTime>& dates,
                                                                const std::vector<std::vector<double>>& position_matrix,
                                                                const std::vector<std::string>& symbols = {"A", "B",
                                                                                                           "cash"});

    static TestTransactionSeries create_test_transactions_from_data(
        const std::vector<std::tuple<DateTime, std::string, double, double>>& txn_data);
};

/**
 * Precision comparison utilities for test validation
 */
namespace test_precision {

// Comparison tolerances matching Python test precision
constexpr double FLOAT_TOLERANCE      = 1e-8;
constexpr double PERCENTAGE_TOLERANCE = 1e-6;
constexpr double FINANCIAL_TOLERANCE  = 1e-10;

/**
 * Compare floating point values with appropriate tolerance
 */
bool are_close(double a, double b, double tolerance = FLOAT_TOLERANCE);

/**
 * Compare vectors with element-wise tolerance
 */
bool are_vectors_close(const std::vector<double>& a, const std::vector<double>& b, double tolerance = FLOAT_TOLERANCE);

/**
 * Compare with special handling for NaN and infinity
 */
bool are_close_with_special_values(double a, double b, double tolerance = FLOAT_TOLERANCE);

/**
 * Validate time series results against expected values
 */
bool validate_time_series_result(const TimeSeries<double>& actual, const std::vector<double>& expected,
                                 double tolerance = FLOAT_TOLERANCE);

}  // namespace test_precision

}  // namespace pyfolio::test_data