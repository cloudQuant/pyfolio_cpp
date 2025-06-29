#pragma once

#include "../core/error_handling.h"
#include "../core/types.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <span>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace pyfolio::stats {

// Compile-time mathematical constants
namespace constants {
constexpr double pi      = M_PI;
constexpr double e       = 2.71828182845904523536;
constexpr double sqrt_2  = 1.41421356237309504880;
constexpr double sqrt_pi = 1.77245385090551602730;
constexpr double ln_2    = 0.69314718055994530942;
constexpr double ln_10   = 2.30258509299404568402;

// Finance-specific constants
constexpr double trading_days_per_year  = 252.0;
constexpr double calendar_days_per_year = 365.25;
constexpr double months_per_year        = 12.0;
constexpr double weeks_per_year         = 52.0;

// Risk-free rate assumptions (can be overridden)
constexpr double default_risk_free_rate = 0.02;  // 2% annual

// Common percentiles for risk metrics
constexpr double var_95_percentile  = 0.05;
constexpr double var_99_percentile  = 0.01;
constexpr double cvar_95_percentile = 0.05;
constexpr double cvar_99_percentile = 0.01;
}  // namespace constants

// Compile-time utility functions
namespace constexpr_utils {
/**
 * @brief Compile-time power function for integer exponents
 */
constexpr double power(double base, int exp) noexcept {
    if (exp == 0)
        return 1.0;
    if (exp == 1)
        return base;
    if (exp < 0)
        return 1.0 / power(base, -exp);

    double result = 1.0;
    while (exp > 0) {
        if (exp & 1) {
            result *= base;
        }
        base *= base;
        exp >>= 1;
    }
    return result;
}

/**
 * @brief Compile-time absolute value
 */
constexpr double abs(double x) noexcept {
    return x < 0.0 ? -x : x;
}

/**
 * @brief Compile-time max
 */
constexpr double max(double a, double b) noexcept {
    return a > b ? a : b;
}

/**
 * @brief Compile-time min
 */
constexpr double min(double a, double b) noexcept {
    return a < b ? a : b;
}

/**
 * @brief Annualization factor for given frequency
 */
constexpr double annualization_factor(Frequency freq) noexcept {
    switch (freq) {
        case Frequency::Daily:
            return constants::trading_days_per_year;
        case Frequency::Weekly:
            return constants::weeks_per_year;
        case Frequency::Monthly:
            return constants::months_per_year;
        case Frequency::Quarterly:
            return 4.0;
        case Frequency::Yearly:
            return 1.0;
        default:
            return constants::trading_days_per_year;
    }
}
}  // namespace constexpr_utils

/**
 * @brief Calculate mean of a data series
 */
template <Numeric T>
Result<double> mean(std::span<const T> data) {
    if (data.empty()) {
        return Result<double>::error(ErrorCode::InsufficientData, "Cannot calculate mean of empty data");
    }

    double sum = std::accumulate(data.begin(), data.end(), 0.0);
    return Result<double>::success(sum / data.size());
}

/**
 * @brief Calculate variance of a data series
 */
template <Numeric T>
Result<double> variance(std::span<const T> data, bool sample = true) {
    if (data.size() < (sample ? 2 : 1)) {
        return Result<double>::error(ErrorCode::InsufficientData, "Insufficient data for variance calculation");
    }

    auto mean_result = mean(data);
    if (mean_result.is_error()) {
        return mean_result;
    }

    double data_mean   = mean_result.value();
    double sum_sq_diff = 0.0;

    for (const auto& value : data) {
        double diff = static_cast<double>(value) - data_mean;
        sum_sq_diff += diff * diff;
    }

    size_t denominator = sample ? data.size() - 1 : data.size();
    return Result<double>::success(sum_sq_diff / denominator);
}

/**
 * @brief Calculate standard deviation
 */
template <Numeric T>
Result<double> standard_deviation(std::span<const T> data, bool sample = true) {
    auto var_result = variance(data, sample);
    if (var_result.is_error()) {
        return var_result;
    }

    return Result<double>::success(std::sqrt(var_result.value()));
}

/**
 * @brief Calculate skewness
 */
