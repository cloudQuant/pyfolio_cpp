#include <gtest/gtest.h>
#include <pyfolio/transactions/transaction.h>

using namespace pyfolio;

class TransactionTest : public ::testing::Test {
  protected:
    void SetUp() override {
        base_date = DateTime::parse("2024-01-15").value();

        // Create sample transactions
        sample_transactions = {
            pyfolio::transactions::TransactionRecord{"AAPL",             base_date, 100.0, 150.00,
                                                     pyfolio::transactions::TransactionType::Buy, "USD" },
            pyfolio::transactions::TransactionRecord{"AAPL", base_date.add_days(1), -50.0, 152.00,
                                                     pyfolio::transactions::TransactionType::Sell, "USD"},
            pyfolio::transactions::TransactionRecord{"MSFT", base_date.add_days(2),  80.0, 300.00,
                                                     pyfolio::transactions::TransactionType::Buy, "USD" },
            pyfolio::transactions::TransactionRecord{"AAPL", base_date.add_days(5), -50.0, 155.00,
                                                     pyfolio::transactions::TransactionType::Sell, "USD"},
            pyfolio::transactions::TransactionRecord{"MSFT", base_date.add_days(7), -30.0, 305.00,
                                                     pyfolio::transactions::TransactionType::Sell, "USD"}
        };
    }

    DateTime base_date;
    std::vector<pyfolio::transactions::TransactionRecord> sample_transactions;
};

TEST_F(TransactionTest, TransactionRecordCreation) {
    pyfolio::transactions::TransactionRecord txn{
        "AAPL", base_date, 100.0, 150.00, pyfolio::transactions::TransactionType::Buy, "USD"};

    EXPECT_EQ(txn.symbol(), "AAPL");
    EXPECT_EQ(txn.date(), base_date);
    EXPECT_DOUBLE_EQ(txn.shares(), 100.0);
    EXPECT_DOUBLE_EQ(txn.price(), 150.00);
    EXPECT_EQ(txn.type(), pyfolio::transactions::TransactionType::Buy);
    EXPECT_EQ(txn.currency(), "USD");
    EXPECT_DOUBLE_EQ(txn.notional_value(), 15000.0);
}

TEST_F(TransactionTest, TransactionRecordValidation) {
    // Valid transaction
    auto valid_txn = pyfolio::transactions::TransactionRecord::create(
        "AAPL", base_date, 100.0, 150.00, pyfolio::transactions::TransactionType::Buy, "USD");
    EXPECT_TRUE(valid_txn.is_ok());

    // Invalid price
    auto invalid_price = pyfolio::transactions::TransactionRecord::create(
        "AAPL", base_date, 100.0, -150.00, pyfolio::transactions::TransactionType::Buy, "USD");
    EXPECT_TRUE(invalid_price.is_error());

    // Zero shares
    auto zero_shares = pyfolio::transactions::TransactionRecord::create(
        "AAPL", base_date, 0.0, 150.00, pyfolio::transactions::TransactionType::Buy, "USD");
    EXPECT_TRUE(zero_shares.is_error());

    // Empty symbol
    auto empty_symbol = pyfolio::transactions::TransactionRecord::create(
        "", base_date, 100.0, 150.00, pyfolio::transactions::TransactionType::Buy, "USD");
    EXPECT_TRUE(empty_symbol.is_error());
}

TEST_F(TransactionTest, TransactionSeriesCreation) {
    pyfolio::transactions::TransactionSeries series;

    for (const auto& txn : sample_transactions) {
        auto result = series.add_transaction(txn);
        EXPECT_TRUE(result.is_ok());
    }

    EXPECT_EQ(series.size(), sample_transactions.size());
    EXPECT_FALSE(series.empty());

    // Check chronological order
    for (size_t i = 1; i < series.size(); ++i) {
        EXPECT_LE(series[i - 1].date(), series[i].date());
    }
}

