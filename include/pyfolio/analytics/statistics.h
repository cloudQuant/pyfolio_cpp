#pragma once

/**
 * @file statistics.h
 * @brief Statistical analysis utilities
 * @version 1.0.0
 * @date 2024
 *
 * This file provides statistical functions that complement the core
 * math/statistics module for analytics purposes.
 */

#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../core/types.h"
#include "../math/statistics.h"

namespace pyfolio::analytics::statistics {

/**
 * @brief Statistical summary for a time series
 */
struct StatisticalSummary {
    double mean;
    double median;
    double std_dev;
    double variance;
    double skewness;
    double kurtosis;
    double min_value;
    double max_value;
    double q25;  // 25th percentile
    double q75;  // 75th percentile
    size_t count;
    size_t missing_count;
};

/**
 * @brief Calculate comprehensive statistical summary
 */
template <typename T>
Result<StatisticalSummary> calculate_summary(const TimeSeries<T>& series) {
    if (series.empty()) {
        return Result<StatisticalSummary>::error(ErrorCode::InvalidInput, "Empty time series");
    }

    auto values = series.values();
    std::span<const T> data_span(values);

    StatisticalSummary summary;

    // Basic statistics
    auto mean_result = stats::mean(data_span);
    if (!mean_result.has_value()) {
        return Result<StatisticalSummary>::error(ErrorCode::CalculationError,
                                                 "Failed to calculate mean: " + mean_result.error().message);
    }
    summary.mean = mean_result.value();

    auto variance_result = stats::variance(data_span);
    if (!variance_result.has_value()) {
        // For single value, variance and std dev are 0
        summary.variance = 0.0;
        summary.std_dev  = 0.0;
    } else {
        summary.variance = variance_result.value();
        summary.std_dev  = std::sqrt(summary.variance);
    }

    auto skew_result = stats::skewness(data_span);
    if (!skew_result.has_value()) {
        // For single value, skewness is undefined but set to 0
        summary.skewness = 0.0;
    } else {
        summary.skewness = skew_result.value();
    }

    auto kurt_result = stats::kurtosis(data_span);
    if (!kurt_result.has_value()) {
        // For single value, kurtosis is undefined but set to 0
        summary.kurtosis = 0.0;
    } else {
        summary.kurtosis = kurt_result.value();
    }

    // Min/max
    auto minmax       = std::minmax_element(values.begin(), values.end());
    summary.min_value = *minmax.first;
    summary.max_value = *minmax.second;

    // Percentiles
    std::vector<T> sorted_values = values;
    std::sort(sorted_values.begin(), sorted_values.end());

    size_t n       = sorted_values.size();
    summary.median = sorted_values[n / 2];
    summary.q25    = sorted_values[n / 4];
    summary.q75    = sorted_values[3 * n / 4];

    summary.count         = n;
    summary.missing_count = 0;  // Assuming no missing values for now

    return Result<StatisticalSummary>::success(summary);
}

/**
 * @brief Distribution analysis
 */
struct DistributionAnalysis {
    double jarque_bera_statistic;
    double jarque_bera_p_value;
    bool is_normal;
    double anderson_darling_statistic;
    double kolmogorov_smirnov_statistic;
    std::vector<double> histogram_bins;
    std::vector<double> histogram_counts;
};

/**
 * @brief Analyze distribution properties
 */
template <typename T>
Result<DistributionAnalysis> analyze_distribution(const TimeSeries<T>& series) {
    if (series.empty()) {
        return Error("Empty time series");
    }

    DistributionAnalysis analysis;
    auto values = series.values();

    // Calculate Jarque-Bera test for normality
    auto summary_result = calculate_summary(series);
    if (!summary_result.has_value()) {
        return Error("Failed to calculate summary statistics");
    }

    auto summary = summary_result.value();

    // Jarque-Bera statistic
    double n = static_cast<double>(values.size());
    analysis.jarque_bera_statistic =
        (n / 6.0) * (std::pow(summary.skewness, 2) + std::pow(summary.kurtosis - 3.0, 2) / 4.0);

    // Simple p-value approximation (chi-square with 2 df)
    analysis.jarque_bera_p_value = 1.0 - std::erf(analysis.jarque_bera_statistic / 2.0);
    analysis.is_normal           = analysis.jarque_bera_p_value > 0.05;

    // Create histogram
    int n_bins = static_cast<int>(std::sqrt(n));
    analysis.histogram_bins.resize(n_bins + 1);
    analysis.histogram_counts.resize(n_bins);

    double bin_width = (summary.max_value - summary.min_value) / n_bins;
    for (int i = 0; i <= n_bins; ++i) {
        analysis.histogram_bins[i] = summary.min_value + i * bin_width;
    }

    // Count values in each bin
    std::fill(analysis.histogram_counts.begin(), analysis.histogram_counts.end(), 0.0);
    for (const auto& value : values) {
        int bin = static_cast<int>((value - summary.min_value) / bin_width);
        if (bin >= n_bins)
            bin = n_bins - 1;
        if (bin >= 0)
            analysis.histogram_counts[bin]++;
    }

    return analysis;
}

/**
 * @brief Time series correlation analysis
 */
struct CorrelationAnalysis {
    double pearson_correlation;
    double spearman_correlation;
    double kendall_tau;
    double r_squared;
    std::vector<double> rolling_correlations;
    size_t window_size;
};

/**
 * @brief Calculate correlation analysis between two series
 */
template <typename T1, typename T2>
Result<CorrelationAnalysis> analyze_correlation(const TimeSeries<T1>& series1, const TimeSeries<T2>& series2,
                                                size_t rolling_window = 252) {
    if (series1.empty() || series2.empty()) {
        return Error("Empty time series");
    }

    CorrelationAnalysis analysis;
    analysis.window_size = rolling_window;

    // Align series by dates
    auto aligned = align_series(series1, series2);
    if (aligned.first.empty() || aligned.second.empty()) {
        return Error("No overlapping dates between series");
    }

    auto values1 = aligned.first.values();
    auto values2 = aligned.second.values();

    std::span<const T1> span1(values1);
    std::span<const T2> span2(values2);

    // Pearson correlation
    auto corr_result = stats::correlation(span1, span2);
    if (!corr_result.has_value()) {
        return Error("Failed to calculate correlation");
    }
    analysis.pearson_correlation = corr_result.value();
    analysis.r_squared           = analysis.pearson_correlation * analysis.pearson_correlation;

    // Simplified Spearman and Kendall (would need ranking functions)
    analysis.spearman_correlation = analysis.pearson_correlation;        // Approximation
    analysis.kendall_tau          = analysis.pearson_correlation * 0.8;  // Approximation

    // Rolling correlations
    if (values1.size() >= rolling_window) {
        for (size_t i = rolling_window; i <= values1.size(); ++i) {
            std::span<const T1> window1(values1.data() + i - rolling_window, rolling_window);
            std::span<const T2> window2(values2.data() + i - rolling_window, rolling_window);

            auto window_corr = stats::correlation(window1, window2);
            if (window_corr.has_value()) {
                analysis.rolling_correlations.push_back(window_corr.value());
            } else {
                analysis.rolling_correlations.push_back(0.0);
            }
        }
    }

    return analysis;
}

/**
 * @brief Helper function to align two time series by dates
 */
template <typename T1, typename T2>
std::pair<TimeSeries<T1>, TimeSeries<T2>> align_series(const TimeSeries<T1>& series1, const TimeSeries<T2>& series2) {
    TimeSeries<T1> aligned1;
    TimeSeries<T2> aligned2;

    auto dates1  = series1.timestamps();
    auto dates2  = series2.timestamps();
    auto values1 = series1.values();
    auto values2 = series2.values();

    // Simple intersection approach
    for (size_t i = 0; i < dates1.size(); ++i) {
        for (size_t j = 0; j < dates2.size(); ++j) {
            if (dates1[i] == dates2[j]) {
                aligned1.push_back(dates1[i], values1[i]);
                aligned2.push_back(dates2[j], values2[j]);
                break;
            }
        }
    }

    return {aligned1, aligned2};
}

}  // namespace pyfolio::analytics::statistics

