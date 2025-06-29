#include "pyfolio_equivalent_data.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace pyfolio::test_data {

// ============================================================================
// Factory Methods for Test Data Creation
// ============================================================================

PyfolioEquivalentTestData::CapacityTestData PyfolioEquivalentTestData::create_capacity_test_data() {
    CapacityTestData data;

    // Create test date range
    auto dates = create_test_date_range("2004-01-01", 3);

    // Create test positions matching Python test_capacity.py
    std::vector<std::vector<double>> position_matrix = {
        { 0.0, 0.5, 0.0}, // Day 1: 0.5 shares of B
        {0.75, 0.0, 0.0}  // Day 2: 0.75 shares of A
    };

    data.positions = create_test_positions_from_matrix(dates, position_matrix, {"A", "B", "cash"});

    // Create volume data (daily volume for each symbol)
    std::vector<double> volume_A = {3.0, 2.0, 4.0};  // ADV for symbol A
    std::vector<double> volume_B = {3.0, 3.0, 3.0};  // ADV for symbol B

    data.volume_data["A"] = TimeSeries<double>(dates, volume_A);
    data.volume_data["B"] = TimeSeries<double>(dates, volume_B);

    // Create price data
    std::vector<double> price_A = {10.0, 11.0, 12.0};
    std::vector<double> price_B = {20.0, 21.0, 22.0};

    data.price_data["A"] = TimeSeries<double>(dates, price_A);
    data.price_data["B"] = TimeSeries<double>(dates, price_B);

    // Create test transactions for slippage analysis
    std::vector<std::tuple<DateTime, std::string, double, double>> txn_data = {
        {dates[0], "A", 100.0, 10.0}, // Buy 100 shares of A at $10
        {dates[1], "B", -50.0, 21.0}  // Sell 50 shares of B at $21
    };

    data.transactions = create_test_transactions_from_data(txn_data);

    return data;
}

PyfolioEquivalentTestData::AttributionTestData PyfolioEquivalentTestData::create_attribution_test_data() {
    AttributionTestData data;

    // Create test date range
    auto dates = create_test_date_range("2004-01-01", 2);

    // Create simple returns for attribution testing
    std::vector<Return> returns = {0.025, 0.025};  // 2.5% daily returns
    data.returns                = TimeSeries<Return>(dates, returns);

    // Create test positions
    std::vector<std::vector<double>> position_matrix = {
        {100.0, 0.0, 0.0}, // 100 shares of A
        {100.0, 0.0, 0.0}  // 100 shares of A
    };

    data.positions = create_test_positions_from_matrix(dates, position_matrix);

    // Create factor returns
    std::vector<double> factor1_returns = {0.025, 0.025};
    std::vector<double> factor2_returns = {0.025, 0.025};

    data.factor_returns["risk_factor1"] = TimeSeries<double>(dates, factor1_returns);
    data.factor_returns["risk_factor2"] = TimeSeries<double>(dates, factor2_returns);

    // Create factor loadings (beta = 1.0 for both factors)
    for (const auto& date : dates) {
        data.factor_loadings["risk_factor1"][date] = 1.0;
        data.factor_loadings["risk_factor2"][date] = 1.0;
    }

    // Zero residuals and intercepts (perfect factor model)
    std::vector<double> zeros(dates.size(), 0.0);
    data.residuals  = TimeSeries<double>(dates, zeros);
    data.intercepts = TimeSeries<double>(dates, zeros);

    return data;
}