TEST_F(TransactionTest, TransactionSeriesFiltering) {
    pyfolio::transactions::TransactionSeries series;
    for (const auto& txn : sample_transactions) {
        series.add_transaction(txn);
    }

    // Filter by symbol
    auto aapl_txns = series.filter_by_symbol("AAPL");
    ASSERT_TRUE(aapl_txns.is_ok());

    auto aapl_series = aapl_txns.value();
    EXPECT_EQ(aapl_series.size(), 3);  // 3 AAPL transactions

    for (size_t i = 0; i < aapl_series.size(); ++i) {
        EXPECT_EQ(aapl_series[i].symbol(), "AAPL");
    }

    // Filter by date range
    DateTime start_date = base_date;
    DateTime end_date   = base_date.add_days(3);

    auto date_filtered = series.filter_by_date_range(start_date, end_date);
    ASSERT_TRUE(date_filtered.is_ok());

    auto filtered_series = date_filtered.value();
    for (size_t i = 0; i < filtered_series.size(); ++i) {
        EXPECT_GE(filtered_series[i].date(), start_date);
        EXPECT_LE(filtered_series[i].date(), end_date);
    }
}

TEST_F(TransactionTest, TransactionSeriesAggregation) {
    pyfolio::transactions::TransactionSeries series;
    for (const auto& txn : sample_transactions) {
        series.add_transaction(txn);
    }

    // Get daily aggregation
    auto daily_agg = series.aggregate_daily();
    ASSERT_TRUE(daily_agg.is_ok());

    auto daily_map = daily_agg.value();
    EXPECT_GT(daily_map.size(), 0);

    // Check that we have multiple days represented
    EXPECT_GE(daily_map.size(), 4);  // At least 4 different days from our sample

    // Get symbol aggregation
    auto symbol_agg = series.aggregate_by_symbol();
    ASSERT_TRUE(symbol_agg.is_ok());

    auto symbol_map = symbol_agg.value();
    EXPECT_EQ(symbol_map.size(), 2);  // AAPL and MSFT
    EXPECT_GT(symbol_map["AAPL"].size(), 0);
    EXPECT_GT(symbol_map["MSFT"].size(), 0);
}

TEST_F(TransactionTest, TotalNotionalValue) {
    pyfolio::transactions::TransactionSeries series;
    for (const auto& txn : sample_transactions) {
        series.add_transaction(txn);
    }

    auto total_notional = series.total_notional_value();
    ASSERT_TRUE(total_notional.is_ok());

    double expected_total = 0.0;
    for (const auto& txn : sample_transactions) {
        expected_total += std::abs(txn.notional_value());
    }

    EXPECT_NEAR(total_notional.value(), expected_total, 1e-10);
}

TEST_F(TransactionTest, NetSharesBySymbol) {
    pyfolio::transactions::TransactionSeries series;
    for (const auto& txn : sample_transactions) {
        series.add_transaction(txn);
    }

    auto net_shares = series.net_shares_by_symbol();
    ASSERT_TRUE(net_shares.is_ok());

    auto shares_map = net_shares.value();

    // AAPL: +100 -50 -50 = 0
    EXPECT_DOUBLE_EQ(shares_map["AAPL"], 0.0);

    // MSFT: +80 -30 = 50
    EXPECT_DOUBLE_EQ(shares_map["MSFT"], 50.0);
}

TEST_F(TransactionTest, AverageTransactionSize) {
    pyfolio::transactions::TransactionSeries series;
    for (const auto& txn : sample_transactions) {
        series.add_transaction(txn);
    }

    auto avg_size = series.average_transaction_size();
    ASSERT_TRUE(avg_size.is_ok());

    double total_notional = 0.0;
    for (const auto& txn : sample_transactions) {
        total_notional += std::abs(txn.notional_value());
    }

    double expected_avg = total_notional / sample_transactions.size();
    EXPECT_NEAR(avg_size.value(), expected_avg, 1e-10);
}