// Statistics class wrapper for test compatibility
namespace pyfolio {

/**
 * @brief Wrapper class to provide static methods expected by tests
 */
class Statistics {
  public:
    // Basic statistics
    template <typename T>
    static Result<analytics::statistics::StatisticalSummary> calculate_basic_stats(const TimeSeries<T>& series) {
        if (series.empty()) {
            return Result<analytics::statistics::StatisticalSummary>::error(ErrorCode::InvalidInput,
                                                                            "Empty time series");
        }
        return analytics::statistics::calculate_summary(series);
    }

    // Sharpe ratio calculation
    template <typename T>
    static Result<double> sharpe_ratio(const TimeSeries<T>& returns, double risk_free_rate = 0.0) {
        auto mean_result = returns.mean();
        auto std_result  = returns.std();
        if (mean_result.is_error() || std_result.is_error()) {
            return Result<double>::error(ErrorCode::CalculationError, "Failed to calculate Sharpe ratio");
        }
        // Check for zero standard deviation
        if (std_result.value() == 0.0) {
            return Result<double>::error(ErrorCode::CalculationError,
                                         "Zero standard deviation in Sharpe ratio calculation");
        }

        // Annualize returns and volatility (assuming daily data)
        double annualized_return = mean_result.value() * 252.0;
        double annualized_std    = std_result.value() * std::sqrt(252.0);
        double excess_return     = annualized_return - risk_free_rate;
        double sharpe            = excess_return / annualized_std;
        return Result<double>::success(sharpe);
    }

