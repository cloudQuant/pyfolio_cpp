#pragma once

#include "error_handling.h"
#include "types.h"
#include <algorithm>
#include <chrono>
#include <set>
#include <string>
#include <vector>

namespace pyfolio {

// Forward declaration
class DateTime;

/**
 * @brief Business calendar for handling trading days and holidays
 */
class BusinessCalendar {
  private:
    std::set<std::chrono::year_month_day> holidays_;
    std::string name_;

  public:
    explicit BusinessCalendar(const std::string& name = "NYSE") : name_(name) { initialize_default_holidays(); }

    /**
     * @brief Check if a date is a business day
     */
    bool is_business_day(const std::chrono::year_month_day& date) const {
        // Check if it's a weekend
        auto time_point = std::chrono::sys_days{date};
        auto weekday    = std::chrono::weekday{time_point};

        if (weekday == std::chrono::Saturday || weekday == std::chrono::Sunday) {
            return false;
        }

        // Check if it's a holiday
        return holidays_.find(date) == holidays_.end();
    }

    /**
     * @brief Get the next business day
     */
    std::chrono::year_month_day next_business_day(const std::chrono::year_month_day& date) const {
        auto current = date;
        do {
            current = std::chrono::year_month_day{std::chrono::sys_days{current} + std::chrono::days{1}};
        } while (!is_business_day(current));

        return current;
    }

    /**
     * @brief Get the previous business day
     */
    std::chrono::year_month_day previous_business_day(const std::chrono::year_month_day& date) const {
        auto current = date;
        do {
            current = std::chrono::year_month_day{std::chrono::sys_days{current} - std::chrono::days{1}};
        } while (!is_business_day(current));

        return current;
    }

    /**
     * @brief Count business days between two dates (exclusive of start, inclusive of end)
     */
    int business_days_between(const std::chrono::year_month_day& start, const std::chrono::year_month_day& end) const {
        if (start >= end)
            return 0;

        int count    = 0;
        auto current = std::chrono::year_month_day{std::chrono::sys_days{start} + std::chrono::days{1}};

        while (current <= end) {
            if (is_business_day(current)) {
                ++count;
            }
            current = std::chrono::year_month_day{std::chrono::sys_days{current} + std::chrono::days{1}};
        }

        return count;
    }

    /**
     * @brief Add holidays to the calendar
     */
    void add_holiday(const std::chrono::year_month_day& date) { holidays_.insert(date); }

    /**
     * @brief Remove holidays from the calendar
     */
    void remove_holiday(const std::chrono::year_month_day& date) { holidays_.erase(date); }

    /**
     * @brief Check if a date is a holiday
     */
    bool is_holiday(const std::chrono::year_month_day& date) const { return holidays_.find(date) != holidays_.end(); }

    // Forward declarations for DateTime overloads (implemented after DateTime class)
    bool is_holiday(const DateTime& date) const;
    bool is_business_day(const DateTime& date) const;
    void add_holiday(const DateTime& date);

  private:
    void initialize_default_holidays() {
        // Add common US holidays (simplified version)
        // In practice, this would be loaded from a comprehensive holiday database

        // New Year's Day
        holidays_.insert(std::chrono::year{2024} / std::chrono::January / 1);
        holidays_.insert(std::chrono::year{2025} / std::chrono::January / 1);

        // Independence Day
        holidays_.insert(std::chrono::year{2024} / std::chrono::July / 4);
        holidays_.insert(std::chrono::year{2025} / std::chrono::July / 4);

        // Christmas Day
        holidays_.insert(std::chrono::year{2024} / std::chrono::December / 25);
        holidays_.insert(std::chrono::year{2025} / std::chrono::December / 25);
    }
};

/**
 * @brief DateTime utilities for financial calculations
 */
class DateTime {
  private:
    TimePoint time_point_;

  public:
    static BusinessCalendar default_calendar_;

  public:
    DateTime() : time_point_(std::chrono::system_clock::now()) {}

    explicit DateTime(const TimePoint& tp) : time_point_(tp) {}

    explicit DateTime(const std::chrono::year_month_day& date) : time_point_(std::chrono::sys_days{date}) {}

    DateTime(int year, int month, int day)
        : time_point_(std::chrono::sys_days{std::chrono::year{year} / std::chrono::month{static_cast<unsigned>(month)} /
                                            std::chrono::day{static_cast<unsigned>(day)}}) {}

