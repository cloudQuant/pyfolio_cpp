#pragma once

#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../core/types.h"
#include "returns.h"
#include <algorithm>
#include <limits>

namespace pyfolio::performance {

/**
 * @brief Drawdown analysis result
 */
struct DrawdownInfo {
    double max_drawdown;
    DateTime start_date;
    DateTime end_date;
    DateTime recovery_date;
    int duration_days;
    int recovery_days;
    bool recovered;
};

/**
 * @brief Calculate drawdown series from returns
 */
inline Result<ReturnSeries> calculate_drawdowns(const ReturnSeries& returns) {
    if (returns.empty()) {
        return Result<ReturnSeries>::error(ErrorCode::InsufficientData,
                                           "Cannot calculate drawdowns for empty return series");
    }

    // Calculate cumulative returns starting from 1.0
    auto cumulative_result = calculate_cumulative_returns(returns, 1.0);
    if (cumulative_result.is_error()) {
        return Result<ReturnSeries>::error(cumulative_result.error().code, cumulative_result.error().message);
    }

    const auto& cumulative_returns = cumulative_result.value();

    std::vector<Return> drawdowns;
    drawdowns.reserve(cumulative_returns.size());

    double running_max = 1.0;  // Starting value

    for (const auto& cum_ret : cumulative_returns) {
        double current_value = 1.0 + cum_ret;

        if (current_value > running_max) {
            running_max = current_value;
        }

        // Calculate drawdown as percentage from peak
        double drawdown = (running_max - current_value) / running_max;
        drawdowns.push_back(drawdown);
    }

    return Result<ReturnSeries>::success(
        ReturnSeries{cumulative_returns.timestamps(), std::move(drawdowns), returns.name() + "_drawdowns"});
}

/**
 * @brief Calculate maximum drawdown
 */
inline Result<double> max_drawdown(const ReturnSeries& returns) {
    auto drawdowns_result = calculate_drawdowns(returns);
    if (drawdowns_result.is_error()) {
        return Result<double>::error(drawdowns_result.error().code, drawdowns_result.error().message);
    }

    const auto& drawdowns = drawdowns_result.value();

    if (drawdowns.empty()) {
        return Result<double>::error(ErrorCode::InsufficientData, "No drawdown data available");
    }

    double max_dd = *std::max_element(drawdowns.begin(), drawdowns.end());

    return Result<double>::success(max_dd);
}

/**
 * @brief Calculate detailed drawdown information
 */
inline Result<DrawdownInfo> max_drawdown_info(const ReturnSeries& returns) {
    if (returns.empty()) {
        return Result<DrawdownInfo>::error(ErrorCode::InsufficientData,
                                           "Cannot calculate drawdown info for empty return series");
    }

    auto cumulative_result = calculate_cumulative_returns(returns, 1.0);
    if (cumulative_result.is_error()) {
        return Result<DrawdownInfo>::error(cumulative_result.error().code, cumulative_result.error().message);
    }

    const auto& cumulative_returns = cumulative_result.value();

    double running_max        = 1.0;
    double max_drawdown_value = 0.0;

    size_t peak_index     = 0;
    size_t trough_index   = 0;
    size_t recovery_index = 0;
    bool found_recovery   = false;

    for (size_t i = 0; i < cumulative_returns.size(); ++i) {
        double current_value = 1.0 + cumulative_returns[i];

        if (current_value > running_max) {
            running_max = current_value;
            peak_index  = i;

            // Check if we've recovered from previous drawdown
            if (!found_recovery && max_drawdown_value > 0.0) {
                recovery_index = i;
                found_recovery = true;
            }
        }

        double drawdown = (running_max - current_value) / running_max;

        if (drawdown > max_drawdown_value) {
            max_drawdown_value = drawdown;
            trough_index       = i;
            found_recovery     = false;  // Reset recovery flag for new max drawdown
        }
    }

    // Check if recovery occurred after the maximum drawdown
    if (!found_recovery && max_drawdown_value > 0.0) {
        double peak_value = 1.0 + cumulative_returns[peak_index];

        for (size_t i = trough_index + 1; i < cumulative_returns.size(); ++i) {
            double current_value = 1.0 + cumulative_returns[i];
            if (current_value >= peak_value) {
                recovery_index = i;
                found_recovery = true;
                break;
            }
        }
    }

    DrawdownInfo info;
    info.max_drawdown = max_drawdown_value;
    info.start_date   = returns.timestamp(peak_index);
    info.end_date     = returns.timestamp(trough_index);
    info.recovered    = found_recovery;

    if (found_recovery) {
        info.recovery_date = returns.timestamp(recovery_index);
        info.recovery_days = static_cast<int>(recovery_index - trough_index);
    } else {
        info.recovery_date = DateTime::now();  // Placeholder
        info.recovery_days = -1;               // Not recovered
    }

    info.duration_days = static_cast<int>(trough_index - peak_index);

    return Result<DrawdownInfo>::success(info);
}

/**
 * @brief Calculate drawdown duration (underwater periods)
 */
inline Result<ReturnSeries> drawdown_duration(const ReturnSeries& returns) {
    if (returns.empty()) {
        return Result<ReturnSeries>::error(ErrorCode::InsufficientData,
                                           "Cannot calculate drawdown duration for empty return series");
    }

    auto cumulative_result = calculate_cumulative_returns(returns, 1.0);
    if (cumulative_result.is_error()) {
        return Result<ReturnSeries>::error(cumulative_result.error().code, cumulative_result.error().message);
    }

    const auto& cumulative_returns = cumulative_result.value();

    std::vector<Return> durations;
    durations.reserve(cumulative_returns.size());

    double running_max  = 1.0;
    int underwater_days = 0;

    for (const auto& cum_ret : cumulative_returns) {
        double current_value = 1.0 + cum_ret;

        if (current_value >= running_max) {
            running_max     = current_value;
            underwater_days = 0;  // Reset counter at new peak
        } else {
            underwater_days++;  // Increment days underwater
        }

        durations.push_back(static_cast<double>(underwater_days));
    }

    return Result<ReturnSeries>::success(
        ReturnSeries{cumulative_returns.timestamps(), std::move(durations), returns.name() + "_underwater_duration"});
}

/**
 * @brief Find all drawdown periods
 */
inline Result<std::vector<DrawdownInfo>> find_drawdown_periods(const ReturnSeries& returns,
                                                               double min_drawdown = 0.01) {
    if (returns.empty()) {
        return Result<std::vector<DrawdownInfo>>::error(ErrorCode::InsufficientData,
                                                        "Cannot find drawdown periods for empty return series");
    }

    auto cumulative_result = calculate_cumulative_returns(returns, 1.0);
    if (cumulative_result.is_error()) {
        return Result<std::vector<DrawdownInfo>>::error(cumulative_result.error().code,
                                                        cumulative_result.error().message);
    }

    const auto& cumulative_returns = cumulative_result.value();

    std::vector<DrawdownInfo> drawdown_periods;

    double running_max         = 1.0;
    bool in_drawdown           = false;
    size_t peak_index          = 0;
    size_t trough_index        = 0;
    double period_max_drawdown = 0.0;

    for (size_t i = 0; i < cumulative_returns.size(); ++i) {
        double current_value = 1.0 + cumulative_returns[i];

        if (current_value > running_max) {
            // New peak reached
            if (in_drawdown && period_max_drawdown >= min_drawdown) {
                // End of significant drawdown period
                DrawdownInfo info;
                info.max_drawdown  = period_max_drawdown;
                info.start_date    = returns.timestamp(peak_index);
                info.end_date      = returns.timestamp(trough_index);
                info.recovery_date = returns.timestamp(i);
                info.duration_days = static_cast<int>(trough_index - peak_index);
                info.recovery_days = static_cast<int>(i - trough_index);
                info.recovered     = true;

                drawdown_periods.push_back(info);
            }

            running_max         = current_value;
            peak_index          = i;
            in_drawdown         = false;
            period_max_drawdown = 0.0;
        } else {
            // Below peak - in drawdown
            double drawdown = (running_max - current_value) / running_max;

            if (!in_drawdown && drawdown >= min_drawdown) {
                in_drawdown = true;
            }

            if (drawdown > period_max_drawdown) {
                period_max_drawdown = drawdown;
                trough_index        = i;
            }
        }
    }

    // Handle case where series ends in drawdown
    if (in_drawdown && period_max_drawdown >= min_drawdown) {
        DrawdownInfo info;
        info.max_drawdown  = period_max_drawdown;
        info.start_date    = returns.timestamp(peak_index);
        info.end_date      = returns.timestamp(trough_index);
        info.recovery_date = DateTime::now();  // Not recovered
        info.duration_days = static_cast<int>(trough_index - peak_index);
        info.recovery_days = -1;
        info.recovered     = false;

        drawdown_periods.push_back(info);
    }

    return Result<std::vector<DrawdownInfo>>::success(std::move(drawdown_periods));
}

/**
 * @brief Calculate average drawdown
 */
inline Result<double> average_drawdown(const ReturnSeries& returns) {
    auto drawdowns_result = calculate_drawdowns(returns);
    if (drawdowns_result.is_error()) {
        return Result<double>::error(drawdowns_result.error().code, drawdowns_result.error().message);
    }

    const auto& drawdowns = drawdowns_result.value();

    if (drawdowns.empty()) {
        return Result<double>::error(ErrorCode::InsufficientData, "No drawdown data available");
    }

    // Only include actual drawdown periods (non-zero values)
    std::vector<double> non_zero_drawdowns;
    for (const auto& dd : drawdowns) {
        if (dd > 0.0) {
            non_zero_drawdowns.push_back(dd);
        }
    }

    if (non_zero_drawdowns.empty()) {
        return Result<double>::success(0.0);
    }

    double sum     = std::accumulate(non_zero_drawdowns.begin(), non_zero_drawdowns.end(), 0.0);
    double average = sum / non_zero_drawdowns.size();

    return Result<double>::success(average);
}

/**
 * @brief Calculate rolling maximum drawdown
 */
inline Result<ReturnSeries> rolling_max_drawdown(const ReturnSeries& returns, size_t window_size) {
    if (window_size == 0 || window_size > returns.size()) {
        return Result<ReturnSeries>::error(ErrorCode::InvalidInput, "Invalid window size for rolling maximum drawdown");
    }

    std::vector<Return> rolling_max_drawdowns;
    std::vector<DateTime> rolling_timestamps;

    rolling_max_drawdowns.reserve(returns.size() - window_size + 1);
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

        auto max_dd_result = max_drawdown(window_series);
        if (max_dd_result.is_error()) {
            rolling_max_drawdowns.push_back(std::numeric_limits<double>::quiet_NaN());
        } else {
            rolling_max_drawdowns.push_back(max_dd_result.value());
        }

        rolling_timestamps.push_back(returns.timestamp(i));
    }

    return Result<ReturnSeries>::success(ReturnSeries{std::move(rolling_timestamps), std::move(rolling_max_drawdowns),
                                                      returns.name() + "_rolling_max_drawdown"});
}

/**
 * @brief Calculate time to recovery from drawdowns
 */
inline Result<double> average_recovery_time(const ReturnSeries& returns, double min_drawdown = 0.01) {
    auto periods_result = find_drawdown_periods(returns, min_drawdown);
    if (periods_result.is_error()) {
        return Result<double>::error(periods_result.error().code, periods_result.error().message);
    }

    const auto& periods = periods_result.value();

    if (periods.empty()) {
        return Result<double>::success(0.0);
    }

    std::vector<int> recovery_times;
    for (const auto& period : periods) {
        if (period.recovered && period.recovery_days > 0) {
            recovery_times.push_back(period.recovery_days);
        }
    }

    if (recovery_times.empty()) {
        return Result<double>::error(ErrorCode::InsufficientData, "No recovered drawdown periods found");
    }

    double sum     = std::accumulate(recovery_times.begin(), recovery_times.end(), 0.0);
    double average = sum / recovery_times.size();

    return Result<double>::success(average);
}

}  // namespace pyfolio::performance