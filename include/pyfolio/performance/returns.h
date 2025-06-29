#pragma once

#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../core/types.h"
#include "../math/statistics.h"
#include <cmath>
#include <vector>

namespace pyfolio::performance {

/**
 * @brief Calculate simple returns from prices
 */
inline Result<ReturnSeries> calculate_returns(const PriceSeries& prices) {
    if (prices.size() < 2) {
        return Result<ReturnSeries>::error(ErrorCode::InsufficientData,
                                           "Need at least 2 price points to calculate returns");
    }

    std::vector<DateTime> return_timestamps;
    std::vector<Return> returns;

    return_timestamps.reserve(prices.size() - 1);
    returns.reserve(prices.size() - 1);

    for (size_t i = 1; i < prices.size(); ++i) {
        double prev_price = prices[i - 1];
        double curr_price = prices[i];

        if (prev_price <= 0.0) {
            return Result<ReturnSeries>::error(ErrorCode::InvalidInput,
                                               "Price must be positive for return calculation");
        }

        double simple_return = (curr_price - prev_price) / prev_price;

        return_timestamps.push_back(prices.timestamp(i));
        returns.push_back(simple_return);
    }

    return Result<ReturnSeries>::success(
        ReturnSeries{std::move(return_timestamps), std::move(returns), prices.name() + "_returns"});
}

/**
 * @brief Calculate log returns from prices
 */
inline Result<ReturnSeries> calculate_log_returns(const PriceSeries& prices) {
    if (prices.size() < 2) {
        return Result<ReturnSeries>::error(ErrorCode::InsufficientData,
                                           "Need at least 2 price points to calculate returns");
    }

    std::vector<DateTime> return_timestamps;
    std::vector<Return> log_returns;

    return_timestamps.reserve(prices.size() - 1);
    log_returns.reserve(prices.size() - 1);

    for (size_t i = 1; i < prices.size(); ++i) {
        double prev_price = prices[i - 1];
        double curr_price = prices[i];

        if (prev_price <= 0.0 || curr_price <= 0.0) {
            return Result<ReturnSeries>::error(ErrorCode::InvalidInput,
                                               "Prices must be positive for log return calculation");
        }

        double log_return = std::log(curr_price / prev_price);

        return_timestamps.push_back(prices.timestamp(i));
        log_returns.push_back(log_return);
    }

    return Result<ReturnSeries>::success(
        ReturnSeries{std::move(return_timestamps), std::move(log_returns), prices.name() + "_log_returns"});
}

/**
 * @brief Calculate excess returns over risk-free rate
 */
inline Result<ReturnSeries> calculate_excess_returns(const ReturnSeries& returns,
                                                     double risk_free_rate = constants::DEFAULT_RISK_FREE_RATE) {
    std::vector<Return> excess_returns;
    excess_returns.reserve(returns.size());

    // Convert annual risk-free rate to period rate
    double period_risk_free = risk_free_rate / constants::TRADING_DAYS_PER_YEAR;

    for (const auto& ret : returns) {
        excess_returns.push_back(ret - period_risk_free);
    }

    return Result<ReturnSeries>::success(
        ReturnSeries{returns.timestamps(), std::move(excess_returns), returns.name() + "_excess"});
}

/**
 * @brief Calculate cumulative returns
 */
inline Result<ReturnSeries> calculate_cumulative_returns(const ReturnSeries& returns, double starting_value = 1.0) {
    if (returns.empty()) {
        return Result<ReturnSeries>::error(ErrorCode::InsufficientData,
                                           "Cannot calculate cumulative returns for empty series");
    }

    std::vector<Return> cumulative_returns;
    cumulative_returns.reserve(returns.size());

    double cumulative_value = starting_value;

    for (const auto& ret : returns) {
        cumulative_value *= (1.0 + ret);
        cumulative_returns.push_back(cumulative_value - starting_value);
    }

    return Result<ReturnSeries>::success(
        ReturnSeries{returns.timestamps(), std::move(cumulative_returns), returns.name() + "_cumulative"});
}

/**
 * @brief Annualize returns
 */
inline Result<double> annualize_returns(const ReturnSeries& returns, Frequency frequency = Frequency::Daily) {
    if (returns.empty()) {
        return Result<double>::error(ErrorCode::InsufficientData, "Cannot annualize empty return series");
    }

    auto mean_result = stats::mean(std::span<const Return>{returns.values()});
    if (mean_result.is_error()) {
        return mean_result;
    }

    double period_return    = mean_result.value();
    double periods_per_year = frequency::to_annual_factor(frequency);

    // Compound the returns
    double annualized = std::pow(1.0 + period_return, periods_per_year) - 1.0;

    return Result<double>::success(annualized);
}

/**
 * @brief Aggregate returns to different frequency
 */
inline Result<ReturnSeries> aggregate_returns(const ReturnSeries& returns, Frequency target_frequency) {
    if (returns.empty()) {
        return Result<ReturnSeries>::error(ErrorCode::InsufficientData, "Cannot aggregate empty return series");
    }

    // Use TimeSeries resample functionality
    auto aggregated = returns.resample(target_frequency, [](const std::vector<Return>& period_returns) -> Return {
        // Compound the returns within the period
        double cumulative = 1.0;
        for (const auto& ret : period_returns) {
            cumulative *= (1.0 + ret);
        }
        return cumulative - 1.0;
    });

    if (aggregated.is_error()) {
        return aggregated;
    }

    auto result = aggregated.value();
    result.set_name(returns.name() + "_aggregated");

    return Result<ReturnSeries>::success(std::move(result));
}

/**
 * @brief Calculate rolling returns
 */
inline Result<ReturnSeries> rolling_returns(const ReturnSeries& returns, size_t window_size) {
    if (window_size == 0 || window_size > returns.size()) {
        return Result<ReturnSeries>::error(ErrorCode::InvalidInput, "Invalid window size for rolling returns");
    }

    auto rolling_result = returns.rolling(window_size, [](std::span<const Return> window) -> Return {
        // Compound the returns in the window
        double cumulative = 1.0;
        for (const auto& ret : window) {
            cumulative *= (1.0 + ret);
        }
        return cumulative - 1.0;
    });

    if (rolling_result.is_error()) {
        return rolling_result;
    }

    auto result = rolling_result.value();
    result.set_name(returns.name() + "_rolling");

    return Result<ReturnSeries>::success(std::move(result));
}

/**
 * @brief Calculate return volatility (annualized standard deviation)
 */
inline Result<double> calculate_volatility(const ReturnSeries& returns, Frequency frequency = Frequency::Daily) {
    if (returns.empty()) {
        return Result<double>::error(ErrorCode::InsufficientData,
                                     "Cannot calculate volatility for empty return series");
    }

    auto std_result = stats::standard_deviation(std::span<const Return>{returns.values()});
    if (std_result.is_error()) {
        return std_result;
    }

    double period_volatility = std_result.value();
    double periods_per_year  = frequency::to_annual_factor(frequency);

    // Annualize volatility
    double annualized_volatility = period_volatility * std::sqrt(periods_per_year);

    return Result<double>::success(annualized_volatility);
}

/**
 * @brief Calculate rolling volatility
 */
inline Result<ReturnSeries> rolling_volatility(const ReturnSeries& returns, size_t window_size,
                                               Frequency frequency = Frequency::Daily) {
    auto rolling_std_result = stats::rolling_std(std::span<const Return>{returns.values()}, window_size);
    if (rolling_std_result.is_error()) {
        return Result<ReturnSeries>::error(rolling_std_result.error().code, "Rolling volatility calculation failed: " +
                                                                                rolling_std_result.error().message);
    }

    double periods_per_year  = frequency::to_annual_factor(frequency);
    auto& rolling_std_values = rolling_std_result.value();

    // Annualize volatilities
    std::vector<Return> annualized_volatilities;
    annualized_volatilities.reserve(rolling_std_values.size());

    for (double vol : rolling_std_values) {
        annualized_volatilities.push_back(vol * std::sqrt(periods_per_year));
    }

    // Create timestamps for rolling results
    std::vector<DateTime> rolling_timestamps;
    rolling_timestamps.reserve(annualized_volatilities.size());

    for (size_t i = window_size - 1; i < returns.size(); ++i) {
        rolling_timestamps.push_back(returns.timestamp(i));
    }

    return Result<ReturnSeries>::success(ReturnSeries{std::move(rolling_timestamps), std::move(annualized_volatilities),
                                                      returns.name() + "_rolling_volatility"});
}

/**
 * @brief Calculate total return over period
 */
inline Result<double> total_return(const ReturnSeries& returns) {
    if (returns.empty()) {
        return Result<double>::error(ErrorCode::InsufficientData, "Cannot calculate total return for empty series");
    }

    double total = 1.0;
    for (const auto& ret : returns) {
        total *= (1.0 + ret);
    }

    return Result<double>::success(total - 1.0);
}

/**
 * @brief Calculate compound annual growth rate (CAGR)
 */
inline Result<double> calculate_cagr(const ReturnSeries& returns, Frequency frequency = Frequency::Daily) {
    if (returns.empty()) {
        return Result<double>::error(ErrorCode::InsufficientData, "Cannot calculate CAGR for empty return series");
    }

    auto total_return_result = total_return(returns);
    if (total_return_result.is_error()) {
        return total_return_result;
    }

    double total_ret        = total_return_result.value();
    double periods_per_year = frequency::to_annual_factor(frequency);
    double years            = returns.size() / periods_per_year;

    if (years <= 0.0) {
        return Result<double>::error(ErrorCode::InvalidInput, "Need at least one year of data for CAGR calculation");
    }

    double cagr = std::pow(1.0 + total_ret, 1.0 / years) - 1.0;

    return Result<double>::success(cagr);
}

/**
 * @brief Convert returns to different base (e.g., percentage)
 */
inline Result<ReturnSeries> convert_returns(const ReturnSeries& returns, double multiplier = 100.0) {
    std::vector<Return> converted_returns;
    converted_returns.reserve(returns.size());

    for (const auto& ret : returns) {
        converted_returns.push_back(ret * multiplier);
    }

    return Result<ReturnSeries>::success(
        ReturnSeries{returns.timestamps(), std::move(converted_returns), returns.name() + "_converted"});
}

}  // namespace pyfolio::performance