    /**
     * @brief Parse datetime from string
     */
    static Result<DateTime> parse(const std::string& date_string, const std::string& format = "%Y-%m-%d") {
        try {
            // Simplified parsing - in practice would use a robust date parsing library
            if (format == "%Y-%m-%d" && date_string.length() == 10) {
                int year  = std::stoi(date_string.substr(0, 4));
                int month = std::stoi(date_string.substr(5, 2));
                int day   = std::stoi(date_string.substr(8, 2));

                auto date = std::chrono::year{year} / std::chrono::month{static_cast<unsigned>(month)} /
                            std::chrono::day{static_cast<unsigned>(day)};

                if (!date.ok()) {
                    return Result<DateTime>::error(ErrorCode::InvalidInput, "Invalid date: " + date_string);
                }

                return Result<DateTime>::success(DateTime{date});
            }

            return Result<DateTime>::error(ErrorCode::ParseError, "Unsupported date format: " + format);

        } catch (const std::exception& e) {
            return Result<DateTime>::error(ErrorCode::ParseError, "Failed to parse date: " + std::string(e.what()));
        }
    }

    /**
     * @brief Get current date/time
     */
    static DateTime now() { return DateTime{std::chrono::system_clock::now()}; }

    /**
     * @brief Create DateTime from TimePoint
     */
    static DateTime from_time_point(const TimePoint& tp) { return DateTime{tp}; }

    /**
     * @brief Convert to date
     */
    std::chrono::year_month_day to_date() const {
        return std::chrono::year_month_day{std::chrono::floor<std::chrono::days>(time_point_)};
    }

    /**
     * @brief Convert to string
     */
    std::string to_string(const std::string& format = "%Y-%m-%d") const {
        auto date = to_date();

        if (format == "%Y-%m-%d") {
            auto month_val = static_cast<unsigned>(date.month());
            auto day_val   = static_cast<unsigned>(date.day());
            return std::to_string(static_cast<int>(date.year())) + "-" + (month_val < 10 ? "0" : "") +
                   std::to_string(month_val) + "-" + (day_val < 10 ? "0" : "") + std::to_string(day_val);
        }

        // For other formats, would use a proper formatting library
        return std::to_string(static_cast<int>(date.year())) + "-" +
               std::to_string(static_cast<unsigned>(date.month())) + "-" +
               std::to_string(static_cast<unsigned>(date.day()));
    }

    /**
     * @brief Check if this is a business day
     */
    bool is_business_day(const BusinessCalendar& calendar = default_calendar_) const {
        return calendar.is_business_day(to_date());
    }

    /**
     * @brief Get next business day
     */
    DateTime next_business_day(const BusinessCalendar& calendar = default_calendar_) const {
        auto next_date = calendar.next_business_day(to_date());
        return DateTime{next_date};
    }

    /**
     * @brief Get previous business day
     */
    DateTime previous_business_day(const BusinessCalendar& calendar = default_calendar_) const {
        auto prev_date = calendar.previous_business_day(to_date());
        return DateTime{prev_date};
    }

    /**
     * @brief Calculate business days between dates
     */
    int business_days_until(const DateTime& other, const BusinessCalendar& calendar = default_calendar_) const {
        return calendar.business_days_between(to_date(), other.to_date());
    }

    /**
     * @brief Add days
     */
    DateTime add_days(int days) const { return DateTime{time_point_ + std::chrono::days{days}}; }

    /**
     * @brief Add months (proper date arithmetic)
     */
    DateTime add_months(int months) const {
        auto date      = to_date();
        auto new_year  = date.year();
        auto new_month = static_cast<int>(static_cast<unsigned>(date.month())) + months;

        // Handle month overflow/underflow
        while (new_month > 12) {
            new_month -= 12;
            new_year = std::chrono::year{static_cast<int>(new_year) + 1};
        }
        while (new_month < 1) {
            new_month += 12;
            new_year = std::chrono::year{static_cast<int>(new_year) - 1};
        }

        auto new_date = new_year / std::chrono::month{static_cast<unsigned>(new_month)} / date.day();

        // Handle invalid dates (e.g., Feb 30 -> Feb 28/29)
        if (!new_date.ok()) {
            // Use last day of month instead
            new_date = new_year / std::chrono::month{static_cast<unsigned>(new_month)} / std::chrono::last;
        }

        return DateTime{new_date};
    }

    /**
     * @brief Add years (proper date arithmetic)
     */
    DateTime add_years(int years) const {
        auto date     = to_date();
        auto new_year = std::chrono::year{static_cast<int>(date.year()) + years};
        auto new_date = new_year / date.month() / date.day();

        // Handle leap year edge case (Feb 29 -> Feb 28 in non-leap year)
        if (!new_date.ok()) {
            new_date = new_year / date.month() / std::chrono::last;
        }

        return DateTime{new_date};
    }

