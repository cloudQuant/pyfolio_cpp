#include <gtest/gtest.h>
#include <pyfolio/transactions/round_trips.h>

using namespace pyfolio;

class RoundTripsTest : public ::testing::Test {
  protected:
    void SetUp() override {
        base_date = DateTime::parse("2024-01-15").value();

        // Create a series of transactions that form round trips
        transactions = {
            // First round trip for AAPL: Buy 100, Sell 100
            pyfolio::transactions::TransactionRecord{ "AAPL",              base_date,  100.0,  150.00,
                                                     pyfolio::transactions::TransactionType::Buy, "USD" },
            pyfolio::transactions::TransactionRecord{ "AAPL",  base_date.add_days(5), -100.0,  155.00,
                                                     pyfolio::transactions::TransactionType::Sell, "USD"},

            // Second round trip for AAPL: Buy 200, Sell 50, Sell 150
            pyfolio::transactions::TransactionRecord{ "AAPL", base_date.add_days(10),  200.0,  148.00,
                                                     pyfolio::transactions::TransactionType::Buy, "USD" },
            pyfolio::transactions::TransactionRecord{ "AAPL", base_date.add_days(15),  -50.0,  152.00,
                                                     pyfolio::transactions::TransactionType::Sell, "USD"},
            pyfolio::transactions::TransactionRecord{ "AAPL", base_date.add_days(20), -150.0,  156.00,
                                                     pyfolio::transactions::TransactionType::Sell, "USD"},

            // Round trip for MSFT
            pyfolio::transactions::TransactionRecord{ "MSFT", base_date.add_days(12),   80.0,  300.00,
                                                     pyfolio::transactions::TransactionType::Buy, "USD" },
            pyfolio::transactions::TransactionRecord{ "MSFT", base_date.add_days(18),  -80.0,  310.00,
                                                     pyfolio::transactions::TransactionType::Sell, "USD"},

            // Partial position (no complete round trip)
            pyfolio::transactions::TransactionRecord{"GOOGL", base_date.add_days(25),   50.0, 2500.00,
                                                     pyfolio::transactions::TransactionType::Buy, "USD" }
        };

        // Create transaction series
        for (const auto& txn : transactions) {
            txn_series.add_transaction(txn);
        }
    }

    DateTime base_date;
    std::vector<pyfolio::transactions::TransactionRecord> transactions;
    pyfolio::transactions::TransactionSeries txn_series;
};

TEST_F(RoundTripsTest, ExtractBasicRoundTrips) {
    pyfolio::transactions::RoundTripAnalyzer analyzer;
    auto round_trips = analyzer.analyze(txn_series);
    ASSERT_TRUE(round_trips.is_ok());

    auto trips = round_trips.value();

    // Should have at least 2 complete round trips (AAPL and MSFT)
    EXPECT_GE(trips.size(), 2);

    // Check that we have round trips for expected symbols
    bool found_aapl = false, found_msft = false;
    for (const auto& trip : trips) {
        if (trip.symbol == "AAPL")
            found_aapl = true;
        if (trip.symbol == "MSFT")
            found_msft = true;
    }

    EXPECT_TRUE(found_aapl);
    EXPECT_TRUE(found_msft);
}

TEST_F(RoundTripsTest, RoundTripProfitLoss) {
    pyfolio::transactions::RoundTripAnalyzer analyzer;
    auto round_trips = analyzer.analyze(txn_series);
    ASSERT_TRUE(round_trips.is_ok());

    auto trips = round_trips.value();

    for (const auto& trip : trips) {
        // Check that P&L is calculated correctly
        double expected_pnl = (trip.close_price - trip.open_price) * trip.shares;
        EXPECT_NEAR(trip.pnl(), expected_pnl, 1e-10);

        // Check return calculation
        double expected_return = (trip.close_price - trip.open_price) / trip.open_price;
        EXPECT_NEAR(trip.return_pct(), expected_return, 1e-10);
    }
}

TEST_F(RoundTripsTest, RoundTripDuration) {
    pyfolio::transactions::RoundTripAnalyzer analyzer;
    auto round_trips = analyzer.analyze(txn_series);
    ASSERT_TRUE(round_trips.is_ok());

    auto trips = round_trips.value();

    for (const auto& trip : trips) {
        // All round trips should have positive duration
        EXPECT_GT(trip.duration_days(), 0);

        // Buy date should be before sell date
        EXPECT_LT(trip.open_date, trip.close_date);
    }
}