    // Sortino ratio calculation
    template <typename T>
    static Result<double> sortino_ratio(const TimeSeries<T>& returns, double risk_free_rate = 0.0) {
        auto mean_result = returns.mean();
        if (mean_result.is_error()) {
            return Result<double>::error(ErrorCode::CalculationError, "Failed to calculate Sortino ratio");
        }

        // Convert annual risk-free rate to daily
        double daily_rf = risk_free_rate / 252.0;

        // Calculate downside deviation
        auto values            = returns.values();
        double sum_negative_sq = 0.0;
        int negative_count     = 0;
        for (const auto& ret : values) {
            if (ret < daily_rf) {
                double diff = ret - daily_rf;
                sum_negative_sq += diff * diff;
                negative_count++;
            }
        }

        if (negative_count == 0) {
            return Result<double>::success(std::numeric_limits<double>::infinity());
        }

        double downside_dev = std::sqrt(sum_negative_sq / negative_count);
        // Annualize the downside deviation
        double annualized_downside_dev = downside_dev * std::sqrt(252.0);
        // Annualize the returns
        double annualized_return = mean_result.value() * 252.0;
        double excess_return     = annualized_return - risk_free_rate;
        double sortino           = excess_return / annualized_downside_dev;
        return Result<double>::success(sortino);
    }

    // Calmar ratio calculation
    template <typename T>
    static Result<double> calmar_ratio(const TimeSeries<T>& returns) {
        auto annual_return = returns.mean();
        if (annual_return.is_error()) {
            return Result<double>::error(ErrorCode::CalculationError, "Failed to calculate Calmar ratio");
        }

        // Simple max drawdown calculation
        auto cumulative_result = returns.cumulative_returns();
        if (cumulative_result.is_error()) {
            return cumulative_result.map([](const auto&) { return 0.0; });
        }

        auto cum_values     = cumulative_result.value().values();
        double max_value    = 0.0;
        double max_drawdown = 0.0;

        for (const auto& val : cum_values) {
            if (val > max_value) {
                max_value = val;
            }
            double drawdown = (val - max_value) / max_value;
            if (drawdown < max_drawdown) {
                max_drawdown = drawdown;
            }
        }

        if (max_drawdown == 0.0) {
            return Result<double>::success(std::numeric_limits<double>::infinity());
        }

        double calmar = annual_return.value() / std::abs(max_drawdown);
        return Result<double>::success(calmar);
    }

    // Simple drawdown info struct
    struct SimpleDrawdownInfo {
        double max_drawdown;
        int duration_days;
        DateTime peak_date   = DateTime::now();
        DateTime valley_date = DateTime::now();
    };

    // Alpha/Beta result struct
    struct AlphaBetaResult {
        double alpha;
        double beta;
        double r_squared = 0.0;
    };