template <Numeric T>
Result<double> skewness(std::span<const T> data) {
    if (data.size() < 3) {
        return Result<double>::error(ErrorCode::InsufficientData,
                                     "Need at least 3 data points for skewness calculation");
    }

    auto mean_result = mean(data);
    auto std_result  = standard_deviation(data);

    if (mean_result.is_error())
        return mean_result;
    if (std_result.is_error())
        return std_result;

    double data_mean = mean_result.value();
    double data_std  = std_result.value();

    if (data_std == 0.0) {
        return Result<double>::error(ErrorCode::DivisionByZero,
                                     "Cannot calculate skewness with zero standard deviation");
    }

    double sum_cubed = 0.0;
    for (const auto& value : data) {
        double standardized = (static_cast<double>(value) - data_mean) / data_std;
        sum_cubed += standardized * standardized * standardized;
    }

    return Result<double>::success(sum_cubed / data.size());
}

/**
 * @brief Calculate kurtosis
 */
template <Numeric T>
Result<double> kurtosis(std::span<const T> data, bool excess = true) {
    if (data.size() < 4) {
        return Result<double>::error(ErrorCode::InsufficientData,
                                     "Need at least 4 data points for kurtosis calculation");
    }

    auto mean_result = mean(data);
    auto std_result  = standard_deviation(data);

    if (mean_result.is_error())
        return mean_result;
    if (std_result.is_error())
        return std_result;

    double data_mean = mean_result.value();
    double data_std  = std_result.value();

    if (data_std == 0.0) {
        return Result<double>::error(ErrorCode::DivisionByZero,
                                     "Cannot calculate kurtosis with zero standard deviation");
    }

    double sum_fourth = 0.0;
    for (const auto& value : data) {
        double standardized = (static_cast<double>(value) - data_mean) / data_std;
        double fourth_power = standardized * standardized * standardized * standardized;
        sum_fourth += fourth_power;
    }

    double kurt = sum_fourth / data.size();
    return Result<double>::success(excess ? kurt - 3.0 : kurt);
}

/**
 * @brief Calculate correlation coefficient between two series
 */
template <Numeric T1, Numeric T2>
Result<double> correlation(std::span<const T1> x, std::span<const T2> y) {
    if (x.size() != y.size()) {
        return Result<double>::error(ErrorCode::InvalidInput,
                                     "Data series must have the same length for correlation calculation");
    }

    if (x.size() < 2) {
        return Result<double>::error(ErrorCode::InsufficientData,
                                     "Need at least 2 data points for correlation calculation");
    }

    auto x_mean_result = mean(x);
    auto y_mean_result = mean(y);

    if (x_mean_result.is_error())
        return x_mean_result;
    if (y_mean_result.is_error())
        return y_mean_result;

    double x_mean = x_mean_result.value();
    double y_mean = y_mean_result.value();

    double numerator = 0.0;
    double x_sum_sq  = 0.0;
    double y_sum_sq  = 0.0;

    for (size_t i = 0; i < x.size(); ++i) {
        double x_diff = static_cast<double>(x[i]) - x_mean;
        double y_diff = static_cast<double>(y[i]) - y_mean;

        numerator += x_diff * y_diff;
        x_sum_sq += x_diff * x_diff;
        y_sum_sq += y_diff * y_diff;
    }

    double denominator = std::sqrt(x_sum_sq * y_sum_sq);

    if (denominator == 0.0) {
        return Result<double>::error(ErrorCode::DivisionByZero, "Cannot calculate correlation with zero variance");
    }

    return Result<double>::success(numerator / denominator);
}

/**
 * @brief Calculate covariance between two series
 */
template <Numeric T1, Numeric T2>
Result<double> covariance(std::span<const T1> x, std::span<const T2> y, bool sample = true) {
    if (x.size() != y.size()) {
        return Result<double>::error(ErrorCode::InvalidInput,
                                     "Data series must have the same length for covariance calculation");
    }

    if (x.size() < (sample ? 2 : 1)) {
        return Result<double>::error(ErrorCode::InsufficientData, "Insufficient data for covariance calculation");
    }

    auto x_mean_result = mean(x);
    auto y_mean_result = mean(y);

    if (x_mean_result.is_error())
        return x_mean_result;
    if (y_mean_result.is_error())
        return y_mean_result;

    double x_mean = x_mean_result.value();
    double y_mean = y_mean_result.value();

    double sum_products = 0.0;
    for (size_t i = 0; i < x.size(); ++i) {
        double x_diff = static_cast<double>(x[i]) - x_mean;
        double y_diff = static_cast<double>(y[i]) - y_mean;
        sum_products += x_diff * y_diff;
    }

    size_t denominator = sample ? x.size() - 1 : x.size();
    return Result<double>::success(sum_products / denominator);
}

