#pragma once

/**
 * @file parallel_performance_suite.h
 * @brief Integration of parallel processing with performance analysis suite
 * @version 1.0.0
 * @date 2024
 *
 * This file integrates parallel processing capabilities with the performance
 * analysis suite for maximum performance on large datasets.
 */

#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../parallel/parallel_algorithms.h"
#include "performance_analysis_suite.h"
#include <memory>

namespace pyfolio::analytics {

/**
 * @brief Enhanced performance analysis suite with parallel processing
 */
class ParallelPerformanceAnalysisSuite : public PerformanceAnalysisSuite {
  private:
    std::unique_ptr<parallel::ParallelAlgorithms> parallel_algo_;

    /**
     * @brief Calculate risk metrics using parallel processing
     */
    template <typename T>
    Result<void> calculate_risk_metrics_parallel(const TimeSeries<T>& returns, AnalysisReport& report) const {
        // Use parallel algorithms for large datasets
        if (returns.size() >= 10000) {
            // Parallel VaR and CVaR calculation
            auto sorted_returns = returns.values();

            // Sort the returns
            std::sort(sorted_returns.begin(), sorted_returns.end());

            if (!sorted_returns.empty()) {
                size_t var_index = static_cast<size_t>(sorted_returns.size() * 0.05);
                report.var_95    = sorted_returns[var_index];

                // Parallel CVaR calculation
                auto cvar_result = parallel_algo_->parallel_reduce(
                    std::vector<double>(sorted_returns.begin(), sorted_returns.begin() + var_index + 1), 0.0,
                    std::plus<double>());

                if (cvar_result.is_ok()) {
                    report.cvar_95 = cvar_result.value() / (var_index + 1);
                }
            }

            // Parallel downside deviation calculation
            auto mean_result = parallel_algo_->parallel_mean(returns);
            if (mean_result.is_error())
                return Result<void>::error(mean_result.error().code, mean_result.error().message);

            double mean_return = mean_result.value();
            const auto& values = returns.values();

            // Parallel calculation of downside deviations
            auto downside_values_result = parallel_algo_->parallel_map(values, [mean_return](double val) -> double {
                return val < mean_return ? (val - mean_return) * (val - mean_return) : 0.0;
            });

            if (downside_values_result.is_ok()) {
                auto downside_sum_result =
                    parallel_algo_->parallel_reduce(downside_values_result.value(), 0.0, std::plus<double>());

                if (downside_sum_result.is_ok()) {
                    size_t downside_count = std::count_if(values.begin(), values.end(),
                                                          [mean_return](double val) { return val < mean_return; });

                    report.downside_deviation =
                        downside_count > 0 ? std::sqrt(downside_sum_result.value() / downside_count) : 0.0;
                }
            }
        } else {
            // Use serial implementation for smaller datasets
            return PerformanceAnalysisSuite::calculate_risk_metrics(returns, report);
        }

        return Result<void>::success();
    }

    /**
     * @brief Calculate rolling metrics using parallel processing
     */
    template <typename T>
    Result<void> calculate_rolling_metrics_parallel(const TimeSeries<T>& returns, AnalysisReport& report) const {
        for (size_t window : get_config().rolling_windows) {
            if (window < returns.size()) {
                // Use parallel rolling calculations for large datasets
                if (returns.size() >= 10000) {
                    auto rolling_mean_result = parallel_algo_->parallel_rolling_mean(returns, window);
                    if (rolling_mean_result.is_ok()) {
                        report.rolling_returns[window] = rolling_mean_result.value();
                    }

                    auto rolling_std_result = parallel_algo_->parallel_rolling_std(returns, window);
                    if (rolling_std_result.is_ok()) {
                        report.rolling_volatility[window] = rolling_std_result.value();

                        // Calculate rolling Sharpe ratio
                        const auto& vol_series = rolling_std_result.value();
                        const auto& ret_series = report.rolling_returns[window];

                        if (vol_series.size() == ret_series.size()) {
                            std::vector<DateTime> dates = vol_series.timestamps();
                            std::vector<double> sharpe_values;

                            double daily_rf = get_config().risk_free_rate / get_config().periods_per_year;

                            // Parallel Sharpe ratio calculation
                            std::vector<std::pair<double, double>> vol_ret_pairs;
                            for (size_t i = 0; i < vol_series.size(); ++i) {
                                vol_ret_pairs.emplace_back(vol_series[i], ret_series[i]);
                            }

                            auto sharpe_result = parallel_algo_->parallel_map(
                                vol_ret_pairs, [daily_rf](const std::pair<double, double>& pair) -> double {
                                    double vol = pair.first;
                                    double ret = pair.second;
                                    if (vol > 0) {
                                        double excess_return = ret - daily_rf;
                                        return excess_return / vol;
                                    }
                                    return 0.0;
                                });

                            if (sharpe_result.is_ok()) {
                                report.rolling_sharpe[window] = TimeSeries<double>(
                                    dates, sharpe_result.value(), "rolling_sharpe_" + std::to_string(window));
                            }
                        }
                    }
                } else {
                    // Use cached calculations for smaller datasets
                    return PerformanceAnalysisSuite::calculate_rolling_metrics(returns, report);
                }
            }
        }

        return Result<void>::success();
    }