    // Maximum drawdown calculation
    template <typename T>
    static Result<SimpleDrawdownInfo> max_drawdown(const TimeSeries<T>& returns) {
        auto cumulative_result = returns.cumulative_returns();
        if (cumulative_result.is_error()) {
            return Result<SimpleDrawdownInfo>::error(cumulative_result.error().code, cumulative_result.error().message);
        }

        auto cum_values = cumulative_result.value().values();
        if (cum_values.empty()) {
            return Result<SimpleDrawdownInfo>::error(ErrorCode::InvalidInput, "Empty cumulative returns");
        }

        // Start with first value as peak (cumulative returns usually start at 1.0)
        double max_value     = std::max(1.0, cum_values[0]);
        double max_drawdown  = 0.0;
        int max_duration     = 0;
        int current_duration = 0;

        for (const auto& val : cum_values) {
            if (val > max_value) {
                max_value        = val;
                current_duration = 0;
            } else {
                current_duration++;
                if (current_duration > max_duration) {
                    max_duration = current_duration;
                }
            }

            // Calculate drawdown as percentage drop from peak
            double drawdown = 0.0;
            if (max_value > 0.0) {
                drawdown = (val - max_value) / max_value;
                // Ensure drawdown doesn't exceed -100%
                drawdown = std::max(drawdown, -1.0);
            } else {
                // If max_value is 0, cap at -100%
                drawdown = -1.0;
            }

            if (drawdown < max_drawdown) {
                max_drawdown = drawdown;
            }
        }

        SimpleDrawdownInfo info;
        info.max_drawdown  = max_drawdown;
        info.duration_days = max_duration;
        return Result<SimpleDrawdownInfo>::success(info);
    }

    // Volatility calculation
    template <typename T>
    static Result<double> volatility(const TimeSeries<T>& returns) {
        return returns.std();
    }

    // Downside deviation calculation
    template <typename T>
    static Result<double> downside_deviation(const TimeSeries<T>& returns, double threshold = 0.0) {
        auto values            = returns.values();
        double sum_negative_sq = 0.0;
        int negative_count     = 0;

        for (const auto& ret : values) {
            if (ret < threshold) {
                double diff = ret - threshold;
                sum_negative_sq += diff * diff;
                negative_count++;
            }
        }

        if (negative_count == 0) {
            return Result<double>::success(0.0);
        }

        double downside_dev = std::sqrt(sum_negative_sq / negative_count);
        return Result<double>::success(downside_dev);
    }

    // Alpha and beta calculation
    template <typename T>
    static Result<AlphaBetaResult> alpha_beta(const TimeSeries<T>& returns, const TimeSeries<T>& benchmark_returns,
                                              double risk_free_rate = 0.0) {
        if (returns.size() != benchmark_returns.size()) {
            return Result<AlphaBetaResult>::error(ErrorCode::InvalidInput, "Return series must have same length");
        }

        auto portfolio_values = returns.values();
        auto benchmark_values = benchmark_returns.values();

        // Calculate means
        double portfolio_mean = 0.0;
        double benchmark_mean = 0.0;
        for (size_t i = 0; i < portfolio_values.size(); ++i) {
            portfolio_mean += portfolio_values[i];
            benchmark_mean += benchmark_values[i];
        }
        portfolio_mean /= portfolio_values.size();
        benchmark_mean /= benchmark_values.size();

        // Calculate beta (covariance / variance)
        double covariance         = 0.0;
        double benchmark_variance = 0.0;

        for (size_t i = 0; i < portfolio_values.size(); ++i) {
            double portfolio_diff = portfolio_values[i] - portfolio_mean;
            double benchmark_diff = benchmark_values[i] - benchmark_mean;
            covariance += portfolio_diff * benchmark_diff;
            benchmark_variance += benchmark_diff * benchmark_diff;
        }

        covariance /= portfolio_values.size();
        benchmark_variance /= benchmark_values.size();

        double beta  = (benchmark_variance != 0.0) ? covariance / benchmark_variance : 0.0;
        double alpha = portfolio_mean - risk_free_rate - beta * (benchmark_mean - risk_free_rate);

        AlphaBetaResult result;
        result.alpha = alpha;
        result.beta  = beta;
        return Result<AlphaBetaResult>::success(result);
    }