/**
 * @brief Calculate percentile of a data series
 */
template <Numeric T>
Result<double> percentile(std::span<const T> data, double p) {
    if (data.empty()) {
        return Result<double>::error(ErrorCode::InsufficientData, "Cannot calculate percentile of empty data");
    }

    if (p < 0.0 || p > 100.0) {
        return Result<double>::error(ErrorCode::InvalidInput, "Percentile must be between 0 and 100");
    }

    std::vector<T> sorted_data(data.begin(), data.end());
    std::sort(sorted_data.begin(), sorted_data.end());

    if (p == 0.0)
        return Result<double>::success(static_cast<double>(sorted_data.front()));
    if (p == 100.0)
        return Result<double>::success(static_cast<double>(sorted_data.back()));

    double index       = (p / 100.0) * (sorted_data.size() - 1);
    size_t lower_index = static_cast<size_t>(std::floor(index));
    size_t upper_index = static_cast<size_t>(std::ceil(index));

    if (lower_index == upper_index) {
        return Result<double>::success(static_cast<double>(sorted_data[lower_index]));
    }

    double weight       = index - lower_index;
    double interpolated = static_cast<double>(sorted_data[lower_index]) * (1.0 - weight) +
                          static_cast<double>(sorted_data[upper_index]) * weight;

    return Result<double>::success(interpolated);
}

/**
 * @brief Calculate median
 */
template <Numeric T>
Result<double> median(std::span<const T> data) {
    return percentile(data, 50.0);
}

/**
 * @brief Calculate quantile
 */
template <Numeric T>
Result<double> quantile(std::span<const T> data, double q) {
    if (q < 0.0 || q > 1.0) {
        return Result<double>::error(ErrorCode::InvalidInput, "Quantile must be between 0 and 1");
    }

    return percentile(data, q * 100.0);
}

/**
 * @brief Calculate Value at Risk (VaR)
 */
template <Numeric T>
Result<double> value_at_risk(std::span<const T> returns, double confidence_level = 0.95) {
    if (confidence_level <= 0.0 || confidence_level >= 1.0) {
        return Result<double>::error(ErrorCode::InvalidInput, "Confidence level must be between 0 and 1 (exclusive)");
    }

    double percentile_level = (1.0 - confidence_level) * 100.0;
    auto var_result         = percentile(returns, percentile_level);

    if (var_result.is_error()) {
        return var_result;
    }

    // VaR should be negative for losses (negative returns)
    // Since we're calculating the percentile of negative returns, return as-is
    return Result<double>::success(var_result.value());
}

/**
 * @brief Calculate Conditional Value at Risk (CVaR) / Expected Shortfall
 */
template <Numeric T>
Result<double> conditional_value_at_risk(std::span<const T> returns, double confidence_level = 0.95) {
    auto var_result = value_at_risk(returns, confidence_level);
    if (var_result.is_error()) {
        return var_result;
    }

    double var_threshold = var_result.value();  // Use VaR threshold directly

    // Calculate mean of returns below VaR threshold
    std::vector<double> tail_returns;
    for (const auto& ret : returns) {
        if (static_cast<double>(ret) <= var_threshold) {
            tail_returns.push_back(static_cast<double>(ret));
        }
    }

    if (tail_returns.empty()) {
        return Result<double>::error(ErrorCode::InsufficientData, "No returns below VaR threshold found");
    }

    auto tail_mean_result = mean(std::span<const double>{tail_returns});
    if (tail_mean_result.is_error()) {
        return tail_mean_result;
    }

    // CVaR should be negative for losses (same as VaR convention)
    return Result<double>::success(tail_mean_result.value());
}

/**
 * @brief Calculate rolling statistics
 */
template <typename T, typename F>
Result<std::vector<double>> rolling_statistic(std::span<const T> data, size_t window_size, F&& func) {
    if (window_size == 0 || window_size > data.size()) {
        return Result<std::vector<double>>::error(ErrorCode::InvalidInput,
                                                  "Invalid window size for rolling calculation");
    }

    std::vector<double> results;
    results.reserve(data.size() - window_size + 1);

    for (size_t i = window_size - 1; i < data.size(); ++i) {
        std::span<const T> window{data.data() + i - window_size + 1, window_size};
        auto result = func(window);

        if (result.is_error()) {
            return Result<std::vector<double>>::error(result.error().code,
                                                      "Rolling calculation failed: " + result.error().message);
        }

        results.push_back(result.value());
    }

    return Result<std::vector<double>>::success(std::move(results));
}