TEST_F(TransactionTest, TransactionStatistics) {
    pyfolio::transactions::TransactionSeries series;
    for (const auto& txn : sample_transactions) {
        series.add_transaction(txn);
    }

    auto stats = series.calculate_statistics();
    ASSERT_TRUE(stats.is_ok());

    auto result = stats.value();
    EXPECT_EQ(result.total_transactions, sample_transactions.size());
    EXPECT_GT(result.total_notional_value, 0.0);
    EXPECT_GT(result.average_transaction_size, 0.0);
    EXPECT_EQ(result.unique_symbols, 2);  // AAPL and MSFT
    EXPECT_GT(result.trading_days, 0);
}

TEST_F(TransactionTest, TransactionCosts) {
    pyfolio::transactions::TransactionSeries series;
    for (const auto& txn : sample_transactions) {
        series.add_transaction(txn);
    }

    // Test with simple flat commission
    double commission_per_trade = 1.0;
    auto total_costs            = series.calculate_transaction_costs(commission_per_trade);
    ASSERT_TRUE(total_costs.is_ok());

    double expected_costs = sample_transactions.size() * commission_per_trade;
    EXPECT_NEAR(total_costs.value(), expected_costs, 1e-10);
}

TEST_F(TransactionTest, EmptySeriesHandling) {
    pyfolio::transactions::TransactionSeries empty_series;

    EXPECT_TRUE(empty_series.empty());
    EXPECT_EQ(empty_series.size(), 0);

    auto total_notional = empty_series.total_notional_value();
    EXPECT_TRUE(total_notional.is_error());

    auto stats = empty_series.calculate_statistics();
    EXPECT_TRUE(stats.is_error());
}

TEST_F(TransactionTest, TransactionSeriesSorting) {
    pyfolio::transactions::TransactionSeries series;

    // Add transactions in non-chronological order
    series.add_transaction(sample_transactions[2]);  // Day 2
    series.add_transaction(sample_transactions[0]);  // Day 0
    series.add_transaction(sample_transactions[1]);  // Day 1

    // Series should maintain chronological order
    EXPECT_LE(series[0].date(), series[1].date());
    EXPECT_LE(series[1].date(), series[2].date());
}

TEST_F(TransactionTest, DuplicateTransactionHandling) {
    pyfolio::transactions::TransactionSeries series;

    auto txn     = sample_transactions[0];
    auto result1 = series.add_transaction(txn);
    EXPECT_TRUE(result1.is_ok());

    // Adding same transaction again should work (allows for identical transactions)
    auto result2 = series.add_transaction(txn);
    EXPECT_TRUE(result2.is_ok());

    EXPECT_EQ(series.size(), 2);
}

TEST_F(TransactionTest, TransactionTypeConsistency) {
    // Test that transaction types are consistent with share quantities
    pyfolio::transactions::TransactionRecord buy_positive{
        "AAPL", base_date, 100.0, 150.0, pyfolio::transactions::TransactionType::Buy, "USD"};
    EXPECT_GT(buy_positive.shares(), 0.0);

    pyfolio::transactions::TransactionRecord sell_negative{
        "AAPL", base_date, -100.0, 150.0, pyfolio::transactions::TransactionType::Sell, "USD"};
    EXPECT_LT(sell_negative.shares(), 0.0);
}

TEST_F(TransactionTest, CurrencyHandling) {
    pyfolio::transactions::TransactionRecord usd_txn{
        "AAPL", base_date, 100.0, 150.0, pyfolio::transactions::TransactionType::Buy, "USD"};
    pyfolio::transactions::TransactionRecord eur_txn{
        "AAPL", base_date, 100.0, 150.0, pyfolio::transactions::TransactionType::Buy, "EUR"};

    EXPECT_EQ(usd_txn.currency(), "USD");
    EXPECT_EQ(eur_txn.currency(), "EUR");
    EXPECT_NE(usd_txn.currency(), eur_txn.currency());
}