TEST_F(RoundTripsTest, FIFOOrdering) {
    // Test that FIFO (First In, First Out) ordering is maintained
    pyfolio::transactions::RoundTripAnalyzer analyzer;
    auto round_trips = analyzer.analyze(txn_series);
    ASSERT_TRUE(round_trips.is_ok());

    auto trips = round_trips.value();

    // Find AAPL round trips
    std::vector<pyfolio::transactions::RoundTrip> aapl_trips;
    for (const auto& trip : trips) {
        if (trip.symbol == "AAPL") {
            aapl_trips.push_back(trip);
        }
    }

    // Should have at least one AAPL round trip
    EXPECT_GE(aapl_trips.size(), 1);

    // First AAPL round trip should use the first buy and first sell
    if (!aapl_trips.empty()) {
        const auto& first_trip = aapl_trips[0];
        EXPECT_EQ(first_trip.open_date, base_date);
        EXPECT_DOUBLE_EQ(first_trip.open_price, 150.00);
        EXPECT_DOUBLE_EQ(first_trip.close_price, 155.00);
    }
}

TEST_F(RoundTripsTest, RoundTripStatistics) {
    pyfolio::transactions::RoundTripAnalyzer analyzer;
    auto round_trips = analyzer.analyze(txn_series);
    ASSERT_TRUE(round_trips.is_ok());

    auto trips = round_trips.value();
    auto stats = pyfolio::transactions::RoundTripStatistics::calculate(trips);
    ASSERT_TRUE(stats.is_ok());

    auto result = stats.value();

    EXPECT_EQ(result.total_trips, trips.size());
    EXPECT_GE(result.total_trips, 0);

    if (result.total_trips > 0) {
        EXPECT_TRUE(std::isfinite(result.total_pnl));
        EXPECT_TRUE(std::isfinite(result.average_pnl));
        EXPECT_TRUE(std::isfinite(result.average_return));
        EXPECT_GT(result.average_duration_days, 0.0);
        EXPECT_GE(result.winning_trips, 0);
        EXPECT_GE(result.losing_trips, 0);
        EXPECT_EQ(result.winning_trips + result.losing_trips, result.total_trips);
    }
}

TEST_F(RoundTripsTest, WinLossRatio) {
    pyfolio::transactions::RoundTripAnalyzer analyzer;
    auto round_trips = analyzer.analyze(txn_series);
    ASSERT_TRUE(round_trips.is_ok());

    auto trips = round_trips.value();
    auto stats = pyfolio::transactions::RoundTripStatistics::calculate(trips);
    ASSERT_TRUE(stats.is_ok());

    auto result = stats.value();

    if (result.total_trips > 0) {
        // Win rate should be between 0 and 1
        EXPECT_GE(result.win_rate, 0.0);
        EXPECT_LE(result.win_rate, 1.0);

        // Check consistency
        double expected_win_rate = static_cast<double>(result.winning_trips) / result.total_trips;
        EXPECT_NEAR(result.win_rate, expected_win_rate, 1e-10);

        // Profit factor should be positive if there are any winning trades
        if (result.winning_trips > 0) {
            EXPECT_GT(result.profit_factor, 0.0);
        }
    }
}

TEST_F(RoundTripsTest, LargestWinnerLoser) {
    pyfolio::transactions::RoundTripAnalyzer analyzer;
    auto round_trips = analyzer.analyze(txn_series);
    ASSERT_TRUE(round_trips.is_ok());

    auto trips = round_trips.value();
    auto stats = pyfolio::transactions::RoundTripStatistics::calculate(trips);
    ASSERT_TRUE(stats.is_ok());

    auto result = stats.value();

    if (result.total_trips > 0) {
        // Find actual largest winner and loser
        double max_pnl = std::numeric_limits<double>::lowest();
        double min_pnl = std::numeric_limits<double>::max();

        for (const auto& trip : trips) {
            max_pnl = std::max(max_pnl, trip.pnl());
            min_pnl = std::min(min_pnl, trip.pnl());
        }

        EXPECT_NEAR(result.best_trade_pnl, max_pnl, 1e-10);
        EXPECT_NEAR(result.worst_trade_pnl, min_pnl, 1e-10);
    }
}

TEST_F(RoundTripsTest, PartialPositions) {
    // Test handling of partial positions (not complete round trips)
    pyfolio::transactions::RoundTripAnalyzer analyzer;
    auto round_trips = analyzer.analyze(txn_series);
    ASSERT_TRUE(round_trips.is_ok());

    auto open_positions = analyzer.get_open_positions();

    // Should have GOOGL as an open position (bought but not sold)
    EXPECT_GT(open_positions.size(), 0);

    bool found_googl = false;
    for (const auto& [symbol, positions] : open_positions) {
        if (symbol == "GOOGL") {
            found_googl = true;
            EXPECT_GT(positions.size(), 0);
            if (!positions.empty()) {
                EXPECT_DOUBLE_EQ(positions[0].shares, 50.0);
                EXPECT_DOUBLE_EQ(positions[0].price, 2500.00);
            }
        }
    }

    EXPECT_TRUE(found_googl);
}

