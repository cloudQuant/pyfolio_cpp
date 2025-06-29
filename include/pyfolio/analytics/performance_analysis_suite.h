#pragma once

/**
 * @file performance_analysis_suite.h
 * @brief Advanced performance analysis suite with intelligent caching
 * @version 1.0.0
 * @date 2024
 *
 * This file provides a comprehensive performance analysis framework that
 * combines cached calculations with portfolio analytics for real-time
 * high-frequency trading applications.
 */

#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "cached_performance_metrics.h"
#include "performance_metrics.h"
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace pyfolio::analytics {

/**
 * @brief Performance analysis configuration
 */
struct AnalysisConfig {
    // Cache settings
    CacheConfig cache_config;

    // Analysis parameters
    double risk_free_rate               = 0.02;
    int periods_per_year                = 252;
    std::vector<size_t> rolling_windows = {30, 60, 90, 252};

    // Performance thresholds
    double min_sharpe_threshold   = 1.0;
    double max_drawdown_threshold = 0.10;  // 10%
    double min_return_threshold   = 0.08;  // 8% annual

    // Reporting settings
    bool enable_detailed_reports = true;
    bool enable_benchmarking     = true;
    std::chrono::milliseconds report_interval{1000};
};

/**
 * @brief Comprehensive performance analysis result
 */
struct AnalysisReport {
    // Basic metrics
    double total_return;
    double annual_return;
    double annual_volatility;
    double sharpe_ratio;
    double sortino_ratio;
    double max_drawdown;
    double calmar_ratio;

    // Risk metrics
    double var_95;
    double cvar_95;
    double downside_deviation;
    double skewness;
    double kurtosis;

    // Rolling metrics
    std::unordered_map<size_t, TimeSeries<double>> rolling_returns;
    std::unordered_map<size_t, TimeSeries<double>> rolling_volatility;
    std::unordered_map<size_t, TimeSeries<double>> rolling_sharpe;

    // Performance vs benchmark (if available)
    std::optional<double> alpha;
    std::optional<double> beta;
    std::optional<double> information_ratio;
    std::optional<double> tracking_error;

    // Cache performance
    CachedPerformanceCalculator::CacheStats cache_stats;

    // Timing information
    std::chrono::milliseconds computation_time;
    std::chrono::steady_clock::time_point analysis_timestamp;

    // Quality indicators
    bool passed_risk_checks;
    std::vector<std::string> warnings;
    std::vector<std::string> recommendations;
};

/**
 * @brief High-performance portfolio analysis suite
 *
 * This class provides a comprehensive toolkit for portfolio performance analysis
 * with intelligent caching, risk management, and real-time monitoring capabilities.
 * Designed for high-frequency trading environments where performance is critical.
 */
class PerformanceAnalysisSuite {
  private:
    std::unique_ptr<CachedPerformanceCalculator> cache_calculator_;
    AnalysisConfig config_;

    // Performance tracking
    mutable std::chrono::steady_clock::time_point last_analysis_time_;
    mutable size_t total_analyses_performed_{0};
    mutable std::chrono::nanoseconds total_computation_time_{0};

    /**
     * @brief Calculate risk-adjusted metrics
     */
    template <typename T>
    Result<void> calculate_risk_metrics(const TimeSeries<T>& returns, AnalysisReport& report) const {
        // Calculate VaR and CVaR
        auto sorted_returns = returns.values();
        std::sort(sorted_returns.begin(), sorted_returns.end());

        if (!sorted_returns.empty()) {
            size_t var_index = static_cast<size_t>(sorted_returns.size() * 0.05);
            report.var_95    = sorted_returns[var_index];

            // CVaR is the mean of returns below VaR
            double cvar_sum = 0.0;
            for (size_t i = 0; i <= var_index; ++i) {
                cvar_sum += sorted_returns[i];
            }
            report.cvar_95 = cvar_sum / (var_index + 1);
        }

        // Calculate downside deviation
        auto mean_result = cache_calculator_->mean(returns);
        if (mean_result.is_error())
            return Result<void>::error(mean_result.error().code, mean_result.error().message);

        double mean_return    = mean_result.value();
        double downside_sum   = 0.0;
        size_t downside_count = 0;

        for (size_t i = 0; i < returns.size(); ++i) {
            if (returns[i] < mean_return) {
                double deviation = returns[i] - mean_return;
                downside_sum += deviation * deviation;
                ++downside_count;
            }
        }

        report.downside_deviation = downside_count > 0 ? std::sqrt(downside_sum / downside_count) : 0.0;

        return Result<void>::success();
    }

    /**
     * @brief Calculate rolling metrics efficiently
     */
    template <typename T>
    Result<void> calculate_rolling_metrics(const TimeSeries<T>& returns, AnalysisReport& report) const {
        for (size_t window : config_.rolling_windows) {
            if (window < returns.size()) {
                // Use cached rolling calculations
                auto rolling_mean_result = cache_calculator_->rolling_mean(returns, window);
                if (rolling_mean_result.is_ok()) {
                    report.rolling_returns[window] = rolling_mean_result.value();
                }

                auto rolling_std_result = cache_calculator_->rolling_std(returns, window);
                if (rolling_std_result.is_ok()) {
                    report.rolling_volatility[window] = rolling_std_result.value();

                    // Calculate rolling Sharpe ratio
                    const auto& vol_series = rolling_std_result.value();
                    const auto& ret_series = report.rolling_returns[window];

                    if (vol_series.size() == ret_series.size()) {
                        std::vector<DateTime> dates = vol_series.timestamps();
                        std::vector<double> sharpe_values;

                        double daily_rf = config_.risk_free_rate / config_.periods_per_year;

                        for (size_t i = 0; i < vol_series.size(); ++i) {
                            if (vol_series[i] > 0) {
                                double excess_return = ret_series[i] - daily_rf;
                                sharpe_values.push_back(excess_return / vol_series[i]);
                            } else {
                                sharpe_values.push_back(0.0);
                            }
                        }

                        report.rolling_sharpe[window] =
                            TimeSeries<double>(dates, sharpe_values, "rolling_sharpe_" + std::to_string(window));
                    }
                }
            }
        }

        return Result<void>::success();
    }

    /**
     * @brief Perform risk checks and generate recommendations
     */
    void perform_risk_analysis(AnalysisReport& report) const {
        report.passed_risk_checks = true;
        report.warnings.clear();
        report.recommendations.clear();

        // Check Sharpe ratio
        if (report.sharpe_ratio < config_.min_sharpe_threshold) {
            report.passed_risk_checks = false;
            report.warnings.push_back("Low Sharpe ratio: " + std::to_string(report.sharpe_ratio));
            report.recommendations.push_back("Consider reducing volatility or improving return generation");
        }

        // Check maximum drawdown
        if (report.max_drawdown > config_.max_drawdown_threshold) {
            report.passed_risk_checks = false;
            report.warnings.push_back("High maximum drawdown: " + std::to_string(report.max_drawdown * 100) + "%");
            report.recommendations.push_back("Implement stronger risk controls or position sizing");
        }

        // Check annual return
        if (report.annual_return < config_.min_return_threshold) {
            report.warnings.push_back("Low annual return: " + std::to_string(report.annual_return * 100) + "%");
            report.recommendations.push_back("Review strategy performance and consider optimization");
        }

        // Check volatility vs return relationship
        double risk_adjusted_return = report.annual_return / report.annual_volatility;
        if (risk_adjusted_return < 0.5) {
            report.warnings.push_back("Poor risk-adjusted return ratio");
            report.recommendations.push_back("Focus on reducing volatility while maintaining returns");
        }

        // Check skewness and kurtosis for tail risks
        if (report.skewness < -1.0) {
            report.warnings.push_back("Negative skewness indicates tail risk");
            report.recommendations.push_back("Consider tail risk hedging strategies");
        }

        if (report.kurtosis > 5.0) {
            report.warnings.push_back("High kurtosis indicates fat tail risk");
            report.recommendations.push_back("Monitor for extreme events and adjust position sizing");
        }
    }

  public:
    explicit PerformanceAnalysisSuite(AnalysisConfig config = {})
        : cache_calculator_(std::make_unique<CachedPerformanceCalculator>(config.cache_config)),
          config_(std::move(config)), last_analysis_time_(std::chrono::steady_clock::now()) {}

    /**
     * @brief Perform comprehensive performance analysis
     */
    template <typename T>
    [[nodiscard]] Result<AnalysisReport> analyze_performance(
        const TimeSeries<T>& returns, const std::optional<TimeSeries<T>>& benchmark = std::nullopt) const {
        auto start_time = std::chrono::high_resolution_clock::now();
        AnalysisReport report{};
        report.analysis_timestamp = std::chrono::steady_clock::now();

        try {
            // Basic performance metrics using cached calculations
            auto mean_result = cache_calculator_->mean(returns);
            if (mean_result.is_error())
                return Result<AnalysisReport>::error(mean_result.error().code, mean_result.error().message);

            auto std_result = cache_calculator_->std_deviation(returns);
            if (std_result.is_error())
                return Result<AnalysisReport>::error(std_result.error().code, std_result.error().message);

            auto sharpe_result = cache_calculator_->sharpe_ratio(returns, config_.risk_free_rate);
            if (sharpe_result.is_error())
                return Result<AnalysisReport>::error(sharpe_result.error().code, sharpe_result.error().message);

            // Calculate cumulative returns for total return
            auto cumulative_result = returns.cumsum();
            if (cumulative_result.is_error())
                return Result<AnalysisReport>::error(cumulative_result.error().code, cumulative_result.error().message);

            const auto& cum_returns = cumulative_result.value();
            report.total_return     = cum_returns.empty() ? 0.0 : cum_returns[cum_returns.size() - 1];

            // Annualized metrics
            report.annual_return     = mean_result.value() * config_.periods_per_year;
            report.annual_volatility = std_result.value() * std::sqrt(config_.periods_per_year);
            report.sharpe_ratio      = sharpe_result.value();

            // Calculate maximum drawdown using cached function
            auto max_dd_result = cache_calculator_->max_drawdown(cum_returns);
            if (max_dd_result.is_error())
                return Result<AnalysisReport>::error(max_dd_result.error().code, max_dd_result.error().message);
            report.max_drawdown = max_dd_result.value();

            // Calmar ratio
            report.calmar_ratio = report.max_drawdown > 0 ? report.annual_return / report.max_drawdown : 0.0;

            // Risk metrics
            auto risk_result = calculate_risk_metrics(returns, report);
            if (risk_result.is_error())
                return Result<AnalysisReport>::error(risk_result.error().code, risk_result.error().message);

            // Statistical moments
            const auto& values = returns.values();
            if (!values.empty()) {
                double mean_val = mean_result.value();
                double variance = 0.0, skew_sum = 0.0, kurt_sum = 0.0;

                for (double val : values) {
                    double diff  = val - mean_val;
                    double diff2 = diff * diff;
                    variance += diff2;
                    skew_sum += diff2 * diff;
                    kurt_sum += diff2 * diff2;
                }

                variance /= values.size();
                double std_dev = std::sqrt(variance);

                if (std_dev > 0) {
                    report.skewness = (skew_sum / values.size()) / (std_dev * std_dev * std_dev);
                    report.kurtosis = (kurt_sum / values.size()) / (variance * variance) - 3.0;
                }
            }

            // Sortino ratio (downside deviation-adjusted)
            report.sortino_ratio = report.downside_deviation > 0
                                       ? (report.annual_return - config_.risk_free_rate) /
                                             (report.downside_deviation * std::sqrt(config_.periods_per_year))
                                       : 0.0;

            // Rolling metrics
            auto rolling_result = calculate_rolling_metrics(returns, report);
            if (rolling_result.is_error())
                return Result<AnalysisReport>::error(rolling_result.error().code, rolling_result.error().message);

            // Benchmark comparison if provided
            if (benchmark) {
                auto benchmark_corr = cache_calculator_->correlation(returns, *benchmark);
                if (benchmark_corr.is_ok()) {
                    auto benchmark_mean = cache_calculator_->mean(*benchmark);
                    auto benchmark_std  = cache_calculator_->std_deviation(*benchmark);

                    if (benchmark_mean.is_ok() && benchmark_std.is_ok()) {
                        // Beta calculation
                        double covariance         = benchmark_corr.value() * std_result.value() * benchmark_std.value();
                        double benchmark_variance = benchmark_std.value() * benchmark_std.value();
                        report.beta               = benchmark_variance > 0 ? covariance / benchmark_variance : 0.0;

                        // Alpha calculation
                        double benchmark_annual_return = benchmark_mean.value() * config_.periods_per_year;
                        report.alpha                   = report.annual_return -
                                       (config_.risk_free_rate +
                                        report.beta.value() * (benchmark_annual_return - config_.risk_free_rate));

                        // Information ratio and tracking error
                        std::vector<double> excess_returns;
                        for (size_t i = 0; i < std::min(returns.size(), benchmark->size()); ++i) {
                            excess_returns.push_back(returns[i] - (*benchmark)[i]);
                        }

                        if (!excess_returns.empty()) {
                            TimeSeries<double> excess_ts(
                                std::vector<DateTime>(returns.timestamps().begin(),
                                                      returns.timestamps().begin() + excess_returns.size()),
                                excess_returns, "excess_returns");

                            auto excess_std  = cache_calculator_->std_deviation(excess_ts);
                            auto excess_mean = cache_calculator_->mean(excess_ts);

                            if (excess_std.is_ok() && excess_mean.is_ok()) {
                                report.tracking_error    = excess_std.value() * std::sqrt(config_.periods_per_year);
                                report.information_ratio = report.tracking_error.value() > 0
                                                               ? (excess_mean.value() * config_.periods_per_year) /
                                                                     report.tracking_error.value()
                                                               : 0.0;
                            }
                        }
                    }
                }
            }

            // Performance analysis and risk checks
            perform_risk_analysis(report);

            // Cache statistics
            report.cache_stats = cache_calculator_->get_cache_stats();

            // Timing information
            auto end_time           = std::chrono::high_resolution_clock::now();
            report.computation_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

            // Update performance tracking
            ++total_analyses_performed_;
            total_computation_time_ += std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
            last_analysis_time_ = std::chrono::steady_clock::now();

            return Result<AnalysisReport>::success(std::move(report));

        } catch (const std::exception& e) {
            return Result<AnalysisReport>::error(ErrorCode::CalculationError,
                                                 "Analysis failed: " + std::string(e.what()));
        }
    }

    /**
     * @brief Get performance statistics for the analysis suite itself
     */
    struct SuitePerformanceStats {
        size_t total_analyses;
        std::chrono::nanoseconds total_computation_time;
        double average_analysis_time_ms;
        CachedPerformanceCalculator::CacheStats cache_stats;
        std::chrono::steady_clock::time_point last_analysis_time;
    };

    SuitePerformanceStats get_performance_stats() const {
        double avg_time_ms = total_analyses_performed_ > 0 ? static_cast<double>(total_computation_time_.count()) /
                                                                 (total_analyses_performed_ * 1000000.0)
                                                           : 0.0;

        return SuitePerformanceStats{.total_analyses           = total_analyses_performed_,
                                     .total_computation_time   = total_computation_time_,
                                     .average_analysis_time_ms = avg_time_ms,
                                     .cache_stats              = cache_calculator_->get_cache_stats(),
                                     .last_analysis_time       = last_analysis_time_};
    }

    /**
     * @brief Update configuration
     */
    void update_config(const AnalysisConfig& new_config) {
        config_ = new_config;
        cache_calculator_->update_config(new_config.cache_config);
    }

    /**
     * @brief Clear all caches
     */
    void clear_cache() { cache_calculator_->clear_cache(); }

    /**
     * @brief Get current configuration
     */
    const AnalysisConfig& get_config() const { return config_; }
};

/**
 * @brief Global performance analysis suite instance
 */
inline PerformanceAnalysisSuite& get_global_analysis_suite() {
    static PerformanceAnalysisSuite global_suite;
    return global_suite;
}

/**
 * @brief Convenience function for quick performance analysis
 */
template <typename T>
[[nodiscard]] Result<AnalysisReport> analyze_portfolio_performance(
    const TimeSeries<T>& returns, const std::optional<TimeSeries<T>>& benchmark = std::nullopt) {
    return get_global_analysis_suite().analyze_performance(returns, benchmark);
}

}  // namespace pyfolio::analytics