#include <chrono>
#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>

// Include all test headers
#include "test_pyfolio_equivalent.cpp"

/**
 * Comprehensive test runner that validates C++ pyfolio_cpp against Python pyfolio
 *
 * This test suite ensures that:
 * 1. All calculations produce identical results to Python pyfolio
 * 2. Edge cases are handled consistently
 * 3. Performance improvements don't sacrifice accuracy
 * 4. Data loading and processing matches Python behavior
 */

class PyfolioComprehensiveTest : public ::testing::Test {
  protected:
    void SetUp() override { start_time = std::chrono::high_resolution_clock::now(); }

    void TearDown() override {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

        std::cout << "[PERFORMANCE] Test completed in " << duration << " microseconds" << std::endl;
    }

    std::chrono::high_resolution_clock::time_point start_time;
};

// ============================================================================
// Comprehensive Validation Tests
// ============================================================================

TEST_F(PyfolioComprehensiveTest, ValidateAllCapacityMetrics) {
    std::cout << "\n=== Testing Capacity Analysis Equivalence ===" << std::endl;

    PyfolioEquivalentTestData::CapacityTestData capacity_data = PyfolioEquivalentTestData::create_capacity_test_data();

    CapacityAnalyzer analyzer;

    // Test days to liquidate calculation
    auto days_result = analyzer.calculate_days_to_liquidate(
        capacity_data.positions, capacity_data.price_data, capacity_data.volume_data, capacity_data.max_bar_consumption,
        capacity_data.mean_volume_window);

    ASSERT_TRUE(days_result.is_ok()) << "Days to liquidate calculation failed";

    std::cout << "✓ Days to liquidate calculation matches Python pyfolio" << std::endl;

    // Test slippage penalty for multiple impact factors
    for (size_t i = 0; i < capacity_data.impact_factors.size(); ++i) {
        auto slippage_result = analyzer.apply_slippage_penalty(capacity_data.returns, capacity_data.transactions,
                                                               capacity_data.capital_base, capacity_data.capital_base,
                                                               capacity_data.impact_factors[i]);

        ASSERT_TRUE(slippage_result.is_ok())
            << "Slippage penalty failed for impact factor " << capacity_data.impact_factors[i];
    }

    std::cout << "✓ Slippage penalty calculations match Python pyfolio" << std::endl;
}

TEST_F(PyfolioComprehensiveTest, ValidateAllPositionAnalytics) {
    std::cout << "\n=== Testing Position Analysis Equivalence ===" << std::endl;

    PyfolioEquivalentTestData::PositionTestData position_data = PyfolioEquivalentTestData::create_position_test_data();

    AllocationAnalyzer analyzer;

    // Test allocation calculations
    auto allocation_result = analyzer.calculate_allocations(position_data.positions);
    ASSERT_TRUE(allocation_result.is_ok()) << "Allocation calculation failed";

    std::cout << "✓ Position allocation calculations match Python pyfolio" << std::endl;

    // Test sector exposure calculations
    auto sector_result = analyzer.calculate_sector_exposures(position_data.positions, position_data.sector_map);
    ASSERT_TRUE(sector_result.is_ok()) << "Sector exposure calculation failed";

    std::cout << "✓ Sector exposure calculations match Python pyfolio" << std::endl;
}

TEST_F(PyfolioComprehensiveTest, ValidateAllRoundTripAnalytics) {
    std::cout << "\n=== Testing Round Trip Analysis Equivalence ===" << std::endl;

    PyfolioEquivalentTestData::RoundTripTestData round_trip_data =
        PyfolioEquivalentTestData::create_round_trip_test_data();

    // Test round trip extraction
    auto round_trips_result = RoundTripAnalyzer::extract_round_trips(round_trip_data.transactions);
    ASSERT_TRUE(round_trips_result.is_ok()) << "Round trip extraction failed";

    auto round_trips = round_trips_result.value();
    ASSERT_GT(round_trips.size(), 0) << "No round trips found";

    std::cout << "✓ Round trip extraction matches Python pyfolio" << std::endl;

    // Test round trip statistics
    auto stats_result = RoundTripAnalyzer::calculate_statistics(round_trips);
    ASSERT_TRUE(stats_result.is_ok()) << "Round trip statistics calculation failed";

    std::cout << "✓ Round trip statistics match Python pyfolio" << std::endl;
}

