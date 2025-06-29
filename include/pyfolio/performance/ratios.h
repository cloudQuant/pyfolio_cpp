#pragma once

#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../core/types.h"
#include "../math/statistics.h"
#include "returns.h"
#include <cmath>

namespace pyfolio::performance {

/**
 * @brief Calculate Sharpe ratio
 */
inline Result<double> sharpe_ratio(const ReturnSeries& returns,
                                   double risk_free_rate = constants::DEFAULT_RISK_FREE_RATE,
                                   Frequency frequency   = Frequency::Daily) {
    if (returns.empty()) {
        return Result<double>::error(ErrorCode::InsufficientData,
                                     "Cannot calculate Sharpe ratio for empty return series");
    }

    // Calculate excess returns
    auto excess_returns_result = calculate_excess_returns(returns, risk_free_rate);
    if (excess_returns_result.is_error()) {
        return Result<double>::error(excess_returns_result.error().code, excess_returns_result.error().message);
    }

    const auto& excess_returns = excess_returns_result.value();

    // Calculate mean excess return
    auto mean_result = stats::mean(std::span<const Return>{excess_returns.values()});
    if (mean_result.is_error()) {
        return Result<double>::error(mean_result.error().code, mean_result.error().message);
    }

    // Calculate volatility of excess returns
    auto volatility_result = calculate_volatility(excess_returns, frequency);
    if (volatility_result.is_error()) {
        return Result<double>::error(volatility_result.error().code, volatility_result.error().message);
    }

    double mean_excess_return = mean_result.value();
    double volatility         = volatility_result.value();

    if (volatility == 0.0) {
        return Result<double>::error(ErrorCode::DivisionByZero, "Cannot calculate Sharpe ratio with zero volatility");
    }

    // Annualize the mean excess return
    double periods_per_year = frequency::to_annual_factor(frequency);
    double annualized_mean  = mean_excess_return * periods_per_year;

    double sharpe = annualized_mean / volatility;

    return Result<double>::success(sharpe);
}

/**
 * @brief Calculate Sortino ratio (downside risk-adjusted return)
 */
inline Result<double> sortino_ratio(const ReturnSeries& returns, double target_return = 0.0,
                                    Frequency frequency = Frequency::Daily) {
    if (returns.empty()) {
        return Result<double>::error(ErrorCode::InsufficientData,
                                     "Cannot calculate Sortino ratio for empty return series");
    }

    // Convert annual target return to period return
    double periods_per_year = frequency::to_annual_factor(frequency);
    double period_target    = target_return / periods_per_year;

    // Calculate mean return
    auto mean_result = stats::mean(std::span<const Return>{returns.values()});
    if (mean_result.is_error()) {
        return Result<double>::error(mean_result.error().code, mean_result.error().message);
    }

    double mean_return = mean_result.value();

    // Calculate downside deviation
    std::vector<double> downside_deviations;
    for (const auto& ret : returns) {
        if (ret < period_target) {
            double deviation = ret - period_target;
            downside_deviations.push_back(deviation * deviation);
        }
    }

    if (downside_deviations.empty()) {
        return Result<double>::error(ErrorCode::InsufficientData,
                                     "No downside deviations found for Sortino ratio calculation");
    }

    double downside_variance =
        std::accumulate(downside_deviations.begin(), downside_deviations.end(), 0.0) / downside_deviations.size();
    double downside_deviation = std::sqrt(downside_variance);

    if (downside_deviation == 0.0) {
        return Result<double>::error(ErrorCode::DivisionByZero,
                                     "Cannot calculate Sortino ratio with zero downside deviation");
    }

    // Annualize
    double annualized_mean         = mean_return * periods_per_year;
    double annualized_downside_dev = downside_deviation * std::sqrt(periods_per_year);

    double sortino = (annualized_mean - target_return) / annualized_downside_dev;

    return Result<double>::success(sortino);
}

/**
 * @brief Calculate Calmar ratio (return over maximum drawdown)
 */
inline Result<double> calmar_ratio(const ReturnSeries& returns, Frequency frequency = Frequency::Daily) {
    if (returns.empty()) {
        return Result<double>::error(ErrorCode::InsufficientData,
                                     "Cannot calculate Calmar ratio for empty return series");
    }

    // Calculate annualized return
    auto annualized_return_result = annualize_returns(returns, frequency);
    if (annualized_return_result.is_error()) {
        return Result<double>::error(annualized_return_result.error().code, annualized_return_result.error().message);
    }

    // Calculate maximum drawdown (we'll implement this in drawdown.h)
    // For now, simplified calculation
    auto cumulative_result = calculate_cumulative_returns(returns);
    if (cumulative_result.is_error()) {
        return Result<double>::error(cumulative_result.error().code, cumulative_result.error().message);
    }

    const auto& cumulative_returns = cumulative_result.value();

    double peak         = cumulative_returns[0];
    double max_drawdown = 0.0;

    for (const auto& ret : cumulative_returns) {
        if (ret > peak) {
            peak = ret;
        }

        double drawdown = (peak - ret) / (1.0 + peak);
        if (drawdown > max_drawdown) {
            max_drawdown = drawdown;
        }
    }

    if (max_drawdown == 0.0) {
        return Result<double>::error(ErrorCode::DivisionByZero,
                                     "Cannot calculate Calmar ratio with zero maximum drawdown");
    }

    double calmar = annualized_return_result.value() / max_drawdown;

    return Result<double>::success(calmar);
}

/**
 * @brief Calculate Information ratio (active return over tracking error)
 */
inline Result<double> information_ratio(const ReturnSeries& portfolio_returns, const ReturnSeries& benchmark_returns,
                                        Frequency frequency = Frequency::Daily) {
    if (portfolio_returns.size() != benchmark_returns.size()) {
        return Result<double>::error(ErrorCode::InvalidInput,
                                     "Portfolio and benchmark returns must have the same length");
    }

    if (portfolio_returns.empty()) {
        return Result<double>::error(ErrorCode::InsufficientData,
                                     "Cannot calculate Information ratio for empty return series");
    }

    // Calculate active returns (portfolio - benchmark)
    std::vector<Return> active_returns;
    active_returns.reserve(portfolio_returns.size());

    for (size_t i = 0; i < portfolio_returns.size(); ++i) {
        active_returns.push_back(portfolio_returns[i] - benchmark_returns[i]);
    }

    // Calculate mean active return
    auto mean_result = stats::mean(std::span<const Return>{active_returns});
    if (mean_result.is_error()) {
        return Result<double>::error(mean_result.error().code, mean_result.error().message);
    }

    // Calculate tracking error (volatility of active returns)
    auto std_result = stats::standard_deviation(std::span<const Return>{active_returns});
    if (std_result.is_error()) {
        return Result<double>::error(std_result.error().code, std_result.error().message);
    }

    double mean_active_return = mean_result.value();
    double tracking_error     = std_result.value();

    if (tracking_error == 0.0) {
        return Result<double>::error(ErrorCode::DivisionByZero,
                                     "Cannot calculate Information ratio with zero tracking error");
    }

    // Annualize
    double periods_per_year          = frequency::to_annual_factor(frequency);
    double annualized_active_return  = mean_active_return * periods_per_year;
    double annualized_tracking_error = tracking_error * std::sqrt(periods_per_year);

    double information_ratio_value = annualized_active_return / annualized_tracking_error;

    return Result<double>::success(information_ratio_value);
}

/**
 * @brief Calculate Omega ratio (probability weighted ratio)
 */
inline Result<double> omega_ratio(const ReturnSeries& returns, double threshold = 0.0,
                                  Frequency frequency = Frequency::Daily) {
    if (returns.empty()) {
        return Result<double>::error(ErrorCode::InsufficientData,
                                     "Cannot calculate Omega ratio for empty return series");
    }

    // Convert annual threshold to period threshold
    double periods_per_year = frequency::to_annual_factor(frequency);
    double period_threshold = threshold / periods_per_year;

    double gains_sum  = 0.0;
    double losses_sum = 0.0;

    for (const auto& ret : returns) {
        if (ret > period_threshold) {
            gains_sum += (ret - period_threshold);
        } else {
            losses_sum += (period_threshold - ret);
        }
    }

    if (losses_sum == 0.0) {
        if (gains_sum > 0.0) {
            return Result<double>::success(std::numeric_limits<double>::infinity());
        } else {
            return Result<double>::error(ErrorCode::DivisionByZero,
                                         "Cannot calculate Omega ratio with zero gains and losses");
        }
    }

    double omega = gains_sum / losses_sum;

    return Result<double>::success(omega);
}

/**
 * @brief Calculate Treynor ratio (return per unit of systematic risk)
 */
inline Result<double> treynor_ratio(const ReturnSeries& portfolio_returns, const ReturnSeries& benchmark_returns,
                                    double risk_free_rate = constants::DEFAULT_RISK_FREE_RATE,
                                    Frequency frequency   = Frequency::Daily) {
    if (portfolio_returns.size() != benchmark_returns.size()) {
        return Result<double>::error(ErrorCode::InvalidInput,
                                     "Portfolio and benchmark returns must have the same length");
    }

    if (portfolio_returns.empty()) {
        return Result<double>::error(ErrorCode::InsufficientData,
                                     "Cannot calculate Treynor ratio for empty return series");
    }

    // Calculate portfolio excess returns
    auto portfolio_excess_result = calculate_excess_returns(portfolio_returns, risk_free_rate);
    if (portfolio_excess_result.is_error()) {
        return Result<double>::error(portfolio_excess_result.error().code, portfolio_excess_result.error().message);
    }

    // Calculate benchmark excess returns
    auto benchmark_excess_result = calculate_excess_returns(benchmark_returns, risk_free_rate);
    if (benchmark_excess_result.is_error()) {
        return Result<double>::error(benchmark_excess_result.error().code, benchmark_excess_result.error().message);
    }

    const auto& portfolio_excess = portfolio_excess_result.value();
    const auto& benchmark_excess = benchmark_excess_result.value();

    // Calculate beta (correlation * (portfolio_vol / benchmark_vol))
    auto correlation_result = stats::correlation(std::span<const Return>{portfolio_excess.values()},
                                                 std::span<const Return>{benchmark_excess.values()});

    if (correlation_result.is_error()) {
        return Result<double>::error(correlation_result.error().code, correlation_result.error().message);
    }

    auto portfolio_vol_result = calculate_volatility(portfolio_excess, frequency);
    auto benchmark_vol_result = calculate_volatility(benchmark_excess, frequency);

    if (portfolio_vol_result.is_error() || benchmark_vol_result.is_error()) {
        return Result<double>::error(ErrorCode::CalculationError, "Failed to calculate volatilities for Treynor ratio");
    }

    double correlation   = correlation_result.value();
    double portfolio_vol = portfolio_vol_result.value();
    double benchmark_vol = benchmark_vol_result.value();

    if (benchmark_vol == 0.0) {
        return Result<double>::error(ErrorCode::DivisionByZero, "Cannot calculate beta with zero benchmark volatility");
    }

    double beta = correlation * (portfolio_vol / benchmark_vol);

    if (beta == 0.0) {
        return Result<double>::error(ErrorCode::DivisionByZero, "Cannot calculate Treynor ratio with zero beta");
    }

    // Calculate mean portfolio excess return
    auto mean_result = stats::mean(std::span<const Return>{portfolio_excess.values()});
    if (mean_result.is_error()) {
        return Result<double>::error(mean_result.error().code, mean_result.error().message);
    }

    double periods_per_year         = frequency::to_annual_factor(frequency);
    double annualized_excess_return = mean_result.value() * periods_per_year;

    double treynor = annualized_excess_return / beta;

    return Result<double>::success(treynor);
}

/**
 * @brief Calculate rolling Sharpe ratio
 */
inline Result<ReturnSeries> rolling_sharpe_ratio(const ReturnSeries& returns, size_t window_size,
                                                 double risk_free_rate = constants::DEFAULT_RISK_FREE_RATE,
                                                 Frequency frequency   = Frequency::Daily) {
    if (window_size == 0 || window_size > returns.size()) {
        return Result<ReturnSeries>::error(ErrorCode::InvalidInput, "Invalid window size for rolling Sharpe ratio");
    }

    std::vector<Return> rolling_sharpe_values;
    std::vector<DateTime> rolling_timestamps;

    rolling_sharpe_values.reserve(returns.size() - window_size + 1);
    rolling_timestamps.reserve(returns.size() - window_size + 1);

    for (size_t i = window_size - 1; i < returns.size(); ++i) {
        // Extract window
        std::vector<DateTime> window_timestamps;
        std::vector<Return> window_returns;

        for (size_t j = i - window_size + 1; j <= i; ++j) {
            window_timestamps.push_back(returns.timestamp(j));
            window_returns.push_back(returns[j]);
        }

        ReturnSeries window_series{std::move(window_timestamps), std::move(window_returns)};

        auto sharpe_result = sharpe_ratio(window_series, risk_free_rate, frequency);
        if (sharpe_result.is_error()) {
            // Use NaN for invalid calculations
            rolling_sharpe_values.push_back(std::numeric_limits<double>::quiet_NaN());
        } else {
            rolling_sharpe_values.push_back(sharpe_result.value());
        }

        rolling_timestamps.push_back(returns.timestamp(i));
    }

    return Result<ReturnSeries>::success(ReturnSeries{std::move(rolling_timestamps), std::move(rolling_sharpe_values),
                                                      returns.name() + "_rolling_sharpe"});
}

}  // namespace pyfolio::performance