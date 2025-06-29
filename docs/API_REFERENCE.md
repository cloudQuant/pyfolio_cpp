# Pyfolio C++ API Reference

Complete reference for the Pyfolio C++ financial analysis library.

## Quick Navigation

- [Core Components](#core-components)
- [Performance Analysis](#performance-analysis)
- [Risk Management](#risk-management)
- [Portfolio Attribution](#portfolio-attribution)
- [Data Structures](#data-structures)
- [Utilities](#utilities)
- [Examples](#examples)

## Core Components

### TimeSeries<T>

High-performance time series container optimized for financial data.

```cpp
#include <pyfolio/core/time_series.h>

template<typename T>
class TimeSeries {
public:
    // Construction
    TimeSeries();
    TimeSeries(const std::vector<DateTime>& timestamps, 
               const std::vector<T>& values,
               const std::string& name = "");
    
    // Factory methods (preferred)
    static Result<TimeSeries<T>> create(
        const std::vector<DateTime>& timestamps,
        const std::vector<T>& values,
        const std::string& name = "");
    
    // Capacity
    constexpr size_type size() const noexcept;
    constexpr bool empty() const noexcept;
    void reserve(size_type n);
    
    // Element access
    constexpr const T& operator[](size_type index) const noexcept;
    const T& at(size_type index) const;
    constexpr const DateTime& timestamp(size_type index) const;
    
    // Modifiers
    void push_back(const DateTime& timestamp, const T& value);
    [[nodiscard]] Result<void> try_push_back(const DateTime& timestamp, const T& value);
    
    // Operations
    [[nodiscard]] Result<TimeSeries<T>> slice(const DateTime& start, const DateTime& end) const;
    [[nodiscard]] Result<TimeSeries<T>> resample(Frequency target_freq) const;
    
    // Rolling operations (O(n) complexity)
    [[nodiscard]] Result<TimeSeries<T>> rolling_mean(size_type window) const;
    [[nodiscard]] Result<TimeSeries<T>> rolling_std(size_type window) const;
    [[nodiscard]] Result<TimeSeries<T>> rolling_min(size_type window) const;
    [[nodiscard]] Result<TimeSeries<T>> rolling_max(size_type window) const;
    
    // Statistical operations (SIMD-optimized)
    [[nodiscard]] Result<T> mean() const;
    [[nodiscard]] Result<T> std() const;
    [[nodiscard]] Result<double> correlation(const TimeSeries<T>& other) const;
    
    // Financial operations
    [[nodiscard]] Result<TimeSeries<Return>> returns() const;
    [[nodiscard]] Result<TimeSeries<T>> cumulative_returns() const;
    
    // Alignment and missing data
    [[nodiscard]] Result<std::pair<TimeSeries<T>, TimeSeries<T>>> 
        align(const TimeSeries<T>& other) const;
    [[nodiscard]] Result<TimeSeries<T>> 
        fill_missing(const std::vector<DateTime>& target_dates, FillMethod method) const;
    
    // SIMD arithmetic operations
    [[nodiscard]] Result<TimeSeries<T>> operator+(const TimeSeries<T>& other) const;
    [[nodiscard]] Result<TimeSeries<T>> operator-(const TimeSeries<T>& other) const;
    [[nodiscard]] Result<TimeSeries<T>> operator*(const TimeSeries<T>& other) const;
    [[nodiscard]] Result<TimeSeries<T>> operator*(T scalar) const;
    [[nodiscard]] Result<T> dot(const TimeSeries<T>& other) const;
};
```

**Performance Notes:**
- All operations are SIMD-optimized for `double` type
- Rolling operations use O(n) algorithms with efficient sliding windows
- Memory layout optimized for cache efficiency

### Result<T> Error Handling

Robust error handling without exceptions.

```cpp
#include <pyfolio/core/error_handling.h>

template<typename T>
class Result {
public:
    // Check state
    constexpr bool is_ok() const noexcept;
    constexpr bool is_error() const noexcept;
    
    // Access value (only when is_ok())
    constexpr const T& value() const &;
    constexpr T& value() &;
    constexpr T&& value() &&;
    
    // Access error (only when is_error())
    constexpr const Error& error() const;
    
    // Factory methods
    static Result<T> success(T&& value);
    static Result<T> error(ErrorCode code, const std::string& message);
    
    // Monadic operations
    template<typename F>
    auto map(F&& func) const -> Result<decltype(func(value()))>;
    
    template<typename F>
    auto and_then(F&& func) const -> decltype(func(value()));
    
    T value_or(const T& default_value) const;
};

// Common usage pattern
auto result = risky_operation();
if (result.is_error()) {
    std::cerr << "Error: " << result.error().message << std::endl;
    return;
}
auto value = result.value();
```

### DateTime Handling

Financial calendar-aware date/time operations.

```cpp
#include <pyfolio/core/datetime.h>

class DateTime {
public:
    // Construction
    DateTime();
    explicit DateTime(std::chrono::system_clock::time_point tp);
    DateTime(int year, int month, int day);
    
    // Parsing
    static Result<DateTime> parse(const std::string& date_str);
    static Result<DateTime> parse(const std::string& date_str, const std::string& format);
    
    // Conversion
    std::string to_string() const;
    std::chrono::system_clock::time_point to_time_point() const;
    std::chrono::year_month_day to_date() const;
    
    // Business day operations
    DateTime next_business_day() const;
    DateTime previous_business_day() const;
    bool is_business_day() const;
    
    // Calendar operations
    DateTime add_days(int days) const;
    DateTime add_months(int months) const;
    DateTime add_years(int years) const;
    
    // Comparison
    auto operator<=>(const DateTime& other) const = default;
};
```

## Performance Analysis

### Basic Metrics

```cpp
#include <pyfolio/performance/ratios.h>

namespace pyfolio::performance {
    // Risk-adjusted returns
    [[nodiscard]] Result<double> sharpe_ratio(
        const TimeSeries<Return>& returns, 
        double risk_free_rate = 0.02);
    
    [[nodiscard]] Result<double> sortino_ratio(
        const TimeSeries<Return>& returns,
        double target_return = 0.0);
    
    [[nodiscard]] Result<double> calmar_ratio(
        const TimeSeries<Return>& returns);
    
    [[nodiscard]] Result<double> information_ratio(
        const TimeSeries<Return>& portfolio_returns,
        const TimeSeries<Return>& benchmark_returns);
    
    // Basic metrics
    [[nodiscard]] Result<double> total_return(
        const TimeSeries<Return>& returns);
    
    [[nodiscard]] Result<double> annual_return(
        const TimeSeries<Return>& returns,
        int periods_per_year = 252);
    
    [[nodiscard]] Result<double> volatility(
        const TimeSeries<Return>& returns,
        int periods_per_year = 252);
    
    // Advanced metrics
    [[nodiscard]] Result<double> omega_ratio(
        const TimeSeries<Return>& returns,
        double threshold = 0.0);
    
    [[nodiscard]] Result<double> tail_ratio(
        const TimeSeries<Return>& returns);
}
```

### Drawdown Analysis

```cpp
#include <pyfolio/performance/drawdown.h>

namespace pyfolio::performance {
    struct DrawdownInfo {
        double max_drawdown;          ///< Maximum drawdown value
        DateTime peak_date;           ///< Date of peak before drawdown
        DateTime trough_date;         ///< Date of maximum drawdown
        DateTime recovery_date;       ///< Date of recovery (if any)
        int duration_days;            ///< Duration in days
        bool recovered;               ///< Whether drawdown recovered
    };
    
    [[nodiscard]] Result<double> max_drawdown(
        const TimeSeries<Return>& returns);
    
    [[nodiscard]] Result<TimeSeries<double>> drawdown_series(
        const TimeSeries<Return>& returns);
    
    [[nodiscard]] Result<std::vector<DrawdownInfo>> drawdown_periods(
        const TimeSeries<Return>& returns,
        double min_drawdown = 0.01);
    
    [[nodiscard]] Result<DrawdownInfo> worst_drawdown(
        const TimeSeries<Return>& returns);
}
```

### Comprehensive Analysis

```cpp
#include <pyfolio/analytics/performance_metrics.h>

namespace pyfolio::analytics {
    struct PerformanceMetrics {
        double total_return;        ///< Cumulative return
        double annual_return;       ///< Annualized return
        double annual_volatility;   ///< Annualized volatility
        double sharpe_ratio;        ///< Sharpe ratio
        double sortino_ratio;       ///< Sortino ratio
        double max_drawdown;        ///< Maximum drawdown
        double calmar_ratio;        ///< Calmar ratio
        double skewness;            ///< Return skewness
        double kurtosis;            ///< Return kurtosis
        double var_95;              ///< 95% Value at Risk
        double var_99;              ///< 99% Value at Risk
        double beta;                ///< Beta vs benchmark
        double alpha;               ///< Alpha vs benchmark
        double information_ratio;   ///< Information ratio
        // ... additional metrics
    };
    
    [[nodiscard]] Result<PerformanceMetrics> calculate_performance_metrics(
        const TimeSeries<Return>& returns,
        const std::optional<TimeSeries<Return>>& benchmark = std::nullopt,
        double risk_free_rate = 0.02,
        int periods_per_year = 252);
    
    [[nodiscard]] Result<TimeSeries<PerformanceMetrics>> 
        calculate_rolling_performance_metrics(
            const TimeSeries<Return>& returns,
            size_t window = 252,
            const std::optional<TimeSeries<Return>>& benchmark = std::nullopt,
            double risk_free_rate = 0.02);
}
```

## Risk Management

### Value at Risk

```cpp
#include <pyfolio/risk/var.h>

namespace pyfolio::risk {
    enum class VaRMethod {
        Historical,     ///< Historical simulation
        Parametric,     ///< Normal distribution assumption
        MonteCarlo      ///< Monte Carlo simulation
    };
    
    [[nodiscard]] Result<double> value_at_risk(
        const TimeSeries<Return>& returns,
        double confidence_level = 0.95,
        VaRMethod method = VaRMethod::Historical);
    
    [[nodiscard]] Result<double> conditional_var(
        const TimeSeries<Return>& returns,
        double confidence_level = 0.95,
        VaRMethod method = VaRMethod::Historical);
    
    [[nodiscard]] Result<TimeSeries<double>> rolling_var(
        const TimeSeries<Return>& returns,
        size_t window = 252,
        double confidence_level = 0.95);
    
    // Stress testing
    [[nodiscard]] Result<std::map<std::string, double>> stress_test(
        const TimeSeries<Return>& returns,
        const std::vector<std::pair<std::string, TimeSeries<Return>>>& scenarios);
}
```

### Factor Exposure

```cpp
#include <pyfolio/risk/factor_exposure.h>

namespace pyfolio::risk {
    struct FactorExposure {
        std::map<std::string, double> exposures;
        double r_squared;
        double tracking_error;
        TimeSeries<double> residuals;
    };
    
    [[nodiscard]] Result<FactorExposure> calculate_factor_exposure(
        const TimeSeries<Return>& portfolio_returns,
        const std::map<std::string, TimeSeries<Return>>& factor_returns);
    
    [[nodiscard]] Result<double> beta(
        const TimeSeries<Return>& portfolio_returns,
        const TimeSeries<Return>& market_returns);
    
    [[nodiscard]] Result<double> alpha(
        const TimeSeries<Return>& portfolio_returns,
        const TimeSeries<Return>& market_returns,
        double risk_free_rate = 0.02);
}
```

## Portfolio Attribution

### Brinson Attribution

```cpp
#include <pyfolio/attribution/brinson.h>

namespace pyfolio::attribution {
    struct AttributionResult {
        double allocation_effect;   ///< Asset allocation effect
        double selection_effect;    ///< Security selection effect
        double interaction_effect;  ///< Interaction effect
        double total_active_return; ///< Total active return
    };
    
    [[nodiscard]] Result<AttributionResult> brinson_attribution(
        const TimeSeries<Return>& portfolio_returns,
        const TimeSeries<Return>& benchmark_returns,
        const TimeSeries<std::map<std::string, double>>& portfolio_weights,
        const TimeSeries<std::map<std::string, double>>& benchmark_weights,
        const TimeSeries<std::map<std::string, Return>>& sector_returns);
    
    // Multi-period attribution
    [[nodiscard]] Result<TimeSeries<AttributionResult>> rolling_attribution(
        const TimeSeries<Return>& portfolio_returns,
        const TimeSeries<Return>& benchmark_returns,
        const PositionSeries& positions,
        size_t window = 252);
}
```

## Data Structures

### Core Types

```cpp
#include <pyfolio/core/types.h>

// Financial value types
using Price = double;
using Return = double;
using Volume = double;
using Shares = double;
using Weight = double;

// Time types
using TimePoint = std::chrono::system_clock::time_point;
using Duration = std::chrono::system_clock::duration;

// Position data
struct Position {
    Symbol symbol;
    Shares shares;
    Price price;
    Weight weight;
    TimePoint timestamp;
};

// Transaction data
struct Transaction {
    Symbol symbol;
    Shares shares;
    Price price;
    TimePoint timestamp;
    Currency currency;
    double commission = 0.0;
    TransactionSide side = TransactionSide::Buy;
};

// Enumerations
enum class Frequency { Daily, Weekly, Monthly, Quarterly, Yearly };
enum class FillMethod { Forward, Backward, Interpolate, Drop };
enum class TransactionSide { Buy, Sell };
```

### Strong Types

```cpp
// Type-safe financial values
template<typename T, typename Tag>
class StrongType {
public:
    explicit constexpr StrongType(const T& value);
    constexpr const T& get() const noexcept;
    constexpr operator const T&() const noexcept;
    
    // Arithmetic operations
    constexpr StrongType operator+(const StrongType& other) const;
    constexpr StrongType operator-(const StrongType& other) const;
    constexpr StrongType operator*(const StrongType& other) const;
    constexpr StrongType operator/(const StrongType& other) const;
};

using StrongPrice = StrongType<double, PriceTag>;
using StrongReturn = StrongType<double, ReturnTag>;
using StrongVolume = StrongType<double, VolumeTag>;
```

## Utilities

### Data Loading

```cpp
#include <pyfolio/io/data_loader.h>

namespace pyfolio::io {
    [[nodiscard]] Result<TimeSeries<Return>> load_returns_csv(
        const std::string& filename,
        const std::string& date_column = "Date",
        const std::string& return_column = "Return");
    
    [[nodiscard]] Result<std::vector<Position>> load_positions_csv(
        const std::string& filename);
    
    [[nodiscard]] Result<std::vector<Transaction>> load_transactions_csv(
        const std::string& filename);
    
    // Save functions
    Result<void> save_returns_csv(
        const TimeSeries<Return>& returns,
        const std::string& filename);
    
    Result<void> save_metrics_json(
        const analytics::PerformanceMetrics& metrics,
        const std::string& filename);
}
```

### Validation

```cpp
#include <pyfolio/utils/validation.h>

namespace pyfolio::validation {
    [[nodiscard]] Result<void> validate_returns(
        const TimeSeries<Return>& returns);
    
    [[nodiscard]] Result<void> validate_positions(
        const std::vector<Position>& positions);
    
    [[nodiscard]] Result<void> validate_transactions(
        const std::vector<Transaction>& transactions);
    
    // Check for data quality issues
    struct DataQualityReport {
        int missing_values;
        int outliers;
        int duplicate_dates;
        std::vector<std::string> warnings;
    };
    
    [[nodiscard]] Result<DataQualityReport> data_quality_check(
        const TimeSeries<Return>& returns,
        double outlier_threshold = 3.0);
}
```

### Parallel Processing

```cpp
#include <pyfolio/core/parallel_algorithms.h>

namespace pyfolio::parallel {
    // Parallel metric calculation
    [[nodiscard]] Result<analytics::PerformanceMetrics> calculate_metrics(
        const TimeSeries<Return>& returns,
        size_t num_threads = std::thread::hardware_concurrency());
    
    // Parallel rolling operations
    template<typename F>
    [[nodiscard]] Result<TimeSeries<double>> parallel_rolling(
        const TimeSeries<Return>& returns,
        size_t window,
        F&& func,
        size_t num_threads = std::thread::hardware_concurrency());
    
    // Monte Carlo simulations
    [[nodiscard]] Result<std::vector<double>> monte_carlo_var(
        const TimeSeries<Return>& returns,
        double confidence_level,
        size_t num_simulations = 10000,
        size_t num_threads = std::thread::hardware_concurrency());
}
```

## Examples

### Complete Portfolio Analysis

```cpp
#include <pyfolio/pyfolio.h>

void complete_analysis_example() {
    using namespace pyfolio;
    
    // Load data
    auto returns = io::load_returns_csv("portfolio.csv").value();
    auto benchmark = io::load_returns_csv("sp500.csv").value();
    auto positions = io::load_positions_csv("positions.csv").value();
    
    // Calculate comprehensive metrics
    auto metrics = analytics::calculate_performance_metrics(
        returns, benchmark, 0.025, 252).value();
    
    std::cout << "=== Portfolio Analysis ===" << std::endl;
    std::cout << "Annual Return: " << metrics.annual_return * 100 << "%" << std::endl;
    std::cout << "Volatility: " << metrics.annual_volatility * 100 << "%" << std::endl;
    std::cout << "Sharpe Ratio: " << metrics.sharpe_ratio << std::endl;
    std::cout << "Max Drawdown: " << metrics.max_drawdown * 100 << "%" << std::endl;
    std::cout << "Beta: " << metrics.beta << std::endl;
    std::cout << "Alpha: " << metrics.alpha * 100 << "%" << std::endl;
    
    // Risk analysis
    auto var_95 = risk::value_at_risk(returns, 0.95).value();
    auto cvar_95 = risk::conditional_var(returns, 0.95).value();
    
    std::cout << "\n=== Risk Metrics ===" << std::endl;
    std::cout << "VaR (95%): " << var_95 * 100 << "%" << std::endl;
    std::cout << "CVaR (95%): " << cvar_95 * 100 << "%" << std::endl;
    
    // Save results
    io::save_metrics_json(metrics, "analysis_results.json");
}
```

### High-Frequency Analysis

```cpp
void high_frequency_example() {
    using namespace pyfolio;
    
    // Pre-allocate for performance
    TimeSeries<Return> returns;
    returns.reserve(100000);
    
    // Use memory pool for transactions
    memory::PoolAllocator<Transaction> tx_allocator(1000000);
    
    // SIMD-optimized operations
    auto correlation_matrix = simd::calculate_correlation_matrix(returns_matrix);
    
    // Parallel processing
    auto rolling_metrics = parallel::parallel_rolling(
        returns, 252, [](auto window) {
            return calculate_sharpe_ratio(window);
        }, 8);  // 8 threads
}
```

### Real-time Monitoring

```cpp
#include <pyfolio/rest/api_server.h>

void real_time_example() {
    using namespace pyfolio;
    
    // Create REST API server
    rest::APIServer server(8080);
    
    // Add endpoints
    server.add_endpoint("/metrics", [](const auto& returns_data) {
        auto returns = parse_returns_json(returns_data);
        auto metrics = analytics::calculate_performance_metrics(returns);
        return metrics_to_json(metrics.value());
    });
    
    server.add_endpoint("/risk", [](const auto& returns_data) {
        auto returns = parse_returns_json(returns_data);
        auto var = risk::value_at_risk(returns, 0.95);
        return risk_to_json(var.value());
    });
    
    // Start server
    server.start();
    std::cout << "API server running on http://localhost:8080" << std::endl;
}
```

## Error Handling Best Practices

```cpp
// Always check Result<T> return values
auto result = risky_operation();
if (result.is_error()) {
    std::cerr << "Error [" << static_cast<int>(result.error().code) 
              << "]: " << result.error().message << std::endl;
    return;
}

// Use monadic operations for chaining
auto final_result = load_data("file.csv")
    .and_then([](const auto& data) { return validate_data(data); })
    .and_then([](const auto& data) { return calculate_metrics(data); })
    .map([](const auto& metrics) { return format_output(metrics); });

// Provide default values when appropriate
auto sharpe = calculate_sharpe_ratio(returns).value_or(0.0);
```

This API reference provides comprehensive coverage of all major components in Pyfolio C++. For detailed implementation examples, see the `examples/` directory and the generated Doxygen documentation.