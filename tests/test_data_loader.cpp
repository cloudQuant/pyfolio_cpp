#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <pyfolio/core/time_series.h>
#include <pyfolio/core/types.h>
#include <pyfolio/io/data_loader.h>
#include <sstream>

using namespace pyfolio;
using namespace pyfolio::io;

class DataLoaderTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Create temporary directory for test files
        test_dir_ = std::filesystem::temp_directory_path() / "pyfolio_test";
        std::filesystem::create_directories(test_dir_);

        // Create sample CSV content
        createSampleCSVFiles();
    }

    void TearDown() override {
        // Clean up test files
        std::filesystem::remove_all(test_dir_);
    }

    void createSampleCSVFiles() {
        // Create returns CSV
        {
            std::ofstream file(test_dir_ / "returns.csv");
            file << "date,return\n";
            file << "2023-01-01,0.01\n";
            file << "2023-01-02,-0.005\n";
            file << "2023-01-03,0.008\n";
            file << "2023-01-04,-0.002\n";
            file << "2023-01-05,0.012\n";
        }

        // Create positions CSV
        {
            std::ofstream file(test_dir_ / "positions.csv");
            file << "date,symbol,shares,price\n";
            file << "2023-01-01,AAPL,100,150.0\n";
            file << "2023-01-01,GOOGL,50,2800.0\n";
            file << "2023-01-02,AAPL,100,152.0\n";
            file << "2023-01-02,GOOGL,50,2850.0\n";
            file << "2023-01-03,AAPL,120,151.0\n";
            file << "2023-01-03,GOOGL,45,2820.0\n";
        }

        // Create transactions CSV
        {
            std::ofstream file(test_dir_ / "transactions.csv");
            file << "datetime,symbol,shares,price,side\n";
            file << "2023-01-01 09:30:00,AAPL,100,150.0,buy\n";
            file << "2023-01-01 10:15:00,GOOGL,50,2800.0,buy\n";
            file << "2023-01-03 11:30:00,AAPL,20,151.0,buy\n";
            file << "2023-01-03 14:45:00,GOOGL,-5,2820.0,sell\n";
        }

        // Create factor returns CSV
        {
            std::ofstream file(test_dir_ / "factor_returns.csv");
            file << "date,momentum,value,size,profitability\n";
            file << "2023-01-01,0.001,-0.002,0.003,0.001\n";
            file << "2023-01-02,-0.001,0.001,-0.002,0.000\n";
            file << "2023-01-03,0.002,0.000,0.001,-0.001\n";
        }

        // Create market data CSV
        {
            std::ofstream file(test_dir_ / "market_data.csv");
            file << "date,symbol,open,high,low,close,volume\n";
            file << "2023-01-01,AAPL,149.0,152.0,148.5,150.0,1000000\n";
            file << "2023-01-01,GOOGL,2790.0,2810.0,2785.0,2800.0,500000\n";
            file << "2023-01-02,AAPL,150.5,154.0,150.0,152.0,1200000\n";
            file << "2023-01-02,GOOGL,2800.0,2860.0,2795.0,2850.0,600000\n";
        }

        // Create malformed CSV for error testing
        {
            std::ofstream file(test_dir_ / "malformed.csv");
            file << "date,return\n";
            file << "2023-01-01,0.01\n";
            file << "invalid_date,not_a_number\n";
            file << "2023-01-03,0.008\n";
        }

        // Create CSV with different delimiter
        {
            std::ofstream file(test_dir_ / "semicolon.csv");
            file << "date;return\n";
            file << "2023-01-01;0.01\n";
            file << "2023-01-02;-0.005\n";
        }
    }

    std::filesystem::path test_dir_;
};

TEST_F(DataLoaderTest, TestLoadReturnsFromCSV) {
    CSVConfig config;
    auto result = load_returns_from_csv((test_dir_ / "returns.csv").string(), config);

    ASSERT_TRUE(result.has_value());

    const auto& returns = result.value();
    EXPECT_EQ(returns.size(), 5);

    const auto& values     = returns.values();
    const auto& timestamps = returns.timestamps();
    EXPECT_DOUBLE_EQ(values[0], 0.01);
    EXPECT_DOUBLE_EQ(values[1], -0.005);
    EXPECT_DOUBLE_EQ(values[2], 0.008);
    EXPECT_DOUBLE_EQ(values[3], -0.002);
    EXPECT_DOUBLE_EQ(values[4], 0.012);

    // Check dates
    EXPECT_EQ(timestamps[0].year(), 2023);
    EXPECT_EQ(timestamps[0].month(), 1);
    EXPECT_EQ(timestamps[0].day(), 1);
}

TEST_F(DataLoaderTest, TestLoadPositionsFromCSV) {
    CSVConfig config;
    auto result = load_positions_from_csv((test_dir_ / "positions.csv").string(), config);

    ASSERT_TRUE(result.has_value());

    const auto& positions = result.value();
    EXPECT_EQ(positions.size(), 3);  // 3 unique dates

    const auto& values = positions.values();

    // Check first date
    const auto& first_positions = values[0];
    EXPECT_EQ(first_positions.size(), 2);  // AAPL and GOOGL

    EXPECT_TRUE(first_positions.find("AAPL") != first_positions.end());
    EXPECT_TRUE(first_positions.find("GOOGL") != first_positions.end());

    const auto& aapl_pos = first_positions.at("AAPL");
    EXPECT_EQ(aapl_pos.shares, 100);
    EXPECT_DOUBLE_EQ(aapl_pos.price, 150.0);
}

