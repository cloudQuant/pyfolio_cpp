#pragma once

#include "../core/time_series.h"
#include "../core/types.h"
#include "../math/statistics.h"
#include "../performance/ratios.h"
#include "../performance/returns.h"
#include <algorithm>
#include <cmath>

namespace pyfolio {
namespace performance {

using namespace pyfolio::stats;

/**
 * @brief Calculate rolling volatility
 *
 * @param returns Time series of returns
 * @param window Window size for rolling calculation
 * @param min_periods Minimum number of observations required
 * @param annualization_factor Factor to annualize volatility (default: 252 for daily)
 * @return Time series of rolling volatility
 */
inline TimeSeries<double> calculate_rolling_volatility(const TimeSeries<Return>& returns, size_t window,
                                                       size_t min_periods = 1, double annualization_factor = 252.0) {
    TimeSeries<double> rolling_vol;
    const auto& timestamps = returns.timestamps();
    const auto& values     = returns.values();

    if (values.size() < min_periods) {
        return rolling_vol;
    }

    // Calculate rolling volatility
    for (size_t i = 0; i < values.size(); ++i) {
        if (i + 1 < min_periods) {
            continue;
        }

        size_t start_idx     = (i >= window - 1) ? i - window + 1 : 0;
        size_t actual_window = i - start_idx + 1;

        if (actual_window >= min_periods) {
            // Calculate volatility for the window
            double sum    = 0.0;
            double sum_sq = 0.0;

            for (size_t j = start_idx; j <= i; ++j) {
                double ret = values[j];
                sum += ret;
                sum_sq += ret * ret;
            }

            double mean     = sum / actual_window;
            double variance = (sum_sq / actual_window) - (mean * mean);
            double vol      = std::sqrt(variance * annualization_factor);

            rolling_vol.push_back(timestamps[i], vol);
        }
    }

    return rolling_vol;
}

/**
 * @brief Calculate rolling Sharpe ratio
 *
 * @param returns Time series of returns
 * @param window Window size for rolling calculation
 * @param risk_free_rate Risk-free rate (annualized)
 * @param periods_per_year Number of periods per year
 * @param min_periods Minimum number of observations required
 * @return Time series of rolling Sharpe ratios
 */
inline TimeSeries<double> calculate_rolling_sharpe(const TimeSeries<Return>& returns, size_t window,
                                                   double risk_free_rate = 0.0, int periods_per_year = 252,
                                                   size_t min_periods = 1) {
    TimeSeries<double> rolling_sharpe;
    const auto& timestamps = returns.timestamps();
    const auto& values     = returns.values();

    if (values.size() < min_periods) {
        return rolling_sharpe;
    }

    double period_risk_free = risk_free_rate / periods_per_year;

    for (size_t i = 0; i < values.size(); ++i) {
        if (i + 1 < min_periods) {
            continue;
        }

        size_t start_idx     = (i >= window - 1) ? i - window + 1 : 0;
        size_t actual_window = i - start_idx + 1;

        if (actual_window >= min_periods) {
            // Extract window returns
            std::vector<double> window_returns;
            window_returns.reserve(actual_window);

            for (size_t j = start_idx; j <= i; ++j) {
                window_returns.push_back(values[j] - period_risk_free);
            }

            // Calculate Sharpe ratio for the window
            auto mean_result = stats::mean(std::span<const double>(window_returns));
            auto std_result  = stats::standard_deviation(std::span<const double>(window_returns));

            if (mean_result.is_error() || std_result.is_error()) {
                continue;
            }

            double mean    = mean_result.value();
            double std_dev = std_result.value();

            double sharpe = 0.0;
            if (std_dev > 1e-8) {
                sharpe = mean / std_dev * std::sqrt(static_cast<double>(periods_per_year));
            }

            rolling_sharpe.push_back(timestamps[i], sharpe);
        }
    }

    return rolling_sharpe;
}

/**
 * @brief Calculate rolling beta (simplified version without alignment)
 *
 * Assumes returns and benchmark_returns have same timestamps
 *
 * @param returns Time series of strategy returns
 * @param benchmark_returns Time series of benchmark returns
 * @param window Window size for rolling calculation
 * @param min_periods Minimum number of observations required
 * @return Time series of rolling beta values
 */
inline TimeSeries<double> calculate_rolling_beta(const TimeSeries<Return>& returns,
                                                 const TimeSeries<Return>& benchmark_returns, size_t window,
                                                 size_t min_periods = 1) {
    TimeSeries<double> rolling_beta;

    const auto& timestamps   = returns.timestamps();
    const auto& strat_values = returns.values();
    const auto& bench_values = benchmark_returns.values();

    size_t min_size = std::min(strat_values.size(), bench_values.size());

    if (min_size < min_periods) {
        return rolling_beta;
    }

    for (size_t i = 0; i < min_size; ++i) {
        if (i + 1 < min_periods) {
            continue;
        }

        size_t start_idx     = (i >= window - 1) ? i - window + 1 : 0;
        size_t actual_window = i - start_idx + 1;

        if (actual_window >= min_periods) {
            // Extract window data
            std::vector<double> strat_returns, bench_returns;
            strat_returns.reserve(actual_window);
            bench_returns.reserve(actual_window);

            for (size_t j = start_idx; j <= i; ++j) {
                strat_returns.push_back(strat_values[j]);
                bench_returns.push_back(bench_values[j]);
            }

            // Calculate beta
            auto cov_result =
                stats::covariance(std::span<const double>(strat_returns), std::span<const double>(bench_returns));
            auto var_result = stats::variance(std::span<const double>(bench_returns));

            if (cov_result.is_error() || var_result.is_error()) {
                continue;
            }

            double covariance     = cov_result.value();
            double bench_variance = var_result.value();

            double beta = 0.0;
            if (bench_variance > 1e-8) {
                beta = covariance / bench_variance;
            }

            rolling_beta.push_back(timestamps[i], beta);
        }
    }

    return rolling_beta;
}

/**
 * @brief Calculate rolling correlation
 *
 * @param returns Time series of strategy returns
 * @param benchmark_returns Time series of benchmark returns
 * @param window Window size for rolling calculation
 * @param min_periods Minimum number of observations required
 * @return Time series of rolling correlation values
 */
inline TimeSeries<double> calculate_rolling_correlation(const TimeSeries<Return>& returns,
                                                        const TimeSeries<Return>& benchmark_returns, size_t window,
                                                        size_t min_periods = 1) {
    TimeSeries<double> rolling_corr;

    const auto& timestamps   = returns.timestamps();
    const auto& strat_values = returns.values();
    const auto& bench_values = benchmark_returns.values();

    size_t min_size = std::min(strat_values.size(), bench_values.size());

    if (min_size < min_periods) {
        return rolling_corr;
    }

    for (size_t i = 0; i < min_size; ++i) {
        if (i + 1 < min_periods) {
            continue;
        }

        size_t start_idx     = (i >= window - 1) ? i - window + 1 : 0;
        size_t actual_window = i - start_idx + 1;

        if (actual_window >= min_periods) {
            // Extract window data
            std::vector<double> strat_returns, bench_returns;
            strat_returns.reserve(actual_window);
            bench_returns.reserve(actual_window);

            for (size_t j = start_idx; j <= i; ++j) {
                strat_returns.push_back(strat_values[j]);
                bench_returns.push_back(bench_values[j]);
            }

            // Calculate correlation
            auto corr_result =
                stats::correlation(std::span<const double>(strat_returns), std::span<const double>(bench_returns));

            if (corr_result.is_error()) {
                continue;
            }

            double correlation = corr_result.value();
            rolling_corr.push_back(timestamps[i], correlation);
        }
    }

    return rolling_corr;
}

/**
 * @brief Calculate rolling maximum drawdown
 *
 * @param returns Time series of returns
 * @param window Window size for rolling calculation
 * @param min_periods Minimum number of observations required
 * @return Time series of rolling maximum drawdown values
 */
inline TimeSeries<double> calculate_rolling_max_drawdown(const TimeSeries<Return>& returns, size_t window,
                                                         size_t min_periods = 1) {
    TimeSeries<double> rolling_dd;
    const auto& timestamps = returns.timestamps();
    const auto& values     = returns.values();

    if (values.size() < min_periods) {
        return rolling_dd;
    }

    // First calculate cumulative returns
    std::vector<double> cum_returns;
    cum_returns.reserve(values.size());
    double cum_prod = 1.0;

    for (const auto& ret : values) {
        cum_prod *= (1.0 + ret);
        cum_returns.push_back(cum_prod);
    }

    // Calculate rolling max drawdown
    for (size_t i = 0; i < values.size(); ++i) {
        if (i + 1 < min_periods) {
            continue;
        }

        size_t start_idx     = (i >= window - 1) ? i - window + 1 : 0;
        size_t actual_window = i - start_idx + 1;

        if (actual_window >= min_periods) {
            // Find max drawdown in the window
            double max_dd     = 0.0;
            double window_max = cum_returns[start_idx];

            for (size_t j = start_idx + 1; j <= i; ++j) {
                window_max = std::max(window_max, cum_returns[j]);
                double dd  = (window_max - cum_returns[j]) / window_max;
                max_dd     = std::max(max_dd, dd);
            }

            rolling_dd.push_back(timestamps[i], max_dd);
        }
    }

    return rolling_dd;
}

/**
 * @brief Calculate rolling Sortino ratio
 *
 * @param returns Time series of returns
 * @param window Window size for rolling calculation
 * @param risk_free_rate Risk-free rate (annualized)
 * @param periods_per_year Number of periods per year
 * @param min_periods Minimum number of observations required
 * @return Time series of rolling Sortino ratios
 */
inline TimeSeries<double> calculate_rolling_sortino(const TimeSeries<Return>& returns, size_t window,
                                                    double risk_free_rate = 0.0, int periods_per_year = 252,
                                                    size_t min_periods = 1) {
    TimeSeries<double> rolling_sortino;
    const auto& timestamps = returns.timestamps();
    const auto& values     = returns.values();

    if (values.size() < min_periods) {
        return rolling_sortino;
    }

    double period_risk_free = risk_free_rate / periods_per_year;

    for (size_t i = 0; i < values.size(); ++i) {
        if (i + 1 < min_periods) {
            continue;
        }

        size_t start_idx     = (i >= window - 1) ? i - window + 1 : 0;
        size_t actual_window = i - start_idx + 1;

        if (actual_window >= min_periods) {
            // Extract window returns
            std::vector<double> window_returns;
            window_returns.reserve(actual_window);

            for (size_t j = start_idx; j <= i; ++j) {
                window_returns.push_back(values[j]);
            }

            // Calculate Sortino ratio
            auto mean_result = stats::mean(std::span<const double>(window_returns));

            if (mean_result.is_error()) {
                continue;
            }

            double mean_return   = mean_result.value();
            double excess_return = mean_return - period_risk_free;

            // Calculate downside deviation
            double downside_sum = 0.0;
            int downside_count  = 0;
            for (double ret : window_returns) {
                double excess = ret - period_risk_free;
                if (excess < 0) {
                    downside_sum += excess * excess;
                    downside_count++;
                }
            }

            double sortino = 0.0;
            if (downside_count > 0) {
                double downside_dev = std::sqrt(downside_sum / downside_count);
                if (downside_dev > 1e-8) {
                    sortino = excess_return / downside_dev * std::sqrt(static_cast<double>(periods_per_year));
                }
            }

            rolling_sortino.push_back(timestamps[i], sortino);
        }
    }

    return rolling_sortino;
}

/**
 * @brief Calculate rolling downside deviation
 *
 * @param returns Time series of returns
 * @param window Window size for rolling calculation
 * @param mar Minimum acceptable return (default: 0)
 * @param periods_per_year Number of periods per year for annualization
 * @param min_periods Minimum number of observations required
 * @return Time series of rolling downside deviation
 */
inline TimeSeries<double> calculate_rolling_downside_deviation(const TimeSeries<Return>& returns, size_t window,
                                                               double mar = 0.0, int periods_per_year = 252,
                                                               size_t min_periods = 1) {
    TimeSeries<double> rolling_dd;
    const auto& timestamps = returns.timestamps();
    const auto& values     = returns.values();

    if (values.size() < min_periods) {
        return rolling_dd;
    }

    for (size_t i = 0; i < values.size(); ++i) {
        if (i + 1 < min_periods) {
            continue;
        }

        size_t start_idx     = (i >= window - 1) ? i - window + 1 : 0;
        size_t actual_window = i - start_idx + 1;

        if (actual_window >= min_periods) {
            // Calculate downside deviation for the window
            double sum_squares    = 0.0;
            size_t downside_count = 0;

            for (size_t j = start_idx; j <= i; ++j) {
                double excess = values[j] - mar;
                if (excess < 0) {
                    sum_squares += excess * excess;
                    downside_count++;
                }
            }

            double downside_dev = 0.0;
            if (downside_count > 0) {
                downside_dev = std::sqrt(sum_squares / actual_window * periods_per_year);
            }

            rolling_dd.push_back(timestamps[i], downside_dev);
        }
    }

    return rolling_dd;
}

}  // namespace performance
}  // namespace pyfolio