    // Additional methods that tests expect
    template <typename T>
    static Result<double> information_ratio(const TimeSeries<T>& returns, const TimeSeries<T>& benchmark_returns) {
        // Information ratio = (portfolio_return - benchmark_return) / tracking_error
        if (returns.size() != benchmark_returns.size()) {
            return Result<double>::error(ErrorCode::InvalidInput, "Return series must have same length");
        }

        // Calculate excess returns (portfolio - benchmark)
        auto portfolio_values = returns.values();
        auto benchmark_values = benchmark_returns.values();
        std::vector<T> excess_returns_values;

        for (size_t i = 0; i < portfolio_values.size(); ++i) {
            excess_returns_values.push_back(portfolio_values[i] - benchmark_values[i]);
        }

        // Calculate mean and std of excess returns
        double sum = 0.0;
        for (const auto& val : excess_returns_values) {
            sum += val;
        }
        double mean_excess = sum / excess_returns_values.size();

        double variance = 0.0;
        for (const auto& val : excess_returns_values) {
            double diff = val - mean_excess;
            variance += diff * diff;
        }
        variance /= excess_returns_values.size();
        double std_excess = std::sqrt(variance);

        if (std_excess == 0.0) {
            return Result<double>::error(ErrorCode::CalculationError, "Zero tracking error");
        }

        // Annualize for information ratio
        double annualized_excess_return  = mean_excess * 252.0;
        double annualized_tracking_error = std_excess * std::sqrt(252.0);

        return Result<double>::success(annualized_excess_return / annualized_tracking_error);
    }

    template <typename T>
    static Result<double> tracking_error(const TimeSeries<T>& returns, const TimeSeries<T>& benchmark_returns) {
        // Tracking error is annualized std dev of excess returns
        if (returns.size() != benchmark_returns.size()) {
            return Result<double>::error(ErrorCode::InvalidInput, "Return series must have same length");
        }

        // Calculate excess returns (portfolio - benchmark)
        auto portfolio_values = returns.values();
        auto benchmark_values = benchmark_returns.values();
        std::vector<T> excess_returns_values;

        for (size_t i = 0; i < portfolio_values.size(); ++i) {
            excess_returns_values.push_back(portfolio_values[i] - benchmark_values[i]);
        }

        // Calculate std of excess returns
        double sum = 0.0;
        for (const auto& val : excess_returns_values) {
            sum += val;
        }
        double mean_excess = sum / excess_returns_values.size();

        double variance = 0.0;
        for (const auto& val : excess_returns_values) {
            double diff = val - mean_excess;
            variance += diff * diff;
        }
        variance /= excess_returns_values.size();
        double std_excess = std::sqrt(variance);

        // Annualize tracking error
        double annualized_tracking_error = std_excess * std::sqrt(252.0);

        return Result<double>::success(annualized_tracking_error);
    }

    template <typename T>
    static Result<double> skewness(const TimeSeries<T>& returns) {
        auto values = returns.values();
        return pyfolio::stats::skewness(std::span<const T>(values));
    }

    template <typename T>
    static Result<double> kurtosis(const TimeSeries<T>& returns) {
        auto values = returns.values();
        return pyfolio::stats::kurtosis(std::span<const T>(values));
    }

    template <typename T>
    static Result<double> value_at_risk_historical(const TimeSeries<T>& returns, double confidence_level = 0.05) {
        auto values = returns.values();
        // Convert from confidence level (e.g., 0.05) to confidence level used by implementation (e.g., 0.95)
        double impl_confidence_level = 1.0 - confidence_level;
        return pyfolio::stats::value_at_risk(std::span<const T>(values), impl_confidence_level);
    }

    template <typename T>
    static Result<double> conditional_value_at_risk(const TimeSeries<T>& returns, double confidence_level = 0.05) {
        auto values = returns.values();
        // Convert from confidence level (e.g., 0.05) to confidence level used by implementation (e.g., 0.95)
        double impl_confidence_level = 1.0 - confidence_level;
        return pyfolio::stats::conditional_value_at_risk(std::span<const T>(values), impl_confidence_level);
    }
};

}  // namespace pyfolio