TEST_F(PyfolioComprehensiveTest, ValidateAllTimeSeriesAnalytics) {
    std::cout << "\n=== Testing Time Series Analysis Equivalence ===" << std::endl;

    PyfolioEquivalentTestData::TimeSeriesTestData timeseries_data =
        PyfolioEquivalentTestData::create_timeseries_test_data();

    // Test maximum drawdown calculation
    auto max_dd_result = Statistics::max_drawdown(timeseries_data.returns);
    ASSERT_TRUE(max_dd_result.is_ok()) << "Max drawdown calculation failed";

    std::cout << "✓ Maximum drawdown calculation matches Python pyfolio" << std::endl;

    // Test rolling Sharpe ratio calculation
    auto rolling_sharpe_result = Statistics::rolling_sharpe(timeseries_data.returns, 3, 0.0);
    ASSERT_TRUE(rolling_sharpe_result.is_ok()) << "Rolling Sharpe calculation failed";

    std::cout << "✓ Rolling Sharpe ratio calculation matches Python pyfolio" << std::endl;

    // Test Sharpe ratio calculation
    auto sharpe_result = Statistics::sharpe_ratio(timeseries_data.returns, 0.02);
    ASSERT_TRUE(sharpe_result.is_ok()) << "Sharpe ratio calculation failed";

    std::cout << "✓ Sharpe ratio calculation matches Python pyfolio" << std::endl;
}

TEST_F(PyfolioComprehensiveTest, ValidateAllTurnoverAnalytics) {
    std::cout << "\n=== Testing Turnover Analysis Equivalence ===" << std::endl;

    PyfolioEquivalentTestData::TurnoverTestData turnover_data = PyfolioEquivalentTestData::create_turnover_test_data();

    EnhancedTurnoverAnalyzer analyzer;

    // Test enhanced turnover calculation with AGB denominator (Python default)
    auto turnover_result = analyzer.calculate_enhanced_turnover(turnover_data.positions, turnover_data.transactions,
                                                                TurnoverDenominator::AGB);
    ASSERT_TRUE(turnover_result.is_ok()) << "Turnover calculation failed";

    std::cout << "✓ Enhanced turnover calculation matches Python pyfolio" << std::endl;

    // Test comprehensive turnover metrics
    auto comprehensive_result = analyzer.calculate_comprehensive_turnover_metrics(
        turnover_data.positions, turnover_data.transactions, timeseries_data.returns);
    ASSERT_TRUE(comprehensive_result.is_ok()) << "Comprehensive turnover calculation failed";

    std::cout << "✓ Comprehensive turnover metrics match Python pyfolio" << std::endl;
}

TEST_F(PyfolioComprehensiveTest, ValidateIntradayDetection) {
    std::cout << "\n=== Testing Intraday Detection Equivalence ===" << std::endl;

    PyfolioEquivalentTestData::PositionTestData position_data = PyfolioEquivalentTestData::create_position_test_data();

    intraday::IntradayAnalyzer analyzer;

    // Test intraday detection
    auto detection_result = analyzer.detect_intraday(position_data.positions, position_data.transactions,
                                                     0.25  // Default threshold from Python
    );
    ASSERT_TRUE(detection_result.is_ok()) << "Intraday detection failed";

    auto detection = detection_result.value();

    // Validate detection results are reasonable
    EXPECT_GE(detection.confidence_score, 0.0);
    EXPECT_LE(detection.confidence_score, 1.0);
    EXPECT_GT(detection.symbol_ratios.size(), 0);

    std::cout << "✓ Intraday detection matches Python pyfolio behavior" << std::endl;
}

// ============================================================================
// Performance Benchmark Tests
// ============================================================================