PyfolioEquivalentTestData::PositionTestData PyfolioEquivalentTestData::create_position_test_data() {
    PositionTestData data;

    // Create test date range
    auto dates = create_test_date_range("2004-01-01", 3);

    // Create test positions matching Python test_pos.py
    // Position values, not shares (to match Python DataFrame structure)
    std::vector<std::vector<double>> position_matrix = {
        { 4.0, 2.0, 10.0}, // Day 1: $4 in A, $2 in B, $10 cash
        { 0.0, 8.0,  8.0}, // Day 2: $0 in A, $8 in B, $8 cash
        {12.0, 0.0,  8.0}  // Day 3: $12 in A, $0 in B, $8 cash
    };

    data.positions = create_test_positions_from_matrix(dates, position_matrix);

    // Create sector mapping
    data.sector_map = {
        {   "A",    "A"}, // Symbol A is in sector A
        {   "B",    "B"}, // Symbol B is in sector B
        {"cash", "cash"}  // Cash is cash
    };

    // Create test transactions
    std::vector<std::tuple<DateTime, std::string, double, double>> txn_data = {
        {dates[0], "A",  0.4, 10.0}, // Buy 0.4 shares of A at $10
        {dates[1], "A", -0.4, 11.0}, // Sell 0.4 shares of A at $11
        {dates[1], "B",  0.4, 20.0}, // Buy 0.4 shares of B at $20
        {dates[2], "B", -0.4, 21.0}, // Sell 0.4 shares of B at $21
        {dates[2], "A",  1.0, 12.0}  // Buy 1.0 shares of A at $12
    };

    data.transactions = create_test_transactions_from_data(txn_data);

    // Create simple returns from position changes
    std::vector<Return> returns;
    for (size_t i = 1; i < dates.size(); ++i) {
        // Calculate return based on position value changes
        double prev_total = 16.0;  // Total portfolio value
        double curr_total = 16.0;  // Assume constant for simplicity
        returns.push_back((curr_total - prev_total) / prev_total);
    }

    if (!returns.empty()) {
        std::vector<DateTime> return_dates(dates.begin() + 1, dates.end());
        data.returns = TimeSeries<Return>(return_dates, returns);
    }

    return data;
}

PyfolioEquivalentTestData::RoundTripTestData PyfolioEquivalentTestData::create_round_trip_test_data() {
    RoundTripTestData data;

    // Create test date range
    auto dates = create_test_date_range("2004-01-01", 3);

    // Create transactions that form a complete round trip
    // Buy 2 shares of A at $10, then sell 2 shares of A at $15
    std::vector<std::tuple<DateTime, std::string, double, double>> txn_data = {
        {dates[0], "A",  2.0, 10.0}, // Buy 2 shares at $10
        {dates[1], "A", -2.0, 15.0}  // Sell 2 shares at $15
    };

    data.transactions = create_test_transactions_from_data(txn_data);

    // Expected round trip already set in header with:
    // P&L = (15 - 10) * 2 = 10
    // Returns = 10 / (10 * 2) = 0.5 (50%)
    // Duration = 1 day

    return data;
}

PyfolioEquivalentTestData::TimeSeriesTestData PyfolioEquivalentTestData::create_timeseries_test_data() {
    TimeSeriesTestData data;

    // Create test date range
    auto dates = create_test_date_range("2000-01-01", 8);
    data.dates = dates;

    // Convert price series to returns
    auto convert_to_returns = [](const std::vector<double>& prices) -> std::vector<Return> {
        std::vector<Return> returns;
        for (size_t i = 1; i < prices.size(); ++i) {
            returns.push_back((prices[i] - prices[i - 1]) / prices[i - 1]);
        }
        return returns;
    };

    // Create returns from complex price series
    auto returns = convert_to_returns(data.complex_price_series);
    std::vector<DateTime> return_dates(dates.begin() + 1, dates.end());
    data.returns = TimeSeries<Return>(return_dates, returns);

    // Create benchmark returns (assume flat 0% for simplicity)
    std::vector<Return> benchmark_returns(returns.size(), 0.0);
    data.benchmark_returns = TimeSeries<Return>(return_dates, benchmark_returns);

    return data;
}