TEST_F(RoundTripsTest, RoundTripsBySymbol) {
    pyfolio::transactions::RoundTripAnalyzer analyzer;
    auto round_trips = analyzer.analyze(txn_series);
    ASSERT_TRUE(round_trips.is_ok());

    auto trips         = round_trips.value();
    auto symbol_groups = pyfolio::transactions::group_by_symbol(trips);

    // Should have groups for symbols with complete round trips
    EXPECT_GT(symbol_groups.size(), 0);

    // Check that each group contains only trips for that symbol
    for (const auto& [symbol, symbol_trips] : symbol_groups) {
        for (const auto& trip : symbol_trips) {
            EXPECT_EQ(trip.symbol, symbol);
        }
    }
}

TEST_F(RoundTripsTest, EmptyTransactionSeries) {
    pyfolio::transactions::TransactionSeries empty_series;

    pyfolio::transactions::RoundTripAnalyzer analyzer;
    auto round_trips = analyzer.analyze(empty_series);
    ASSERT_TRUE(round_trips.is_ok());

    auto trips = round_trips.value();
    EXPECT_EQ(trips.size(), 0);

    auto stats = pyfolio::transactions::RoundTripStatistics::calculate(trips);
    ASSERT_TRUE(stats.is_ok());

    auto result = stats.value();
    EXPECT_EQ(result.total_trips, 0);
}

TEST_F(RoundTripsTest, SingleTransactionNoRoundTrip) {
    pyfolio::transactions::TransactionSeries single_txn_series;
    pyfolio::transactions::TransactionRecord single_txn{
        "AAPL", base_date, 100.0, 150.0, pyfolio::transactions::TransactionType::Buy, "USD"};
    single_txn_series.add_transaction(single_txn);

    pyfolio::transactions::RoundTripAnalyzer analyzer;
    auto round_trips = analyzer.analyze(single_txn_series);
    ASSERT_TRUE(round_trips.is_ok());

    auto trips = round_trips.value();
    EXPECT_EQ(trips.size(), 0);  // No complete round trips

    // But should show up in open positions
    auto open_positions = analyzer.get_open_positions();
    EXPECT_EQ(open_positions.size(), 1);

    bool found_aapl = false;
    for (const auto& [symbol, positions] : open_positions) {
        if (symbol == "AAPL") {
            found_aapl = true;
            EXPECT_GT(positions.size(), 0);
        }
    }
    EXPECT_TRUE(found_aapl);
}

TEST_F(RoundTripsTest, ComplexFIFOScenario) {
    // Create a more complex scenario to test FIFO logic
    pyfolio::transactions::TransactionSeries complex_series;

    // Multiple buys and sells
    complex_series.add_transaction(pyfolio::transactions::TransactionRecord{
        "AAPL", base_date, 100.0, 100.0, pyfolio::transactions::TransactionType::Buy, "USD"});
    complex_series.add_transaction(pyfolio::transactions::TransactionRecord{
        "AAPL", base_date.add_days(1), 200.0, 101.0, pyfolio::transactions::TransactionType::Buy, "USD"});
    complex_series.add_transaction(pyfolio::transactions::TransactionRecord{
        "AAPL", base_date.add_days(2), 150.0, 102.0, pyfolio::transactions::TransactionType::Buy, "USD"});
    complex_series.add_transaction(pyfolio::transactions::TransactionRecord{
        "AAPL", base_date.add_days(5), -50.0, 105.0, pyfolio::transactions::TransactionType::Sell, "USD"});
    complex_series.add_transaction(pyfolio::transactions::TransactionRecord{
        "AAPL", base_date.add_days(7), -100.0, 107.0, pyfolio::transactions::TransactionType::Sell, "USD"});
    complex_series.add_transaction(pyfolio::transactions::TransactionRecord{
        "AAPL", base_date.add_days(10), -250.0, 110.0, pyfolio::transactions::TransactionType::Sell, "USD"});

    pyfolio::transactions::RoundTripAnalyzer analyzer;
    auto round_trips = analyzer.analyze(complex_series);
    ASSERT_TRUE(round_trips.is_ok());

    auto trips = round_trips.value();

    // Should have multiple round trips
    EXPECT_GT(trips.size(), 0);

    // Verify FIFO logic: first sell should match with first buy
    if (!trips.empty()) {
        // Find the first round trip
        auto first_trip =
            std::min_element(trips.begin(), trips.end(),
                             [](const pyfolio::transactions::RoundTrip& a, const pyfolio::transactions::RoundTrip& b) {
                                 return a.close_date < b.close_date;
                             });

        // First sell was 50 shares, should match with part of first buy (100 shares at $100)
        EXPECT_DOUBLE_EQ(first_trip->open_price, 100.0);
        EXPECT_DOUBLE_EQ(first_trip->shares, 50.0);
    }
}