  public:
    explicit ParallelPerformanceAnalysisSuite(AnalysisConfig config = {})
        : PerformanceAnalysisSuite(std::move(config)),
          parallel_algo_(std::make_unique<parallel::ParallelAlgorithms>()) {}

    /**
     * @brief Perform comprehensive performance analysis with parallel processing
     */
    template <typename T>
    [[nodiscard]] Result<AnalysisReport> analyze_performance_parallel(
        const TimeSeries<T>& returns, const std::optional<TimeSeries<T>>& benchmark = std::nullopt) const {
        auto start_time = std::chrono::high_resolution_clock::now();
        AnalysisReport report{};
        report.analysis_timestamp = std::chrono::steady_clock::now();

        try {
            // Determine if we should use parallel processing
            bool use_parallel = returns.size() >= 10000;

            if (use_parallel) {
                // Parallel basic performance metrics
                auto mean_result = parallel_algo_->parallel_mean(returns);
                if (mean_result.is_error())
                    return Result<AnalysisReport>::error(mean_result.error().code, mean_result.error().message);

                auto std_result = parallel_algo_->parallel_std_deviation(returns);
                if (std_result.is_error())
                    return Result<AnalysisReport>::error(std_result.error().code, std_result.error().message);

                // Calculate cumulative returns for total return
                auto cumulative_result = returns.cumsum();
                if (cumulative_result.is_error())
                    return Result<AnalysisReport>::error(cumulative_result.error().code,
                                                         cumulative_result.error().message);

                const auto& cum_returns = cumulative_result.value();
                report.total_return     = cum_returns.empty() ? 0.0 : cum_returns[cum_returns.size() - 1];

                // Annualized metrics
                report.annual_return     = mean_result.value() * get_config().periods_per_year;
                report.annual_volatility = std_result.value() * std::sqrt(get_config().periods_per_year);

                // Parallel Sharpe ratio calculation
                double daily_rf      = get_config().risk_free_rate / get_config().periods_per_year;
                double excess_return = mean_result.value() - daily_rf;
                report.sharpe_ratio  = std_result.value() > 0 ? excess_return / std_result.value() : 0.0;

                // Parallel maximum drawdown calculation
                auto max_dd_result = parallel_algo_->parallel_reduce(
                    cum_returns.values(), std::pair<double, double>{cum_returns[0], 0.0},  // {peak, max_drawdown}
                    [](const std::pair<double, double>& acc, double val) {
                        double new_peak = std::max(acc.first, val);
                        double drawdown = new_peak > 0 ? (new_peak - val) / new_peak : 0.0;
                        double max_dd   = std::max(acc.second, drawdown);
                        return std::make_pair(new_peak, max_dd);
                    });

                if (max_dd_result.is_ok()) {
                    report.max_drawdown = max_dd_result.value().second;
                }

                // Calmar ratio
                report.calmar_ratio = report.max_drawdown > 0 ? report.annual_return / report.max_drawdown : 0.0;

                // Parallel risk metrics
                auto risk_result = calculate_risk_metrics_parallel(returns, report);
                if (risk_result.is_error())
                    return Result<AnalysisReport>::error(risk_result.error().code, risk_result.error().message);

                // Parallel rolling metrics
                auto rolling_result = calculate_rolling_metrics_parallel(returns, report);
                if (rolling_result.is_error())
                    return Result<AnalysisReport>::error(rolling_result.error().code, rolling_result.error().message);

                // Parallel benchmark comparison if provided
                if (benchmark) {
                    auto benchmark_corr = parallel_algo_->parallel_correlation(returns, *benchmark);
                    if (benchmark_corr.is_ok()) {
                        auto benchmark_mean = parallel_algo_->parallel_mean(*benchmark);
                        auto benchmark_std  = parallel_algo_->parallel_std_deviation(*benchmark);

                        if (benchmark_mean.is_ok() && benchmark_std.is_ok()) {
                            // Beta calculation
                            double covariance = benchmark_corr.value() * std_result.value() * benchmark_std.value();
                            double benchmark_variance = benchmark_std.value() * benchmark_std.value();
                            report.beta               = benchmark_variance > 0 ? covariance / benchmark_variance : 0.0;

                            // Alpha calculation
                            double benchmark_annual_return = benchmark_mean.value() * get_config().periods_per_year;
                            report.alpha =
                                report.annual_return -
                                (get_config().risk_free_rate +
                                 report.beta.value() * (benchmark_annual_return - get_config().risk_free_rate));

                            // Parallel tracking error calculation
                            std::vector<double> excess_returns;
                            const auto& ret_vals   = returns.values();
                            const auto& bench_vals = benchmark->values();

                            size_t min_size = std::min(ret_vals.size(), bench_vals.size());
                            excess_returns.reserve(min_size);

                            for (size_t i = 0; i < min_size; ++i) {
                                excess_returns.push_back(ret_vals[i] - bench_vals[i]);
                            }

                            if (!excess_returns.empty()) {
                                // Create TimeSeries for parallel processing
                                std::vector<DateTime> excess_dates(
                                    returns.timestamps().begin(), returns.timestamps().begin() + excess_returns.size());

                                TimeSeries<double> excess_ts(excess_dates, excess_returns, "excess_returns");

                                auto excess_std  = parallel_algo_->parallel_std_deviation(excess_ts);
                                auto excess_mean = parallel_algo_->parallel_mean(excess_ts);

                                if (excess_std.is_ok() && excess_mean.is_ok()) {
                                    report.tracking_error =
                                        excess_std.value() * std::sqrt(get_config().periods_per_year);
                                    report.information_ratio =
                                        report.tracking_error.value() > 0
                                            ? (excess_mean.value() * get_config().periods_per_year) /
                                                  report.tracking_error.value()
                                            : 0.0;
                                }
                            }
                        }
                    }
                }
            } else {
                // Use cached performance analysis suite for smaller datasets
                return PerformanceAnalysisSuite::analyze_performance(returns, benchmark);
            }

            // Statistical moments calculation (parallel for large datasets)
            const auto& values = returns.values();
            if (!values.empty() && use_parallel) {
                auto mean_result = parallel_algo_->parallel_mean(returns);
                if (mean_result.is_ok()) {
                    double mean_val = mean_result.value();

                    // Parallel moments calculation
                    auto moments_result = parallel_algo_->parallel_reduce(
                        values, std::array<double, 3>{0.0, 0.0, 0.0},  // {variance_sum, skew_sum, kurt_sum}
                        [mean_val](const std::array<double, 3>& acc, double val) {
                            double diff  = val - mean_val;
                            double diff2 = diff * diff;
                            return std::array<double, 3>{acc[0] + diff2, acc[1] + diff2 * diff, acc[2] + diff2 * diff2};
                        });

                    if (moments_result.is_ok()) {
                        const auto& moments = moments_result.value();
                        double variance     = moments[0] / values.size();
                        double std_dev      = std::sqrt(variance);

                        if (std_dev > 0) {
                            report.skewness = (moments[1] / values.size()) / (std_dev * std_dev * std_dev);
                            report.kurtosis = (moments[2] / values.size()) / (variance * variance) - 3.0;
                        }
                    }
                }
            }

            // Sortino ratio calculation
            report.sortino_ratio = report.downside_deviation > 0
                                       ? (report.annual_return - get_config().risk_free_rate) /
                                             (report.downside_deviation * std::sqrt(get_config().periods_per_year))
                                       : 0.0;

            // Performance analysis and risk checks
            // Risk analysis performed in parallel above

            // Cache statistics (from parent class)
            // Note: Parallel processing doesn't use the cache directly

            // Timing information
            auto end_time           = std::chrono::high_resolution_clock::now();
            report.computation_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

            return Result<AnalysisReport>::success(std::move(report));

        } catch (const std::exception& e) {
            return Result<AnalysisReport>::error(ErrorCode::CalculationError,
                                                 "Parallel analysis failed: " + std::string(e.what()));
        }
    }

    /**
     * @brief Get parallel processing statistics
     */
    parallel::ParallelAlgorithms::PerformanceStats get_parallel_stats() const {
        return parallel_algo_->get_performance_stats();
    }

    /**
     * @brief Update parallel processing configuration
     */
    void update_parallel_config(const parallel::ParallelConfig& config) { parallel_algo_->update_config(config); }
};

/**
 * @brief Global parallel performance analysis suite instance
 */
inline ParallelPerformanceAnalysisSuite& get_global_parallel_analysis_suite() {
    static ParallelPerformanceAnalysisSuite global_suite;
    return global_suite;
}

/**
 * @brief Convenience function for parallel performance analysis
 */
template <typename T>
[[nodiscard]] Result<AnalysisReport> analyze_portfolio_performance_parallel(
    const TimeSeries<T>& returns, const std::optional<TimeSeries<T>>& benchmark = std::nullopt) {
    return get_global_parallel_analysis_suite().analyze_performance_parallel(returns, benchmark);
}

}  // namespace pyfolio::analytics