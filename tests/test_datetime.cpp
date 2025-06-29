#include <gtest/gtest.h>
#include <pyfolio/core/datetime.h>

using namespace pyfolio;

class DateTimeTest : public ::testing::Test {
  protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DateTimeTest, ParseValidDates) {
    auto date1 = DateTime::parse("2024-01-15");
    ASSERT_TRUE(date1.is_ok());
    EXPECT_EQ(date1.value().to_string(), "2024-01-15");

    auto date2 = DateTime::parse("2023-12-31");
    ASSERT_TRUE(date2.is_ok());
    EXPECT_EQ(date2.value().to_string(), "2023-12-31");
}

TEST_F(DateTimeTest, ParseInvalidDates) {
    auto invalid1 = DateTime::parse("2024-13-01");
    EXPECT_TRUE(invalid1.is_error());

    auto invalid2 = DateTime::parse("invalid-date");
    EXPECT_TRUE(invalid2.is_error());

    auto invalid3 = DateTime::parse("2023-02-29");
    EXPECT_TRUE(invalid3.is_error());
}

TEST_F(DateTimeTest, DateArithmetic) {
    auto date = DateTime::parse("2024-01-15").value();

    auto plus_days = date.add_days(10);
    EXPECT_EQ(plus_days.to_string(), "2024-01-25");

    auto plus_months = date.add_months(2);
    EXPECT_EQ(plus_months.to_string(), "2024-03-15");

    auto plus_years = date.add_years(1);
    EXPECT_EQ(plus_years.to_string(), "2025-01-15");
}

TEST_F(DateTimeTest, BusinessDaysCalculation) {
    auto monday = DateTime::parse("2024-01-08").value();
    auto friday = DateTime::parse("2024-01-12").value();

    auto business_days = monday.business_days_until(friday);
    EXPECT_EQ(business_days, 4);
}

TEST_F(DateTimeTest, WeekdayDetection) {
    auto monday   = DateTime::parse("2024-01-08").value();
    auto saturday = DateTime::parse("2024-01-06").value();
    auto sunday   = DateTime::parse("2024-01-07").value();

    EXPECT_TRUE(monday.is_weekday());
    EXPECT_FALSE(saturday.is_weekday());
    EXPECT_FALSE(sunday.is_weekday());
}

TEST_F(DateTimeTest, BusinessCalendarBasic) {
    BusinessCalendar calendar;

    auto monday   = DateTime::parse("2024-01-08").value();
    auto saturday = DateTime::parse("2024-01-06").value();

    EXPECT_TRUE(calendar.is_business_day(monday));
    EXPECT_FALSE(calendar.is_business_day(saturday));
}

TEST_F(DateTimeTest, BusinessCalendarWithHolidays) {
    BusinessCalendar calendar;
    auto holiday = DateTime::parse("2024-07-04").value();

    calendar.add_holiday(holiday);
    EXPECT_TRUE(calendar.is_holiday(holiday));
    EXPECT_FALSE(calendar.is_business_day(holiday));
}

TEST_F(DateTimeTest, DateComparisons) {
    auto date1 = DateTime::parse("2024-01-01").value();
    auto date2 = DateTime::parse("2024-01-02").value();
    auto date3 = DateTime::parse("2024-01-01").value();

    EXPECT_LT(date1, date2);
    EXPECT_GT(date2, date1);
    EXPECT_EQ(date1, date3);
    EXPECT_LE(date1, date2);
    EXPECT_GE(date2, date1);
}

TEST_F(DateTimeTest, LeapYearHandling) {
    auto leap_date = DateTime::parse("2024-02-29");
    EXPECT_TRUE(leap_date.is_ok());

    auto non_leap_date = DateTime::parse("2023-02-29");
    EXPECT_TRUE(non_leap_date.is_error());
}

TEST_F(DateTimeTest, MonthEndHandling) {
    auto jan31      = DateTime::parse("2024-01-31").value();
    auto plus_month = jan31.add_months(1);

    // Should handle month-end properly (Jan 31 + 1 month = Feb 29 in leap year)
    EXPECT_EQ(plus_month.to_string(), "2024-02-29");
}