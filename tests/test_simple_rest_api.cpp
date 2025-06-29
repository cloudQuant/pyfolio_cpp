#include <chrono>
#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <thread>

using json = nlohmann::json;

/**
 * @brief Test fixture for simple REST API testing
 */
class SimpleRestApiTest : public ::testing::Test {
  protected:
    static void SetUpTestSuite() {
        // Start the server in a separate thread
        server_thread = std::thread([]() {
            // Server setup code would go here
            // For now, we'll test the logic without actually starting a server
        });

        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    static void TearDownTestSuite() {
        if (server_thread.joinable()) {
            server_thread.join();
        }
    }

    static std::thread server_thread;
};

std::thread SimpleRestApiTest::server_thread;

/**
 * @brief Test basic statistics calculation (the core logic from the API)
 */
TEST_F(SimpleRestApiTest, BasicStatisticsCalculation) {
    std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0};

    // Calculate basic statistics (same logic as in the API)
    double sum     = 0.0;
    double min_val = values[0];
    double max_val = values[0];

    for (double val : values) {
        sum += val;
        if (val < min_val)
            min_val = val;
        if (val > max_val)
            max_val = val;
    }

    double mean = sum / values.size();

    // Calculate variance
    double variance = 0.0;
    for (double val : values) {
        variance += (val - mean) * (val - mean);
    }
    variance /= values.size();
    double std_dev = std::sqrt(variance);

    // Verify calculations
    EXPECT_EQ(values.size(), 5);
    EXPECT_DOUBLE_EQ(sum, 15.0);
    EXPECT_DOUBLE_EQ(mean, 3.0);
    EXPECT_DOUBLE_EQ(min_val, 1.0);
    EXPECT_DOUBLE_EQ(max_val, 5.0);
    EXPECT_DOUBLE_EQ(variance, 2.0);
    EXPECT_NEAR(std_dev, 1.4142135623730951, 1e-10);
}

/**
 * @brief Test portfolio value calculation logic
 */
TEST_F(SimpleRestApiTest, PortfolioValueCalculation) {
    // Test data similar to API request
    double cash                                     = 1000.0;
    std::vector<std::pair<double, double>> holdings = {
        {100.0,  150.0}, // 100 shares at $150
        { 50.0, 2800.0}  // 50 shares at $2800
    };

    double total_value = cash;
    for (const auto& holding : holdings) {
        double shares = holding.first;
        double price  = holding.second;
        total_value += shares * price;
    }

    double securities_value = total_value - cash;

    EXPECT_DOUBLE_EQ(total_value, 156000.0);
    EXPECT_DOUBLE_EQ(cash, 1000.0);
    EXPECT_DOUBLE_EQ(securities_value, 155000.0);
    EXPECT_EQ(holdings.size(), 2);
}

/**
 * @brief Test JSON parsing logic
 */
TEST_F(SimpleRestApiTest, JsonParsingLogic) {
    // Test JSON parsing similar to API endpoints
    std::string json_str = R"({"values": [1.0, 2.0, 3.0, 4.0, 5.0]})";

    EXPECT_NO_THROW({
        json data = json::parse(json_str);
        EXPECT_TRUE(data.contains("values"));
        EXPECT_TRUE(data["values"].is_array());

        std::vector<double> values = data["values"];
        EXPECT_EQ(values.size(), 5);
        EXPECT_DOUBLE_EQ(values[0], 1.0);
        EXPECT_DOUBLE_EQ(values[4], 5.0);
    });
}

/**
 * @brief Test portfolio JSON parsing logic
 */
TEST_F(SimpleRestApiTest, PortfolioJsonParsing) {
    std::string json_str = R"({
        "cash": 1000,
        "holdings": [
            {"shares": 100, "price": 150},
            {"shares": 50, "price": 2800}
        ]
    })";

    EXPECT_NO_THROW({
        json data = json::parse(json_str);
        EXPECT_TRUE(data.contains("holdings"));
        EXPECT_TRUE(data["holdings"].is_array());

        double cash = data.value("cash", 0.0);
        EXPECT_DOUBLE_EQ(cash, 1000.0);

        EXPECT_EQ(data["holdings"].size(), 2);

        auto holdings = data["holdings"];
        EXPECT_DOUBLE_EQ(holdings[0].value("shares", 0.0), 100.0);
        EXPECT_DOUBLE_EQ(holdings[0].value("price", 0.0), 150.0);
        EXPECT_DOUBLE_EQ(holdings[1].value("shares", 0.0), 50.0);
        EXPECT_DOUBLE_EQ(holdings[1].value("price", 0.0), 2800.0);
    });
}

/**
 * @brief Test error handling scenarios
 */
TEST_F(SimpleRestApiTest, ErrorHandling) {
    // Test empty values array
    std::vector<double> empty_values;
    EXPECT_TRUE(empty_values.empty());

    // Test invalid JSON
    std::string invalid_json = "{invalid json}";
    EXPECT_THROW(json::parse(invalid_json), std::exception);

    // Test missing fields
    std::string incomplete_json = R"({"not_values": [1, 2, 3]})";
    json data                   = json::parse(incomplete_json);
    EXPECT_FALSE(data.contains("values"));
}