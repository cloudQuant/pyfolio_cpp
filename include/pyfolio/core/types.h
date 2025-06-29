#pragma once

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pyfolio {

// Fundamental types
using Price  = double;
using Return = double;
using Volume = double;
using Shares = double;
using Weight = double;
using Ratio  = double;

// Time types
using TimePoint = std::chrono::system_clock::time_point;
using Duration  = std::chrono::system_clock::duration;
using Date      = std::chrono::year_month_day;

// String types
using Symbol   = std::string;
using Currency = std::string;

// Index types
using Index = std::size_t;
using Count = std::size_t;

// Financial concepts
template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template <typename T>
concept FinancialValue = Numeric<T> && requires(T t) {
    t >= T{0};  // Must support comparison with zero
};

template <typename T>
concept TimeSeriesValue = requires(T t) {
    typename T::value_type;
    t.size();
    t.begin();
    t.end();
};

template <typename T>
concept Portfolio = requires(T t) {
    t.get_returns();
    t.get_positions();
    t.get_transactions();
};

// Constants
namespace constants {
constexpr double TRADING_DAYS_PER_YEAR  = 252.0;
constexpr double BUSINESS_DAYS_PER_YEAR = 260.0;
constexpr double DAYS_PER_YEAR          = 365.25;
constexpr double MONTHS_PER_YEAR        = 12.0;
constexpr double WEEKS_PER_YEAR         = 52.0;

constexpr double DEFAULT_RISK_FREE_RATE      = 0.02;  // 2% annual risk-free rate
constexpr double DEFAULT_CONFIDENCE_LEVEL    = 0.95;
constexpr double DEFAULT_LIQUIDITY_THRESHOLD = 0.2;

constexpr double EPSILON = 1e-10;
constexpr double NaN     = std::numeric_limits<double>::quiet_NaN();
}  // namespace constants

// Enumerations
enum class Frequency { Daily, Weekly, Monthly, Quarterly, Yearly };

enum class ReturnType { Simple, Logarithmic, Excess };

enum class RiskMetric { VaR, CVaR, MaxDrawdown, Volatility };

enum class AttributionMethod { Brinson, BrinsonFachler, Geometric };

// Resampling frequency alias for compatibility
using ResampleFrequency = Frequency;

// Fill method for missing data
enum class FillMethod { Forward, Backward, Interpolate, Drop };

// Strong types for type safety
template <typename T, typename Tag>
class StrongType {
  private:
    T value_;

  public:
    explicit constexpr StrongType(const T& value) : value_(value) {}
    explicit constexpr StrongType(T&& value) : value_(std::move(value)) {}

    constexpr const T& get() const noexcept { return value_; }
    constexpr T& get() noexcept { return value_; }

    constexpr operator const T&() const noexcept { return value_; }

    // Arithmetic operations
    constexpr StrongType operator+(const StrongType& other) const
        requires Numeric<T>
    {
        return StrongType{value_ + other.value_};
    }

    constexpr StrongType operator-(const StrongType& other) const
        requires Numeric<T>
    {
        return StrongType{value_ - other.value_};
    }

    constexpr StrongType operator*(const StrongType& other) const
        requires Numeric<T>
    {
        return StrongType{value_ * other.value_};
    }

    constexpr StrongType operator/(const StrongType& other) const
        requires Numeric<T>
    {
        return StrongType{value_ / other.value_};
    }

    // Comparison operations
    constexpr bool operator==(const StrongType& other) const  = default;
    constexpr auto operator<=>(const StrongType& other) const = default;
};

// Financial strong types
struct PriceTag {};
struct ReturnTag {};
struct VolumeTag {};
struct WeightTag {};

using StrongPrice  = StrongType<double, PriceTag>;
using StrongReturn = StrongType<double, ReturnTag>;
using StrongVolume = StrongType<double, VolumeTag>;
using StrongWeight = StrongType<double, WeightTag>;

// Market data structure (time series of prices and volumes)
struct MarketData {
    std::vector<Price> prices;
    std::vector<Volume> volumes;
    std::vector<TimePoint> timestamps;
    Symbol symbol;
    Currency currency;
};

// OHLCV (Open, High, Low, Close, Volume) data structure
struct OHLCVData {
    Price open;
    Price high;
    Price low;
    Price close;
    Volume volume;
    TimePoint timestamp;
    Symbol symbol;
    Currency currency;
};

// Position data structure
struct Position {
    Symbol symbol;
    Shares shares;
    Price price;
    Weight weight;
    TimePoint timestamp;

    // Constructor for basic initialization
    Position() : shares(0.0), price(0.0), weight(0.0), timestamp{} {}
    Position(const Symbol& sym, Shares sh, Price pr, Weight wt, const TimePoint& ts)
        : symbol(sym), shares(sh), price(pr), weight(wt), timestamp(ts) {}
};

// Transaction side enumeration
enum class TransactionSide { Buy, Sell };

// Transaction data structure
struct Transaction {
    Symbol symbol;
    Shares shares;
    Price price;
    TimePoint timestamp;
    Currency currency;
    double commission    = 0.0;
    TransactionSide side = TransactionSide::Buy;
    // DateTime datetime;  // Additional datetime field for compatibility - use timestamp instead
};

// Forward declarations for time series types
template <typename T>
class TimeSeries;

// Additional type aliases for common collections
using PositionSeries    = std::vector<Position>;
using TransactionSeries = std::vector<Transaction>;

}  // namespace pyfolio