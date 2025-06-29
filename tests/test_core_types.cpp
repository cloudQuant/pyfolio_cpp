#include <gtest/gtest.h>
#include <pyfolio/core/datetime.h>
#include <pyfolio/core/error_handling.h>
#include <pyfolio/core/types.h>
#include <thread>

using namespace pyfolio;

class CoreTypesTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Setup code
    }

    void TearDown() override {
        // Cleanup code
    }
};

// Test Result<T> monad functionality
TEST_F(CoreTypesTest, ResultMonadBasicFunctionality) {
    // Test successful result
    auto success_result = Result<int>::success(42);
    EXPECT_TRUE(success_result.is_ok());
    EXPECT_FALSE(success_result.is_error());
    EXPECT_EQ(success_result.value(), 42);
}

TEST_F(CoreTypesTest, ResultMonadErrorHandling) {
    // Test error result
    auto error_result = Result<int>::error(ErrorCode::InvalidInput, "Test error");
    EXPECT_FALSE(error_result.is_ok());
    EXPECT_TRUE(error_result.is_error());
    EXPECT_EQ(error_result.error().code, ErrorCode::InvalidInput);
    EXPECT_EQ(error_result.error().message, "Test error");
}

TEST_F(CoreTypesTest, ResultMonadChaining) {
    auto result = Result<int>::success(10);

    // Test value access
    if (result.is_ok()) {
        int value = result.value();
        EXPECT_EQ(value, 10);
    }
}

// Test DateTime functionality
TEST_F(CoreTypesTest, DateTimeBasicOperations) {
    auto date1 = DateTime::parse("2024-01-15");
    ASSERT_TRUE(date1.is_ok());

    auto date2 = date1.value().add_days(5);
    EXPECT_EQ(date2.to_string(), "2024-01-20");

    auto date3 = date1.value().add_months(1);
    EXPECT_EQ(date3.to_string(), "2024-02-15");
}

TEST_F(CoreTypesTest, DateTimeDifferences) {
    auto date1 = DateTime::parse("2024-01-01").value();
    auto date2 = DateTime::parse("2024-01-10").value();

    auto diff = date2.days_since(date1);
    EXPECT_EQ(diff, 9);
}

TEST_F(CoreTypesTest, DateTimeBusinessDays) {
    auto date1 = DateTime::parse("2024-01-01").value();  // Monday
    auto date2 = DateTime::parse("2024-01-08").value();  // Next Monday

    auto business_days = date1.business_days_until(date2);
    EXPECT_EQ(business_days, 5);  // 5 business days in a week
}

// Test business calendar
TEST_F(CoreTypesTest, BusinessCalendarHolidays) {
    BusinessCalendar calendar;

    // Add a holiday
    auto holiday = DateTime::parse("2024-07-04").value();  // Independence Day
    calendar.add_holiday(holiday);

    EXPECT_TRUE(calendar.is_holiday(holiday));
    EXPECT_FALSE(calendar.is_business_day(holiday));
}

TEST_F(CoreTypesTest, BusinessCalendarWeekends) {
    BusinessCalendar calendar;

    auto saturday = DateTime::parse("2024-01-06").value();  // Saturday
    auto sunday   = DateTime::parse("2024-01-07").value();  // Sunday
    auto monday   = DateTime::parse("2024-01-08").value();  // Monday

    EXPECT_FALSE(calendar.is_business_day(saturday));
    EXPECT_FALSE(calendar.is_business_day(sunday));
    EXPECT_TRUE(calendar.is_business_day(monday));
}

// Test financial types
TEST_F(CoreTypesTest, FinancialTypes) {
    Price price   = 100.50;
    Shares shares = 1000;
    Return ret    = 0.05;

    EXPECT_DOUBLE_EQ(price, 100.50);
    EXPECT_DOUBLE_EQ(shares, 1000.0);
    EXPECT_DOUBLE_EQ(ret, 0.05);
}