PyfolioEquivalentTestData::TurnoverTestData PyfolioEquivalentTestData::create_turnover_test_data() {
    TurnoverTestData data;

    // From test_txn.py: TransactionsTestCase.test_get_turnover
    // dates = date_range(start='2015-01-01', freq='D', periods=20)
    auto dates = create_test_date_range("2015-01-01", 20);

    // positions = DataFrame([[10.0, 10.0]]*len(dates), columns=[0, 'cash'], index=dates)
    // positions.loc[::2, 0] = 40  # Set every other non-cash position to 40
    std::vector<std::vector<double>> position_matrix;
    for (size_t i = 0; i < dates.size(); ++i) {
        if (i % 2 == 0) {
            position_matrix.push_back({40.0, 10.0});  // Even indices: 40 in asset, 10 cash
        } else {
            position_matrix.push_back({10.0, 10.0});  // Odd indices: 10 in asset, 10 cash
        }
    }

    data.positions = create_test_positions_from_matrix(dates, position_matrix, {"0", "cash"});

    // Test case 1: No transactions -> expected turnover = [0.0]*len(dates)
    data.no_txn_case.positions         = data.positions;
    data.no_txn_case.transactions      = TestTransactionSeries({});  // Empty transactions
    data.no_txn_case.expected_turnover = std::vector<double>(dates.size(), 0.0);

    // Test case 2: With transactions
    // transactions = DataFrame([[1, 1, 10, 0]]*len(dates) + [[2, -1, 10, 0]]*len(dates))
    std::vector<std::tuple<DateTime, std::string, double, double>> txn_data;
    for (const auto& date : dates) {
        txn_data.emplace_back(date, "0", 1.0, 10.0);   // Buy 1 share
        txn_data.emplace_back(date, "0", -1.0, 10.0);  // Sell 1 share
    }

    data.with_txn_case.positions    = data.positions;
    data.with_txn_case.transactions = create_test_transactions_from_data(txn_data);

    // Expected turnover: [1.0] + [0.8] * (len(dates) - 1)
    // First day = 1.0, subsequent days = 0.8 (because 20 transacted / mean(10,40) = 20/25 = 0.8)
    data.with_txn_case.expected_turnover.push_back(1.0);
    for (size_t i = 1; i < dates.size(); ++i) {
        data.with_txn_case.expected_turnover.push_back(0.8);
    }

    return data;
}

// ============================================================================
// Helper Methods for Data Loading and Construction
// ============================================================================

std::vector<DateTime> PyfolioEquivalentTestData::create_test_date_range(const std::string& start_date, int num_days,
                                                                        bool business_days_only) {
    std::vector<DateTime> dates;

    auto start = DateTime::parse(start_date);
    if (start.is_error()) {
        std::cerr << "Failed to parse start date: " << start_date << std::endl;
        return dates;
    }

    DateTime current = start.value();

    for (int i = 0; i < num_days; ++i) {
        dates.push_back(current);

        if (business_days_only) {
            // Skip weekends
            do {
                current = current.add_days(1);
            } while (current.day_of_week() == 6 || current.day_of_week() == 0);  // Saturday or Sunday
        } else {
            current = current.add_days(1);
        }
    }

    return dates;
}

TestPositionSeries PyfolioEquivalentTestData::create_test_positions_from_matrix(
    const std::vector<DateTime>& dates, const std::vector<std::vector<double>>& position_matrix,
    const std::vector<std::string>& symbols) {
    std::vector<TestPosition> positions;

    for (size_t i = 0; i < std::min(dates.size(), position_matrix.size()); ++i) {
        const auto& date            = dates[i];
        const auto& position_values = position_matrix[i];

        std::map<std::string, double> daily_positions;

        for (size_t j = 0; j < std::min(symbols.size(), position_values.size()); ++j) {
            daily_positions[symbols[j]] = position_values[j];
        }

        positions.emplace_back(date, daily_positions);
    }

    return TestPositionSeries(positions);
}

TestTransactionSeries PyfolioEquivalentTestData::create_test_transactions_from_data(
    const std::vector<std::tuple<DateTime, std::string, double, double>>& txn_data) {
    std::vector<TestTransaction> transactions;

    for (const auto& [date, symbol, shares, price] : txn_data) {
        transactions.emplace_back(date, symbol, shares, price,
                                  shares * price,  // amount
                                  0.0              // commission
        );
    }

    return TestTransactionSeries(transactions);
}

// ============================================================================
// Compressed Test Data Loading (equivalent to Python .csv.gz files)
// ============================================================================