TEST_F(DataLoaderTest, TestLoadTransactionsFromCSV) {
    CSVConfig config;
    auto result = load_transactions_from_csv((test_dir_ / "transactions.csv").string(), config);

    ASSERT_TRUE(result.has_value());

    const auto& transactions = result.value();
    EXPECT_EQ(transactions.size(), 4);

    // Check first transaction
    const auto& first_txn = transactions[0];
    EXPECT_EQ(first_txn.symbol, "AAPL");
    EXPECT_EQ(first_txn.shares, 100);
    EXPECT_DOUBLE_EQ(first_txn.price, 150.0);
    EXPECT_EQ(first_txn.side, TransactionSide::Buy);

    // Check sell transaction
    const auto& sell_txn = transactions[3];
    EXPECT_EQ(sell_txn.symbol, "GOOGL");
    EXPECT_EQ(sell_txn.shares, -5);  // Should be negative for sells
    EXPECT_EQ(sell_txn.side, TransactionSide::Sell);
}

TEST_F(DataLoaderTest, TestLoadFactorReturnsFromCSV) {
    CSVConfig config;
    auto result = load_factor_returns_from_csv((test_dir_ / "factor_returns.csv").string(), config);

    ASSERT_TRUE(result.has_value());

    const auto& factor_returns = result.value();
    EXPECT_EQ(factor_returns.size(), 3);

    const auto& values        = factor_returns.values();
    const auto& first_factors = values[0];

    EXPECT_EQ(first_factors.size(), 4);  // 4 factors
    EXPECT_TRUE(first_factors.find("momentum") != first_factors.end());
    EXPECT_TRUE(first_factors.find("value") != first_factors.end());
    EXPECT_TRUE(first_factors.find("size") != first_factors.end());
    EXPECT_TRUE(first_factors.find("profitability") != first_factors.end());

    EXPECT_DOUBLE_EQ(first_factors.at("momentum"), 0.001);
    EXPECT_DOUBLE_EQ(first_factors.at("value"), -0.002);
}

TEST_F(DataLoaderTest, TestLoadMarketDataFromCSV) {
    CSVConfig config;
    auto result = load_market_data_from_csv((test_dir_ / "market_data.csv").string(), config);

    ASSERT_TRUE(result.has_value());

    const auto& market_data = result.value();
    EXPECT_EQ(market_data.size(), 2);  // 2 unique dates

    const auto& values     = market_data.values();
    const auto& first_data = values[0];

    EXPECT_EQ(first_data.size(), 2);  // AAPL and GOOGL
    EXPECT_TRUE(first_data.find("AAPL") != first_data.end());

    const auto& aapl_data = first_data.at("AAPL");
    EXPECT_DOUBLE_EQ(aapl_data.open, 149.0);
    EXPECT_DOUBLE_EQ(aapl_data.high, 152.0);
    EXPECT_DOUBLE_EQ(aapl_data.low, 148.5);
    EXPECT_DOUBLE_EQ(aapl_data.close, 150.0);
    EXPECT_EQ(aapl_data.volume, 1000000);
}

TEST_F(DataLoaderTest, TestCustomDelimiter) {
    CSVConfig config;
    config.delimiter = ';';

    auto result = load_returns_from_csv((test_dir_ / "semicolon.csv").string(), config);

    ASSERT_TRUE(result.has_value());

    const auto& returns = result.value();
    EXPECT_EQ(returns.size(), 2);

    const auto& values = returns.values();
    EXPECT_DOUBLE_EQ(values[0], 0.01);
    EXPECT_DOUBLE_EQ(values[1], -0.005);
}

TEST_F(DataLoaderTest, TestFileNotFound) {
    CSVConfig config;
    auto result = load_returns_from_csv("nonexistent_file.csv", config);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::FileNotFound);
}

TEST_F(DataLoaderTest, TestMalformedData) {
    CSVConfig config;
    auto result = load_returns_from_csv((test_dir_ / "malformed.csv").string(), config);

    // Should either fail or skip malformed rows
    if (result.has_value()) {
        // If it succeeds, should have fewer rows than expected
        EXPECT_LT(result.value().size(), 3);
    } else {
        // Or it should fail with appropriate error
        EXPECT_EQ(result.error().code, ErrorCode::InvalidInput);
    }
}