TEST_F(CoreTypesTest, SymbolHandling) {
    Symbol symbol1 = "AAPL";
    Symbol symbol2 = "MSFT";

    EXPECT_EQ(symbol1, "AAPL");
    EXPECT_NE(symbol1, symbol2);
    EXPECT_FALSE(symbol1.empty());
}

TEST_F(CoreTypesTest, CurrencyHandling) {
    Currency usd = "USD";
    Currency eur = "EUR";

    EXPECT_EQ(usd, "USD");
    EXPECT_NE(usd, eur);
    EXPECT_FALSE(usd.empty());
}

// Test constants
TEST_F(CoreTypesTest, Constants) {
    EXPECT_GT(constants::TRADING_DAYS_PER_YEAR, 0);
    EXPECT_LT(constants::TRADING_DAYS_PER_YEAR, 365);
    EXPECT_GT(constants::DEFAULT_CONFIDENCE_LEVEL, 0.0);
    EXPECT_LT(constants::DEFAULT_CONFIDENCE_LEVEL, 1.0);
    EXPECT_GT(constants::DEFAULT_RISK_FREE_RATE, 0.0);
    EXPECT_LT(constants::DEFAULT_RISK_FREE_RATE, 1.0);
}

// Test error handling edge cases
TEST_F(CoreTypesTest, ErrorHandlingEdgeCases) {
    // Test error message formatting
    auto error        = Error{ErrorCode::InvalidInput, "Test message"};
    auto error_string = error.to_string();
    EXPECT_FALSE(error_string.empty());
    EXPECT_NE(error_string.find("Test message"), std::string::npos);
}

TEST_F(CoreTypesTest, ResultValueOrDefault) {
    auto success_result = Result<double>::success(3.14);
    auto error_result   = Result<double>::error(ErrorCode::InvalidInput, "Error");

    EXPECT_DOUBLE_EQ(success_result.value_or(0.0), 3.14);
    EXPECT_DOUBLE_EQ(error_result.value_or(2.71), 2.71);
}

// Test DateTime edge cases
TEST_F(CoreTypesTest, DateTimeEdgeCases) {
    // Test invalid date parsing
    auto invalid_date = DateTime::parse("invalid-date");
    EXPECT_TRUE(invalid_date.is_error());

    // Test leap year handling
    auto leap_year_date = DateTime::parse("2024-02-29");
    EXPECT_TRUE(leap_year_date.is_ok());

    auto non_leap_year_date = DateTime::parse("2023-02-29");
    EXPECT_TRUE(non_leap_year_date.is_error());
}

TEST_F(CoreTypesTest, DateTimeComparison) {
    auto date1 = DateTime::parse("2024-01-01").value();
    auto date2 = DateTime::parse("2024-01-02").value();
    auto date3 = DateTime::parse("2024-01-01").value();

    EXPECT_LT(date1, date2);
    EXPECT_GT(date2, date1);
    EXPECT_EQ(date1, date3);
    EXPECT_LE(date1, date3);
    EXPECT_GE(date1, date3);
}

// Test thread safety (basic)
TEST_F(CoreTypesTest, BasicThreadSafety) {
    // Test that basic types can be used in concurrent contexts
    std::vector<std::thread> threads;
    std::atomic<int> counter{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&counter]() {
            auto result = Result<int>::success(42);
            if (result.is_ok()) {
                counter.fetch_add(1);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(counter.load(), 10);
}

// Performance test for basic operations
TEST_F(CoreTypesTest, PerformanceBaseline) {
    const size_t iterations = 100000;

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < iterations; ++i) {
        auto result = Result<double>::success(static_cast<double>(i));
        if (result.is_ok()) {
            volatile double value = result.value();  // Prevent optimization
            (void)value;                             // Suppress unused variable warning
        }
    }

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should complete within reasonable time (less than 100ms for 100k operations)
    EXPECT_LT(duration.count(), 100000);
}