Result<TimeSeries<Return>> PyfolioEquivalentTestData::load_test_returns() {
    // This would normally load from test_returns.csv.gz
    // For now, create synthetic data that matches Python test expectations

    auto dates = create_test_date_range("2004-01-03", 252);  // One year of daily data
    std::vector<Return> returns;

    // Generate synthetic returns with known statistical properties
    double daily_return = 0.08 / 252.0;             // 8% annual return
    double daily_vol    = 0.15 / std::sqrt(252.0);  // 15% annual volatility

    for (size_t i = 0; i < dates.size(); ++i) {
        // Simple deterministic pattern for reproducible tests
        double noise = std::sin(i * 0.1) * daily_vol;
        returns.push_back(daily_return + noise);
    }

    return Result<TimeSeries<Return>>::success(TimeSeries<Return>(dates, returns));
}

Result<TestPositionSeries> PyfolioEquivalentTestData::load_test_positions() {
    // This would normally load from test_pos.csv.gz
    auto dates = create_test_date_range("2004-01-02", 252);

    std::vector<TestPosition> positions;

    for (size_t i = 0; i < dates.size(); ++i) {
        std::map<std::string, double> daily_positions;

        // Create evolving positions over time
        daily_positions["AAPL"] = 1000.0 + i * 10.0;  // Growing Apple position
        daily_positions["MSFT"] = 500.0 - i * 2.0;    // Shrinking Microsoft position
        daily_positions["cash"] = 500.0;              // Constant cash

        positions.emplace_back(dates[i], daily_positions);
    }

    return Result<TestPositionSeries>::success(TestPositionSeries(positions));
}

Result<TestTransactionSeries> PyfolioEquivalentTestData::load_test_transactions() {
    // This would normally load from test_txn.csv.gz
    auto dates = create_test_date_range("2004-01-02", 100);

    std::vector<TestTransaction> transactions;

    // Create realistic transaction patterns
    for (size_t i = 0; i < dates.size(); i += 5) {  // Trade every 5 days
        if (i < dates.size()) {
            // Alternate between buying and selling
            double shares = (i % 10 == 0) ? 100.0 : -100.0;
            double price  = 50.0 + (i * 0.1);  // Trending price

            transactions.emplace_back(dates[i], "AAPL", shares, price, shares * price,
                                      1.0  // $1 commission
            );
        }
    }

    return Result<TestTransactionSeries>::success(TestTransactionSeries(transactions));
}

// ============================================================================
// Precision Comparison Utilities
// ============================================================================

namespace test_precision {

bool are_close(double a, double b, double tolerance) {
    if (std::isnan(a) && std::isnan(b))
        return true;
    if (std::isinf(a) && std::isinf(b) && std::signbit(a) == std::signbit(b))
        return true;
    return std::abs(a - b) <= tolerance;
}

bool are_vectors_close(const std::vector<double>& a, const std::vector<double>& b, double tolerance) {
    if (a.size() != b.size())
        return false;

    for (size_t i = 0; i < a.size(); ++i) {
        if (!are_close(a[i], b[i], tolerance)) {
            return false;
        }
    }
    return true;
}

bool are_close_with_special_values(double a, double b, double tolerance) {
    // Handle NaN values
    if (std::isnan(a) && std::isnan(b))
        return true;
    if (std::isnan(a) || std::isnan(b))
        return false;

    // Handle infinity values
    if (std::isinf(a) && std::isinf(b)) {
        return std::signbit(a) == std::signbit(b);  // Same sign infinity
    }
    if (std::isinf(a) || std::isinf(b))
        return false;

    // Regular floating point comparison
    return std::abs(a - b) <= tolerance;
}

bool validate_time_series_result(const TimeSeries<double>& actual, const std::vector<double>& expected,
                                 double tolerance) {
    if (actual.size() != expected.size())
        return false;

    auto actual_values = actual.values();
    return are_vectors_close(actual_values, expected, tolerance);
}

}  // namespace test_precision

}  // namespace pyfolio::test_data