    /**
     * @brief Check if this is a weekday (Monday-Friday)
     */
    bool is_weekday() const {
        auto date       = to_date();
        auto time_point = std::chrono::sys_days{date};
        auto weekday    = std::chrono::weekday{time_point};
        return weekday != std::chrono::Saturday && weekday != std::chrono::Sunday;
    }

    /**
     * @brief Get day of week (0=Sunday, 1=Monday, ..., 6=Saturday)
     */
    int day_of_week() const {
        auto date       = to_date();
        auto time_point = std::chrono::sys_days{date};
        auto weekday    = std::chrono::weekday{time_point};
        return static_cast<int>(weekday.c_encoding());
    }

    /**
     * @brief Calculate days since another date
     */
    int days_since(const DateTime& other) const {
        auto diff = std::chrono::floor<std::chrono::days>(time_point_) -
                    std::chrono::floor<std::chrono::days>(other.time_point_);
        return static_cast<int>(diff.count());
    }

    /**
     * @brief Add business days
     */
    DateTime add_business_days(int days, const BusinessCalendar& calendar = default_calendar_) const {
        auto current  = *this;
        int remaining = std::abs(days);
        int direction = days > 0 ? 1 : -1;

        while (remaining > 0) {
            current = DateTime{current.time_point_ + std::chrono::days{direction}};
            if (current.is_business_day(calendar)) {
                --remaining;
            }
        }

        return current;
    }

    // Comparison operators
    bool operator==(const DateTime& other) const  = default;
    auto operator<=>(const DateTime& other) const = default;

    // Accessors
    const TimePoint& time_point() const { return time_point_; }

    /**
     * @brief Get year
     */
    int year() const { return static_cast<int>(to_date().year()); }

    /**
     * @brief Get month (1-12)
     */
    int month() const { return static_cast<unsigned>(to_date().month()); }

    /**
     * @brief Get day (1-31)
     */
    int day() const { return static_cast<unsigned>(to_date().day()); }
};

// Initialize static member
inline BusinessCalendar DateTime::default_calendar_{"NYSE"};

// BusinessCalendar DateTime overloads (defined after DateTime class)
inline bool BusinessCalendar::is_holiday(const DateTime& date) const {
    return is_holiday(date.to_date());
}

inline bool BusinessCalendar::is_business_day(const DateTime& date) const {
    return is_business_day(date.to_date());
}

inline void BusinessCalendar::add_holiday(const DateTime& date) {
    add_holiday(date.to_date());
}

/**
 * @brief Date range generator
 */
class DateRange {
  private:
    DateTime start_;
    DateTime end_;
    std::chrono::days step_;
    bool business_days_only_;
    BusinessCalendar calendar_;

  public:
    DateRange(const DateTime& start, const DateTime& end, std::chrono::days step = std::chrono::days{1},
              bool business_days_only = false, const BusinessCalendar& calendar = DateTime::default_calendar_)
        : start_(start), end_(end), step_(step), business_days_only_(business_days_only), calendar_(calendar) {}

    /**
     * @brief Generate vector of dates in range
     */
    std::vector<DateTime> to_vector() const {
        std::vector<DateTime> dates;
        auto current = start_;

        while (current <= end_) {
            if (!business_days_only_ || current.is_business_day(calendar_)) {
                dates.push_back(current);
            }
            current = DateTime{current.time_point() + step_};
        }

        return dates;
    }

    /**
     * @brief Count dates in range
     */
    size_t count() const { return to_vector().size(); }
};

/**
 * @brief Frequency conversion utilities
 */
namespace frequency {

/**
 * @brief Convert frequency to days
 */
constexpr int to_days(Frequency freq) {
    switch (freq) {
        case Frequency::Daily:
            return 1;
        case Frequency::Weekly:
            return 7;
        case Frequency::Monthly:
            return 30;  // Approximate
        case Frequency::Quarterly:
            return 91;  // Approximate
        case Frequency::Yearly:
            return 365;  // Approximate
        default:
            return 1;
    }
}

/**
 * @brief Convert frequency to annualization factor
 */
constexpr double to_annual_factor(Frequency freq) {
    switch (freq) {
        case Frequency::Daily:
            return constants::TRADING_DAYS_PER_YEAR;
        case Frequency::Weekly:
            return constants::WEEKS_PER_YEAR;
        case Frequency::Monthly:
            return constants::MONTHS_PER_YEAR;
        case Frequency::Quarterly:
            return 4.0;
        case Frequency::Yearly:
            return 1.0;
        default:
            return constants::TRADING_DAYS_PER_YEAR;
    }
}

}  // namespace frequency

}  // namespace pyfolio