TEST_F(DataLoaderTest, TestSaveReturnsToCSV) {
    // First load some data
    CSVConfig config;
    auto load_result = load_returns_from_csv((test_dir_ / "returns.csv").string(), config);
    ASSERT_TRUE(load_result.has_value());

    // Save to new file
    auto output_path = test_dir_ / "output_returns.csv";
    auto save_result = save_returns_to_csv(load_result.value(), output_path.string(), config);

    EXPECT_TRUE(save_result.has_value());

    // Verify file was created
    EXPECT_TRUE(std::filesystem::exists(output_path));

    // Load it back and verify
    auto reload_result = load_returns_from_csv(output_path.string(), config);
    ASSERT_TRUE(reload_result.has_value());

    const auto& original = load_result.value();
    const auto& reloaded = reload_result.value();

    EXPECT_EQ(original.size(), reloaded.size());

    // Check values match (within tolerance)
    const auto& orig_values   = original.values();
    const auto& reload_values = reloaded.values();

    for (size_t i = 0; i < orig_values.size(); ++i) {
        EXPECT_NEAR(orig_values[i], reload_values[i], 1e-10);
    }
}

TEST_F(DataLoaderTest, TestCSVParser) {
    CSVParser parser;

    std::string csv_content =
        "date,value,description\n"
        "2023-01-01,1.5,\"Test, with comma\"\n"
        "2023-01-02,2.0,Simple value\n";

    auto result = parser.parse_string(csv_content);

    ASSERT_TRUE(result.has_value());

    const auto& rows = result.value();
    EXPECT_EQ(rows.size(), 3);  // Header + 2 data rows

    // Check header
    EXPECT_EQ(rows[0].size(), 3);
    EXPECT_EQ(rows[0][0], "date");
    EXPECT_EQ(rows[0][1], "value");
    EXPECT_EQ(rows[0][2], "description");

    // Check data rows
    EXPECT_EQ(rows[1][0], "2023-01-01");
    EXPECT_EQ(rows[1][1], "1.5");
    EXPECT_EQ(rows[1][2], "Test, with comma");

    EXPECT_EQ(rows[2][2], "Simple value");
}

TEST_F(DataLoaderTest, TestGetColumnIndex) {
    CSVParser parser;

    std::vector<std::string> headers = {"date", "symbol", "price", "volume"};

    auto date_idx = parser.get_column_index("date", headers);
    ASSERT_TRUE(date_idx.has_value());
    EXPECT_EQ(date_idx.value(), 0);

    auto price_idx = parser.get_column_index("price", headers);
    ASSERT_TRUE(price_idx.has_value());
    EXPECT_EQ(price_idx.value(), 2);

    auto invalid_idx = parser.get_column_index("nonexistent", headers);
    EXPECT_FALSE(invalid_idx.has_value());
}

TEST_F(DataLoaderTest, TestValidation) {
    using namespace validation;

    // Test returns validation
    auto returns_result = load_returns_from_csv((test_dir_ / "returns.csv").string());
    ASSERT_TRUE(returns_result.has_value());

    auto validation_result = validate_returns(returns_result.value());
    EXPECT_TRUE(validation_result.has_value());

    // Test positions validation
    auto positions_result = load_positions_from_csv((test_dir_ / "positions.csv").string());
    ASSERT_TRUE(positions_result.has_value());

    auto pos_validation_result = validate_positions(positions_result.value());
    EXPECT_TRUE(pos_validation_result.has_value());

    // Test transactions validation
    auto transactions_result = load_transactions_from_csv((test_dir_ / "transactions.csv").string());
    ASSERT_TRUE(transactions_result.has_value());

    auto txn_validation_result = validate_transactions(transactions_result.value());
    EXPECT_TRUE(txn_validation_result.has_value());
}

TEST_F(DataLoaderTest, TestSampleDataGeneration) {
    using namespace sample_data;

    auto start_date = DateTime(2023, 1, 1);
    size_t num_days = 100;

    // Test sample returns generation
    auto returns = generate_random_returns(start_date, num_days);
    EXPECT_EQ(returns.size(), num_days);

    // Check dates are sequential
    const auto& timestamps = returns.timestamps();
    for (size_t i = 1; i < timestamps.size(); ++i) {
        EXPECT_GT(timestamps[i], timestamps[i - 1]);
    }

    // Test sample positions generation
    std::vector<std::string> symbols = {"AAPL", "GOOGL", "MSFT"};
    auto positions                   = generate_sample_positions(start_date, num_days, symbols);
    EXPECT_EQ(positions.size(), num_days);

    // Check all symbols are present
    if (positions.size() > 0) {
        const auto& first_pos = positions.values()[0];
        for (const auto& symbol : symbols) {
            EXPECT_TRUE(first_pos.find(symbol) != first_pos.end());
        }
    }

    // Test sample transactions generation
    auto end_date     = start_date.add_days(num_days);
    auto transactions = generate_sample_transactions(start_date, end_date, symbols, 20);
    EXPECT_EQ(transactions.size(), 20);

    // Check all transactions are within date range
    for (const auto& txn : transactions) {
        auto txn_datetime = DateTime::from_time_point(txn.timestamp);
        EXPECT_GE(txn_datetime, start_date);
        EXPECT_LE(txn_datetime, end_date);

        // Check symbol is from our list
        bool found = false;
        for (const auto& symbol : symbols) {
            if (txn.symbol == symbol) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found);
    }
}

// Main function removed - using shared Google Test main from CMake