/**
 * @brief Calculate rolling mean
 */
template <Numeric T>
Result<std::vector<double>> rolling_mean(std::span<const T> data, size_t window_size) {
    return rolling_statistic(data, window_size, [](std::span<const T> window) { return mean(window); });
}

/**
 * @brief Calculate rolling standard deviation
 */
template <Numeric T>
Result<std::vector<double>> rolling_std(std::span<const T> data, size_t window_size) {
    return rolling_statistic(data, window_size, [](std::span<const T> window) { return standard_deviation(window); });
}

/**
 * @brief Calculate rolling correlation
 */
template <Numeric T1, Numeric T2>
Result<std::vector<double>> rolling_correlation(std::span<const T1> x, std::span<const T2> y, size_t window_size) {
    if (x.size() != y.size()) {
        return Result<std::vector<double>>::error(ErrorCode::InvalidInput, "Data series must have the same length");
    }

    if (window_size == 0 || window_size > x.size()) {
        return Result<std::vector<double>>::error(ErrorCode::InvalidInput,
                                                  "Invalid window size for rolling correlation");
    }

    std::vector<double> results;
    results.reserve(x.size() - window_size + 1);

    for (size_t i = window_size - 1; i < x.size(); ++i) {
        std::span<const T1> x_window{x.data() + i - window_size + 1, window_size};
        std::span<const T2> y_window{y.data() + i - window_size + 1, window_size};

        auto corr_result = correlation(x_window, y_window);
        if (corr_result.is_error()) {
            return Result<std::vector<double>>::error(
                corr_result.error().code, "Rolling correlation calculation failed: " + corr_result.error().message);
        }

        results.push_back(corr_result.value());
    }

    return Result<std::vector<double>>::success(std::move(results));
}

/**
 * @brief Standard normal cumulative distribution function (CDF)
 */
inline double normal_cdf(double x) {
    return 0.5 * (1.0 + std::erf(x / std::sqrt(2.0)));
}

/**
 * @brief Standard normal probability density function (PDF)
 */
inline double normal_pdf(double x) {
    static const double inv_sqrt_2pi = 1.0 / std::sqrt(2.0 * M_PI);
    return inv_sqrt_2pi * std::exp(-0.5 * x * x);
}

/**
 * @brief Standard normal percent point function (inverse CDF)
 * Using Beasley-Springer-Moro algorithm
 */
inline double normal_ppf(double p) {
    if (p <= 0.0 || p >= 1.0) {
        if (p == 0.0)
            return -std::numeric_limits<double>::infinity();
        if (p == 1.0)
            return std::numeric_limits<double>::infinity();
        return std::numeric_limits<double>::quiet_NaN();
    }

    // Coefficients for Beasley-Springer-Moro algorithm
    static const double a[] = {-3.969683028665376e+01, 2.209460984245205e+02,  -2.759285104469687e+02,
                               1.383577518672690e+02,  -3.066479806614716e+01, 2.506628277459239e+00};

    static const double b[] = {-5.447609879822406e+01, 1.615858368580409e+02, -1.556989798598866e+02,
                               6.680131188771972e+01, -1.328068155288572e+01};

    static const double c[] = {-7.784894002430293e-03, -3.223964580411365e-01, -2.400758277161838e+00,
                               -2.549732539343734e+00, 4.374664141464968e+00,  2.938163982698783e+00};

    static const double d[] = {7.784695709041462e-03, 3.224671290700398e-01, 2.445134137142996e+00,
                               3.754408661907416e+00};

    double x, r;

    if (p < 0.02425) {
        // Tail region
        x = std::sqrt(-2.0 * std::log(p));
        x = (((((c[0] * x + c[1]) * x + c[2]) * x + c[3]) * x + c[4]) * x + c[5]) /
            ((((d[0] * x + d[1]) * x + d[2]) * x + d[3]) * x + 1.0);
    } else if (p > 0.97575) {
        // Upper tail region
        x = std::sqrt(-2.0 * std::log(1.0 - p));
        x = -(((((c[0] * x + c[1]) * x + c[2]) * x + c[3]) * x + c[4]) * x + c[5]) /
            ((((d[0] * x + d[1]) * x + d[2]) * x + d[3]) * x + 1.0);
    } else {
        // Central region
        x = p - 0.5;
        r = x * x;
        x = (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * x /
            (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1.0);
    }

    return x;
}

}  // namespace pyfolio::stats