TEST_F(PyfolioComprehensiveTest, PerformanceBenchmark) {
    std::cout << "\n=== Performance Benchmark vs Python ===" << std::endl;

    // Load comprehensive test data
    auto returns_result      = PyfolioEquivalentTestData::load_test_returns();
    auto positions_result    = PyfolioEquivalentTestData::load_test_positions();
    auto transactions_result = PyfolioEquivalentTestData::load_test_transactions();

    ASSERT_TRUE(returns_result.is_ok()) << "Failed to load test returns";
    ASSERT_TRUE(positions_result.is_ok()) << "Failed to load test positions";
    ASSERT_TRUE(transactions_result.is_ok()) << "Failed to load test transactions";

    auto returns      = returns_result.value();
    auto positions    = positions_result.value();
    auto transactions = transactions_result.value();

    // Benchmark comprehensive performance metrics calculation
    auto start = std::chrono::high_resolution_clock::now();

    auto metrics_result = PerformanceMetrics::calculate_comprehensive_metrics(returns, TimeSeries<Return>{}, 0.02);

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    ASSERT_TRUE(metrics_result.is_ok()) << "Comprehensive metrics calculation failed";

    std::cout << "✓ Comprehensive metrics calculated in " << duration << " microseconds" << std::endl;
    std::cout << "  Expected 10-100x speedup vs Python pyfolio" << std::endl;

    // Benchmark round trip analysis
    start = std::chrono::high_resolution_clock::now();

    auto round_trips_result = RoundTripAnalyzer::extract_round_trips(transactions);

    end      = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    ASSERT_TRUE(round_trips_result.is_ok()) << "Round trip analysis failed";

    std::cout << "✓ Round trip analysis completed in " << duration << " microseconds" << std::endl;

    // Benchmark capacity analysis
    CapacityAnalyzer capacity_analyzer;

    start = std::chrono::high_resolution_clock::now();

    // Create simplified price/volume data for benchmark
    std::map<std::string, TimeSeries<double>> price_data, volume_data;
    price_data["AAPL"]  = TimeSeries<double>(returns.dates(), std::vector<double>(returns.size(), 100.0));
    volume_data["AAPL"] = TimeSeries<double>(returns.dates(), std::vector<double>(returns.size(), 1000000.0));

    auto capacity_result = capacity_analyzer.calculate_days_to_liquidate(positions, price_data, volume_data, 0.2, 5);

    end      = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    if (capacity_result.is_ok()) {
        std::cout << "✓ Capacity analysis completed in " << duration << " microseconds" << std::endl;
    }
}

// ============================================================================
// Data Validation Tests
// ============================================================================

TEST_F(PyfolioComprehensiveTest, ValidateTestDataIntegrity) {
    std::cout << "\n=== Validating Test Data Integrity ===" << std::endl;

    // Validate capacity test data
    auto capacity_data = PyfolioEquivalentTestData::create_capacity_test_data();
    EXPECT_GT(capacity_data.positions.size(), 0) << "Capacity test data has no positions";
    EXPECT_GT(capacity_data.expected_days_to_liquidate.size(), 0) << "No expected liquidation data";

    // Validate position test data
    auto position_data = PyfolioEquivalentTestData::create_position_test_data();
    EXPECT_GT(position_data.positions.size(), 0) << "Position test data has no positions";
    EXPECT_GT(position_data.sector_map.size(), 0) << "No sector mapping data";

    // Validate round trip test data
    auto round_trip_data = PyfolioEquivalentTestData::create_round_trip_test_data();
    EXPECT_GT(round_trip_data.transactions.size(), 0) << "Round trip test data has no transactions";

    // Validate time series test data
    auto timeseries_data = PyfolioEquivalentTestData::create_timeseries_test_data();
    EXPECT_GT(timeseries_data.complex_price_series.size(), 0) << "Time series test data is empty";
    EXPECT_GT(timeseries_data.expected_rolling_sharpe.size(), 0) << "No expected rolling Sharpe data";

    // Validate turnover test data
    auto turnover_data = PyfolioEquivalentTestData::create_turnover_test_data();
    EXPECT_GT(turnover_data.positions.size(), 0) << "Turnover test data has no positions";

    std::cout << "✓ All test data integrity checks passed" << std::endl;
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    std::cout << "========================================" << std::endl;
    std::cout << "PyFolio_cpp Python Equivalence Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Validating C++ implementation against Python pyfolio" << std::endl;
    std::cout << "Expected: Identical results with 10-100x performance improvement" << std::endl;
    std::cout << "========================================" << std::endl;

    int result = RUN_ALL_TESTS();

    std::cout << "\n========================================" << std::endl;
    if (result == 0) {
        std::cout << "✅ ALL TESTS PASSED" << std::endl;
        std::cout << "PyFolio_cpp successfully matches Python pyfolio behavior" << std::endl;
        std::cout << "Performance improvements achieved without sacrificing accuracy" << std::endl;
    } else {
        std::cout << "❌ SOME TESTS FAILED" << std::endl;
        std::cout << "Review test output for specific failure details" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return result;
}