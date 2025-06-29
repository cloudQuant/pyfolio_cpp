#pragma once

#include "../math/simd_math.h"
#include "datetime.h"
#include "error_handling.h"
#include "types.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <map>
#include <numeric>
#include <span>
#include <vector>

namespace pyfolio {

/**
 * @brief High-performance time series container for financial data
 */
template <typename T>
class TimeSeries {
  private:
    std::vector<DateTime> timestamps_;
    std::vector<T> values_;
    std::string name_;

    // Ensure timestamps and values are synchronized
    Result<void> validate_consistency() const {
        if (timestamps_.size() != values_.size()) {
            return Result<void>::error(ErrorCode::InvalidInput, "TimeSeries: timestamps and values size mismatch");
        }
        return Result<void>::success();
    }

    // Helper to throw exception from Result for API compatibility
    void validate_and_throw() const {
        auto validation = validate_consistency();
        if (validation.is_error()) {
            throw std::runtime_error(validation.error().message);
        }
    }

  public:
    using value_type     = T;
    using size_type      = std::size_t;
    using iterator       = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;

    // Constructors
    TimeSeries() = default;

    explicit TimeSeries(const std::string& name) : name_(name) {}

    TimeSeries(const std::vector<DateTime>& timestamps, const std::vector<T>& values, const std::string& name = "")
        : timestamps_(timestamps), values_(values), name_(name) {
        validate_and_throw();  // For API compatibility - throw on validation failure
        sort_by_time();
    }

    TimeSeries(std::vector<DateTime>&& timestamps, std::vector<T>&& values, const std::string& name = "")
        : timestamps_(std::move(timestamps)), values_(std::move(values)), name_(name) {
        validate_and_throw();  // For API compatibility - throw on validation failure
        sort_by_time();
    }

    // Factory methods that return Result<T> - future preferred API
    static Result<TimeSeries<T>> create(const std::vector<DateTime>& timestamps, const std::vector<T>& values,
                                        const std::string& name = "") {
        if (timestamps.size() != values.size()) {
            return Result<TimeSeries<T>>::error(ErrorCode::InvalidInput,
                                                "TimeSeries: timestamps and values size mismatch");
        }

        // Use private constructor that doesn't validate since we already checked
        TimeSeries<T> ts;
        ts.timestamps_ = timestamps;
        ts.values_     = values;
        ts.name_       = name;
        ts.sort_by_time();

        return Result<TimeSeries<T>>::success(std::move(ts));
    }

    static Result<TimeSeries<T>> create(std::vector<DateTime>&& timestamps, std::vector<T>&& values,
                                        const std::string& name = "") {
        if (timestamps.size() != values.size()) {
            return Result<TimeSeries<T>>::error(ErrorCode::InvalidInput,
                                                "TimeSeries: timestamps and values size mismatch");
        }

        TimeSeries<T> ts;
        ts.timestamps_ = std::move(timestamps);
        ts.values_     = std::move(values);
        ts.name_       = name;
        ts.sort_by_time();

        return Result<TimeSeries<T>>::success(std::move(ts));
    }

    // Capacity
    constexpr size_type size() const noexcept { return values_.size(); }
    constexpr bool empty() const noexcept { return values_.empty(); }
    void reserve(size_type n) {
        timestamps_.reserve(n);
        values_.reserve(n);
    }

    // Element access
    constexpr const T& operator[](size_type index) const noexcept { return values_[index]; }
    constexpr T& operator[](size_type index) noexcept { return values_[index]; }

    const T& at(size_type index) const { return values_.at(index); }
    T& at(size_type index) { return values_.at(index); }

    constexpr const T& front() const noexcept { return values_.front(); }
    constexpr T& front() noexcept { return values_.front(); }

    constexpr const T& back() const noexcept { return values_.back(); }
    constexpr T& back() noexcept { return values_.back(); }

    // Time access
    constexpr const DateTime& timestamp(size_type index) const { return timestamps_.at(index); }
    constexpr const std::vector<DateTime>& timestamps() const noexcept { return timestamps_; }
    constexpr const std::vector<T>& values() const noexcept { return values_; }

    // Iterators
    constexpr iterator begin() noexcept { return values_.begin(); }
    constexpr const_iterator begin() const noexcept { return values_.begin(); }
    constexpr const_iterator cbegin() const noexcept { return values_.cbegin(); }

    constexpr iterator end() noexcept { return values_.end(); }
    constexpr const_iterator end() const noexcept { return values_.end(); }
    constexpr const_iterator cend() const noexcept { return values_.cend(); }

    // Metadata
    constexpr const std::string& name() const noexcept { return name_; }
    void set_name(const std::string& name) { name_ = name; }

    /**
     * @brief Add a data point
     */
    void push_back(const DateTime& timestamp, const T& value) {
        timestamps_.push_back(timestamp);
        values_.push_back(value);
    }

    void emplace_back(const DateTime& timestamp, T&& value) {
        timestamps_.push_back(timestamp);
        values_.emplace_back(std::move(value));
    }

    /**
     * @brief Add a data point with validation (Result<T> API)
     */
    [[nodiscard]] Result<void> try_push_back(const DateTime& timestamp, const T& value) {
        timestamps_.push_back(timestamp);
        values_.push_back(value);

        // Validate consistency after adding
        auto validation = validate_consistency();
        if (validation.is_error()) {
            // Rollback the changes
            timestamps_.pop_back();
            values_.pop_back();
            return validation;
        }

        return Result<void>::success();
    }

    /**
     * @brief Bulk insert data points (optimized for multiple additions)
     */
    template <typename TimestampIter, typename ValueIter>
    [[nodiscard]] Result<void> bulk_insert(TimestampIter timestamps_begin, TimestampIter timestamps_end,
                                           ValueIter values_begin, ValueIter values_end) {
        size_type new_count    = std::distance(timestamps_begin, timestamps_end);
        size_type values_count = std::distance(values_begin, values_end);

        if (new_count != values_count) {
            return Result<void>::error(ErrorCode::InvalidInput, "Bulk insert: timestamp and value counts must match");
        }

        // Reserve space to avoid multiple reallocations
        size_type old_size = size();
        timestamps_.reserve(old_size + new_count);
        values_.reserve(old_size + new_count);

        // Insert new data
        timestamps_.insert(timestamps_.end(), timestamps_begin, timestamps_end);
        values_.insert(values_.end(), values_begin, values_end);

        // Sort if needed (only if we added data that might be out of order)
        if (old_size > 0 && new_count > 0) {
            bool needs_sort   = false;
            DateTime last_old = timestamps_[old_size - 1];

            // Check if any new timestamp is earlier than the last old timestamp
            for (auto it = timestamps_begin; it != timestamps_end; ++it) {
                if (*it < last_old) {
                    needs_sort = true;
                    break;
                }
            }

            if (needs_sort) {
                sort_by_time();
            }
        }

        return Result<void>::success();
    }

    /**
     * @brief Clear all data
     */
    void clear() {
        timestamps_.clear();
        values_.clear();
    }

    /**
     * @brief Sort by timestamp (maintains time series integrity) - Optimized
     */
    void sort_by_time() {
        if (empty())
            return;

        // Fast path: check if already sorted
        if (is_sorted_by_time()) {
            return;
        }

        // For small datasets, use simple approach
        if (size() <= 32) {
            sort_by_time_small();
            return;
        }

        // For larger datasets, use optimized index-based sorting
        sort_by_time_optimized();
    }

  private:
    /**
     * @brief Check if time series is already sorted by timestamp
     */
    bool is_sorted_by_time() const noexcept {
        if (size() <= 1)
            return true;

        for (size_type i = 1; i < size(); ++i) {
            if (timestamps_[i - 1] > timestamps_[i]) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Optimized sorting for small datasets using insertion sort
     */
    void sort_by_time_small() {
        // Insertion sort - optimal for small datasets
        for (size_type i = 1; i < size(); ++i) {
            DateTime key_time = std::move(timestamps_[i]);
            T key_value       = std::move(values_[i]);

            size_type j = i;
            while (j > 0 && timestamps_[j - 1] > key_time) {
                timestamps_[j] = std::move(timestamps_[j - 1]);
                values_[j]     = std::move(values_[j - 1]);
                --j;
            }

            timestamps_[j] = std::move(key_time);
            values_[j]     = std::move(key_value);
        }
    }

    /**
     * @brief Optimized sorting for larger datasets
     */
    void sort_by_time_optimized() {
        // Create pairs for sorting to maintain data integrity
        std::vector<std::pair<DateTime, T>> paired_data;
        paired_data.reserve(size());

        // Move data into pairs
        for (size_type i = 0; i < size(); ++i) {
            paired_data.emplace_back(std::move(timestamps_[i]), std::move(values_[i]));
        }

        // Sort by timestamp
        std::sort(paired_data.begin(), paired_data.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });

        // Move data back
        for (size_type i = 0; i < size(); ++i) {
            timestamps_[i] = std::move(paired_data[i].first);
            values_[i]     = std::move(paired_data[i].second);
        }
    }

  public:
    /**
     * @brief Find value at specific timestamp
     */
    [[nodiscard]] Result<T> at_time(const DateTime& timestamp) const {
        auto it = std::lower_bound(timestamps_.begin(), timestamps_.end(), timestamp);

        if (it == timestamps_.end() || *it != timestamp) {
            return Result<T>::error(ErrorCode::MissingData, "No data found for timestamp: " + timestamp.to_string());
        }

        size_type index = std::distance(timestamps_.begin(), it);
        return Result<T>::success(values_[index]);
    }

    /**
     * @brief Get slice of time series between dates
     */
    [[nodiscard]] Result<TimeSeries<T>> slice(const DateTime& start, const DateTime& end) const {
        if (start >= end) {
            return Result<TimeSeries<T>>::error(ErrorCode::InvalidDateRange, "Start date must be before end date");
        }

        auto start_it = std::lower_bound(timestamps_.begin(), timestamps_.end(), start);
        auto end_it   = std::upper_bound(timestamps_.begin(), timestamps_.end(), end);

        if (start_it == timestamps_.end()) {
            return Result<TimeSeries<T>>::error(ErrorCode::MissingData, "No data found in specified date range");
        }

        size_type start_idx = std::distance(timestamps_.begin(), start_it);
        size_type end_idx   = std::distance(timestamps_.begin(), end_it);

        std::vector<DateTime> slice_timestamps(timestamps_.begin() + start_idx, timestamps_.begin() + end_idx);
        std::vector<T> slice_values(values_.begin() + start_idx, values_.begin() + end_idx);

        return Result<TimeSeries<T>>::success(
            TimeSeries<T>{std::move(slice_timestamps), std::move(slice_values), name_ + "_slice"});
    }

    /**
     * @brief Resample to different frequency (with default mean aggregator)
     */
    [[nodiscard]] Result<TimeSeries<T>> resample(Frequency target_freq) const
        requires std::floating_point<T>
    {
        // Default aggregator is mean
        auto mean_aggregator = [](const std::vector<T>& values) -> T {
            if (values.empty())
                return T{0};
            T sum = T{0};
            for (const auto& val : values) {
                sum += val;
            }
            return sum / static_cast<T>(values.size());
        };
        return resample(target_freq, mean_aggregator);
    }

    /**
     * @brief Resample to different frequency
     */
    [[nodiscard]] Result<TimeSeries<T>> resample(Frequency target_freq,
                                                 const std::function<T(const std::vector<T>&)>& aggregator) const {
        if (empty()) {
            return Result<TimeSeries<T>>::error(ErrorCode::InsufficientData, "Cannot resample empty time series");
        }

        // Simplified resampling - group by target frequency periods
        std::map<DateTime, std::vector<T>> groups;

        for (size_type i = 0; i < size(); ++i) {
            DateTime period_start = get_period_start(timestamps_[i], target_freq);
            groups[period_start].push_back(values_[i]);
        }

        std::vector<DateTime> new_timestamps;
        std::vector<T> new_values;

        for (const auto& [period_start, period_values] : groups) {
            new_timestamps.push_back(period_start);
            new_values.push_back(aggregator(period_values));
        }

        return Result<TimeSeries<T>>::success(
            TimeSeries<T>{std::move(new_timestamps), std::move(new_values), name_ + "_resampled"});
    }

    /**
     * @brief Rolling window operation
     */
    template <typename F>
    [[nodiscard]] Result<TimeSeries<T>> rolling(size_type window_size, F&& func) const {
        if (window_size == 0 || window_size > size()) {
            return Result<TimeSeries<T>>::error(ErrorCode::InvalidInput,
                                                "Invalid window size: " + std::to_string(window_size));
        }

        std::vector<DateTime> result_timestamps;
        std::vector<T> result_values;

        // Reserve space for all timestamps
        result_timestamps.reserve(size());
        result_values.reserve(size());

        for (size_type i = 0; i < size(); ++i) {
            result_timestamps.push_back(timestamps_[i]);

            if (i >= window_size - 1) {
                // We have enough data for a complete window
                std::span<const T> window_data{values_.data() + i - window_size + 1, window_size};
                result_values.push_back(func(window_data));
            } else {
                // Not enough data, use NaN for floating point types
                if constexpr (std::floating_point<T>) {
                    result_values.push_back(std::numeric_limits<T>::quiet_NaN());
                } else {
                    // For non-floating point types, use default value
                    result_values.push_back(T{});
                }
            }
        }

        return Result<TimeSeries<T>>::success(
            TimeSeries<T>{std::move(result_timestamps), std::move(result_values), name_ + "_rolling"});
    }

    /**
     * @brief Shift values by n periods
     */
    [[nodiscard]] Result<TimeSeries<T>> shift(int periods) const {
        if (empty()) {
            return Result<TimeSeries<T>>::success(*this);
        }

        std::vector<T> shifted_values(size());

        if (periods > 0) {
            // Forward shift - fill beginning with NaN
            std::fill_n(shifted_values.begin(), std::min(static_cast<size_type>(periods), size()),
                        std::numeric_limits<T>::quiet_NaN());

            if (static_cast<size_type>(periods) < size()) {
                std::copy(values_.begin(), values_.end() - periods, shifted_values.begin() + periods);
            }
        } else if (periods < 0) {
            // Backward shift - fill end with NaN
            size_type abs_periods = static_cast<size_type>(-periods);

            if (abs_periods < size()) {
                std::copy(values_.begin() + abs_periods, values_.end(), shifted_values.begin());
            }

            std::fill(shifted_values.end() - std::min(abs_periods, size()), shifted_values.end(),
                      std::numeric_limits<T>::quiet_NaN());
        } else {
            // No shift
            shifted_values = values_;
        }

        return Result<TimeSeries<T>>::success(
            TimeSeries<T>{timestamps_, std::move(shifted_values), name_ + "_shifted"});
    }

    /**
     * @brief Percentage change
     */
    [[nodiscard]] Result<TimeSeries<T>> pct_change(int periods = 1) const
        requires std::floating_point<T>
    {
        auto shifted = shift(periods);
        if (shifted.is_error()) {
            return shifted;
        }

        const auto& shifted_values = shifted.value().values();
        std::vector<T> pct_changes(size());

        for (size_type i = 0; i < size(); ++i) {
            if (std::isnan(shifted_values[i]) || shifted_values[i] == T{0}) {
                pct_changes[i] = std::numeric_limits<T>::quiet_NaN();
            } else {
                pct_changes[i] = (values_[i] - shifted_values[i]) / shifted_values[i];
            }
        }

        return Result<TimeSeries<T>>::success(
            TimeSeries<T>{timestamps_, std::move(pct_changes), name_ + "_pct_change"});
    }

    /**
     * @brief Cumulative sum
     */
    [[nodiscard]] Result<TimeSeries<T>> cumsum() const {
        std::vector<T> cumulative(size());

        if (!empty()) {
            cumulative[0] = values_[0];
            for (size_type i = 1; i < size(); ++i) {
                cumulative[i] = cumulative[i - 1] + values_[i];
            }
        }

        return Result<TimeSeries<T>>::success(TimeSeries<T>{timestamps_, std::move(cumulative), name_ + "_cumsum"});
    }

    /**
     * @brief Cumulative product
     */
    [[nodiscard]] Result<TimeSeries<T>> cumprod() const {
        std::vector<T> cumulative(size());

        if (!empty()) {
            cumulative[0] = values_[0];
            for (size_type i = 1; i < size(); ++i) {
                cumulative[i] = cumulative[i - 1] * values_[i];
            }
        }

        return Result<TimeSeries<T>>::success(TimeSeries<T>{timestamps_, std::move(cumulative), name_ + "_cumprod"});
    }

    /**
     * @brief Drop NaN values
     */
    [[nodiscard]] Result<TimeSeries<T>> dropna() const
        requires std::floating_point<T>
    {
        std::vector<DateTime> clean_timestamps;
        std::vector<T> clean_values;

        for (size_type i = 0; i < size(); ++i) {
            if (!std::isnan(values_[i])) {
                clean_timestamps.push_back(timestamps_[i]);
                clean_values.push_back(values_[i]);
            }
        }

        return Result<TimeSeries<T>>::success(
            TimeSeries<T>{std::move(clean_timestamps), std::move(clean_values), name_ + "_clean"});
    }

    /**
     * @brief Fill NaN values
     */
    [[nodiscard]] Result<TimeSeries<T>> fillna(const T& fill_value) const
        requires std::floating_point<T>
    {
        std::vector<T> filled_values = values_;

        for (auto& value : filled_values) {
            if (std::isnan(value)) {
                value = fill_value;
            }
        }

        return Result<TimeSeries<T>>::success(TimeSeries<T>{timestamps_, std::move(filled_values), name_ + "_filled"});
    }

    /**
     * @brief Find value at specific DateTime (additional overload)
     */
    [[nodiscard]] Result<T> at(const DateTime& timestamp) const {
        for (size_type i = 0; i < size(); ++i) {
            if (timestamps_[i] == timestamp) {
                return Result<T>::success(values_[i]);
            }
        }
        return Result<T>::error(ErrorCode::NotFound, "Timestamp not found in time series");
    }

    /**
     * @brief Calculate rolling mean (optimized)
     */
    [[nodiscard]] Result<TimeSeries<T>> rolling_mean(size_type window) const
        requires std::floating_point<T>
    {
        if (window == 0 || window > size()) {
            return Result<TimeSeries<T>>::error(ErrorCode::InvalidInput,
                                                "Invalid window size: " + std::to_string(window));
        }

        std::vector<DateTime> result_timestamps;
        std::vector<T> result_values;
        result_timestamps.reserve(size());
        result_values.reserve(size());

        // Running sum for efficient calculation
        T running_sum = T{0};

        for (size_type i = 0; i < size(); ++i) {
            result_timestamps.push_back(timestamps_[i]);

            if (i < window) {
                // Building up the initial window
                running_sum += values_[i];
                if (i == window - 1) {
                    // First complete window
                    result_values.push_back(running_sum / static_cast<T>(window));
                } else {
                    // Incomplete window
                    result_values.push_back(std::numeric_limits<T>::quiet_NaN());
                }
            } else {
                // Sliding window: remove oldest, add newest
                running_sum = running_sum - values_[i - window] + values_[i];
                result_values.push_back(running_sum / static_cast<T>(window));
            }
        }

        return Result<TimeSeries<T>>::success(
            TimeSeries<T>{std::move(result_timestamps), std::move(result_values), name_ + "_rolling_mean"});
    }

    /**
     * @brief Calculate rolling standard deviation (optimized)
     */
    [[nodiscard]] Result<TimeSeries<T>> rolling_std(size_type window) const
        requires std::floating_point<T>
    {
        if (window == 0 || window > size()) {
            return Result<TimeSeries<T>>::error(ErrorCode::InvalidInput,
                                                "Invalid window size: " + std::to_string(window));
        }

        std::vector<DateTime> result_timestamps;
        std::vector<T> result_values;
        result_timestamps.reserve(size());
        result_values.reserve(size());

        // Running sums for efficient calculation
        T running_sum    = T{0};
        T running_sum_sq = T{0};

        for (size_type i = 0; i < size(); ++i) {
            result_timestamps.push_back(timestamps_[i]);

            if (i < window) {
                // Building up the initial window
                running_sum += values_[i];
                running_sum_sq += values_[i] * values_[i];

                if (i == window - 1) {
                    // First complete window
                    T mean     = running_sum / static_cast<T>(window);
                    T variance = (running_sum_sq / static_cast<T>(window)) - (mean * mean);
                    result_values.push_back(std::sqrt(std::max(T{0}, variance)));
                } else {
                    // Incomplete window
                    result_values.push_back(std::numeric_limits<T>::quiet_NaN());
                }
            } else {
                // Sliding window: remove oldest, add newest
                T old_val = values_[i - window];
                T new_val = values_[i];

                running_sum    = running_sum - old_val + new_val;
                running_sum_sq = running_sum_sq - (old_val * old_val) + (new_val * new_val);

                T mean     = running_sum / static_cast<T>(window);
                T variance = (running_sum_sq / static_cast<T>(window)) - (mean * mean);
                result_values.push_back(std::sqrt(std::max(T{0}, variance)));
            }
        }

        return Result<TimeSeries<T>>::success(
            TimeSeries<T>{std::move(result_timestamps), std::move(result_values), name_ + "_rolling_std"});
    }

    /**
     * @brief Calculate rolling minimum (optimized with deque)
     */
    [[nodiscard]] Result<TimeSeries<T>> rolling_min(size_type window) const {
        if (window == 0 || window > size()) {
            return Result<TimeSeries<T>>::error(ErrorCode::InvalidInput,
                                                "Invalid window size: " + std::to_string(window));
        }

        std::vector<DateTime> result_timestamps;
        std::vector<T> result_values;
        result_timestamps.reserve(size());
        result_values.reserve(size());

        // Deque to maintain window minimum efficiently
        std::deque<size_type> min_deque;

        for (size_type i = 0; i < size(); ++i) {
            result_timestamps.push_back(timestamps_[i]);

            // Remove elements outside current window
            while (!min_deque.empty() && min_deque.front() <= i - window) {
                min_deque.pop_front();
            }

            // Remove elements greater than current element
            while (!min_deque.empty() && values_[min_deque.back()] >= values_[i]) {
                min_deque.pop_back();
            }

            // Add current element
            min_deque.push_back(i);

            if (i >= window - 1) {
                // Complete window
                result_values.push_back(values_[min_deque.front()]);
            } else {
                // Incomplete window
                if constexpr (std::floating_point<T>) {
                    result_values.push_back(std::numeric_limits<T>::quiet_NaN());
                } else {
                    result_values.push_back(T{});
                }
            }
        }

        return Result<TimeSeries<T>>::success(
            TimeSeries<T>{std::move(result_timestamps), std::move(result_values), name_ + "_rolling_min"});
    }

    /**
     * @brief Calculate rolling maximum (optimized with deque)
     */
    [[nodiscard]] Result<TimeSeries<T>> rolling_max(size_type window) const {
        if (window == 0 || window > size()) {
            return Result<TimeSeries<T>>::error(ErrorCode::InvalidInput,
                                                "Invalid window size: " + std::to_string(window));
        }

        std::vector<DateTime> result_timestamps;
        std::vector<T> result_values;
        result_timestamps.reserve(size());
        result_values.reserve(size());

        // Deque to maintain window maximum efficiently
        std::deque<size_type> max_deque;

        for (size_type i = 0; i < size(); ++i) {
            result_timestamps.push_back(timestamps_[i]);

            // Remove elements outside current window
            while (!max_deque.empty() && max_deque.front() <= i - window) {
                max_deque.pop_front();
            }

            // Remove elements smaller than current element
            while (!max_deque.empty() && values_[max_deque.back()] <= values_[i]) {
                max_deque.pop_back();
            }

            // Add current element
            max_deque.push_back(i);

            if (i >= window - 1) {
                // Complete window
                result_values.push_back(values_[max_deque.front()]);
            } else {
                // Incomplete window
                if constexpr (std::floating_point<T>) {
                    result_values.push_back(std::numeric_limits<T>::quiet_NaN());
                } else {
                    result_values.push_back(T{});
                }
            }
        }

        return Result<TimeSeries<T>>::success(
            TimeSeries<T>{std::move(result_timestamps), std::move(result_values), name_ + "_rolling_max"});
    }

    /**
     * @brief Calculate mean of all values (SIMD-optimized for double)
     */
    [[nodiscard]] Result<T> mean() const
        requires std::floating_point<T>
    {
        if (empty()) {
            return Result<T>::error(ErrorCode::InsufficientData, "Cannot calculate mean of empty series");
        }

        if constexpr (std::is_same_v<T, double>) {
            // Use SIMD-optimized version for double
            double result = simd::vector_mean(std::span<const double>(values_.data(), values_.size()));
            return Result<T>::success(static_cast<T>(result));
        } else {
            // Fallback for other floating-point types
            T sum = T{0};
            for (const auto& val : values_) {
                sum += val;
            }
            return Result<T>::success(sum / static_cast<T>(size()));
        }
    }

    /**
     * @brief Calculate standard deviation
     */
    [[nodiscard]] Result<T> std() const
        requires std::floating_point<T>
    {
        if (empty()) {
            return Result<T>::error(ErrorCode::InsufficientData, "Cannot calculate std of empty series");
        }

        auto mean_result = mean();
        if (mean_result.is_error()) {
            return mean_result;
        }

        T mean_val    = mean_result.value();
        T sum_sq_diff = T{0};

        for (const auto& val : values_) {
            T diff = val - mean_val;
            sum_sq_diff += diff * diff;
        }

        T variance = sum_sq_diff / static_cast<T>(size());
        return Result<T>::success(std::sqrt(variance));
    }

    /**
     * @brief Calculate returns (percentage change)
     */
    [[nodiscard]] Result<TimeSeries<Return>> returns() const
        requires std::floating_point<T>
    {
        if (size() < 2) {
            return Result<TimeSeries<Return>>::error(ErrorCode::InsufficientData,
                                                     "Need at least 2 data points to calculate returns");
        }

        std::vector<DateTime> ret_timestamps;
        std::vector<Return> ret_values;

        for (size_type i = 1; i < size(); ++i) {
            if (values_[i - 1] != T{0}) {
                Return ret = (values_[i] - values_[i - 1]) / values_[i - 1];
                ret_timestamps.push_back(timestamps_[i]);
                ret_values.push_back(ret);
            }
        }

        return Result<TimeSeries<Return>>::success(
            TimeSeries<Return>{std::move(ret_timestamps), std::move(ret_values), name_ + "_returns"});
    }

    /**
     * @brief Calculate cumulative returns
     */
    [[nodiscard]] Result<TimeSeries<T>> cumulative_returns() const
        requires std::floating_point<T>
    {
        if (empty()) {
            return Result<TimeSeries<T>>::error(ErrorCode::InsufficientData,
                                                "Cannot calculate cumulative returns of empty series");
        }

        std::vector<T> cum_returns;
        cum_returns.reserve(size());

        T cum_prod = T{1};
        for (const auto& ret : values_) {
            cum_prod *= (T{1} + ret);
            cum_returns.push_back(cum_prod - T{1});
        }

        return Result<TimeSeries<T>>::success(
            TimeSeries<T>{timestamps_, std::move(cum_returns), name_ + "_cumulative"});
    }

    /**
     * @brief Initialize with new data (for test compatibility)
     */
    [[nodiscard]] Result<TimeSeries<T>> initialize(const std::vector<DateTime>& new_timestamps,
                                                   const std::vector<T>& new_values) const {
        if (new_timestamps.size() != new_values.size()) {
            return Result<TimeSeries<T>>::error(ErrorCode::InvalidInput,
                                                "Timestamps and values must have the same size");
        }

        return Result<TimeSeries<T>>::success(TimeSeries<T>{new_timestamps, new_values, name_ + "_initialized"});
    }

    /**
     * @brief Align two time series by finding common dates
     */
    [[nodiscard]] Result<std::pair<TimeSeries<T>, TimeSeries<T>>> align(const TimeSeries<T>& other) const {
        if (empty() || other.empty()) {
            return Result<std::pair<TimeSeries<T>, TimeSeries<T>>>::error(ErrorCode::InvalidInput,
                                                                          "Cannot align empty time series");
        }

        // Find common dates
        std::vector<DateTime> common_dates;
        std::vector<T> aligned_values1, aligned_values2;

        for (size_t i = 0; i < timestamps_.size(); ++i) {
            auto it = std::find(other.timestamps_.begin(), other.timestamps_.end(), timestamps_[i]);
            if (it != other.timestamps_.end()) {
                common_dates.push_back(timestamps_[i]);
                aligned_values1.push_back(values_[i]);
                size_t idx = std::distance(other.timestamps_.begin(), it);
                aligned_values2.push_back(other.values_[idx]);
            }
        }

        if (common_dates.empty()) {
            return Result<std::pair<TimeSeries<T>, TimeSeries<T>>>::error(ErrorCode::InvalidInput,
                                                                          "No common dates found for alignment");
        }

        TimeSeries<T> aligned1(common_dates, aligned_values1, name_ + "_aligned");
        TimeSeries<T> aligned2(common_dates, aligned_values2, other.name_ + "_aligned");

        return Result<std::pair<TimeSeries<T>, TimeSeries<T>>>::success(
            std::make_pair(std::move(aligned1), std::move(aligned2)));
    }

    /**
     * @brief Fill missing values using specified method
     */
    [[nodiscard]] Result<TimeSeries<T>> fill_missing(const std::vector<DateTime>& target_dates,
                                                     FillMethod method) const {
        if (target_dates.empty()) {
            return Result<TimeSeries<T>>::error(ErrorCode::InvalidInput, "Target dates cannot be empty");
        }

        std::vector<T> filled_values;
        filled_values.reserve(target_dates.size());

        for (const auto& target_date : target_dates) {
            auto it = std::find(timestamps_.begin(), timestamps_.end(), target_date);

            if (it != timestamps_.end()) {
                // Date exists, use actual value
                size_t idx = std::distance(timestamps_.begin(), it);
                filled_values.push_back(values_[idx]);
            } else {
                // Date missing, apply fill method
                switch (method) {
                    case FillMethod::Forward:
                        // Find last available value before or at this date
                        {
                            T fill_value = T{0};
                            bool found   = false;

                            for (size_t i = 0; i < timestamps_.size(); ++i) {
                                if (timestamps_[i] <= target_date) {
                                    fill_value = values_[i];
                                    found      = true;
                                } else {
                                    break;  // Stop at first date after target
                                }
                            }

                            if (found) {
                                filled_values.push_back(fill_value);
                            } else if (!empty()) {
                                // Use first value if no prior value found
                                filled_values.push_back(values_[0]);
                            } else {
                                return Result<TimeSeries<T>>::error(ErrorCode::InsufficientData,
                                                                    "No data to fill from");
                            }
                        }
                        break;

                    case FillMethod::Backward:
                        // Find next available value after this date
                        for (size_t i = 0; i < timestamps_.size(); ++i) {
                            if (timestamps_[i] > target_date) {
                                filled_values.push_back(values_[i]);
                                goto next_date;
                            }
                        }
                        // If no future value found, use last value
                        if (!empty()) {
                            filled_values.push_back(values_.back());
                        } else {
                            return Result<TimeSeries<T>>::error(ErrorCode::InsufficientData, "No data to fill from");
                        }
                        break;

                    case FillMethod::Drop:
                        // For Drop method, we'll skip missing values (not implemented fully)
                        // For now, use zero as fallback
                        filled_values.push_back(T{0});
                        break;

                    default:
                        return Result<TimeSeries<T>>::error(ErrorCode::InvalidInput, "Unknown fill method");
                }
            next_date:;
            }
        }

        return Result<TimeSeries<T>>::success(TimeSeries<T>{target_dates, filled_values, name_ + "_filled"});
    }

    // SIMD-optimized arithmetic operations for time series

    /**
     * @brief Element-wise addition (SIMD-optimized)
     */
    [[nodiscard]] Result<TimeSeries<T>> operator+(const TimeSeries<T>& other) const
        requires std::floating_point<T>
    {
        if (size() != other.size()) {
            return Result<TimeSeries<T>>::error(ErrorCode::InvalidInput, "TimeSeries sizes must match for addition");
        }

        if (empty()) {
            return Result<TimeSeries<T>>::success(*this);
        }

        std::vector<T> result_values(size());

        if constexpr (std::is_same_v<T, double>) {
            // Use SIMD-optimized version for double
            simd::vector_add(std::span<const double>(values_.data(), values_.size()),
                             std::span<const double>(other.values_.data(), other.values_.size()),
                             std::span<double>(result_values.data(), result_values.size()));
        } else {
            // Fallback for other types
            std::transform(values_.begin(), values_.end(), other.values_.begin(), result_values.begin(),
                           std::plus<T>{});
        }

        return Result<TimeSeries<T>>::success(TimeSeries<T>{timestamps_, std::move(result_values), name_ + "_add"});
    }

    /**
     * @brief Element-wise subtraction (SIMD-optimized)
     */
    [[nodiscard]] Result<TimeSeries<T>> operator-(const TimeSeries<T>& other) const
        requires std::floating_point<T>
    {
        if (size() != other.size()) {
            return Result<TimeSeries<T>>::error(ErrorCode::InvalidInput, "TimeSeries sizes must match for subtraction");
        }

        if (empty()) {
            return Result<TimeSeries<T>>::success(*this);
        }

        std::vector<T> result_values(size());

        if constexpr (std::is_same_v<T, double>) {
            // Use SIMD-optimized version for double
            simd::vector_subtract(std::span<const double>(values_.data(), values_.size()),
                                  std::span<const double>(other.values_.data(), other.values_.size()),
                                  std::span<double>(result_values.data(), result_values.size()));
        } else {
            // Fallback for other types
            std::transform(values_.begin(), values_.end(), other.values_.begin(), result_values.begin(),
                           std::minus<T>{});
        }

        return Result<TimeSeries<T>>::success(TimeSeries<T>{timestamps_, std::move(result_values), name_ + "_sub"});
    }

    /**
     * @brief Element-wise multiplication (SIMD-optimized)
     */
    [[nodiscard]] Result<TimeSeries<T>> operator*(const TimeSeries<T>& other) const
        requires std::floating_point<T>
    {
        if (size() != other.size()) {
            return Result<TimeSeries<T>>::error(ErrorCode::InvalidInput,
                                                "TimeSeries sizes must match for multiplication");
        }

        if (empty()) {
            return Result<TimeSeries<T>>::success(*this);
        }

        std::vector<T> result_values(size());

        if constexpr (std::is_same_v<T, double>) {
            // Use SIMD-optimized version for double
            simd::vector_multiply(std::span<const double>(values_.data(), values_.size()),
                                  std::span<const double>(other.values_.data(), other.values_.size()),
                                  std::span<double>(result_values.data(), result_values.size()));
        } else {
            // Fallback for other types
            std::transform(values_.begin(), values_.end(), other.values_.begin(), result_values.begin(),
                           std::multiplies<T>{});
        }

        return Result<TimeSeries<T>>::success(TimeSeries<T>{timestamps_, std::move(result_values), name_ + "_mul"});
    }

    /**
     * @brief Scalar multiplication (SIMD-optimized)
     */
    [[nodiscard]] Result<TimeSeries<T>> operator*(T scalar) const
        requires std::floating_point<T>
    {
        if (empty()) {
            return Result<TimeSeries<T>>::success(*this);
        }

        std::vector<T> result_values(size());

        if constexpr (std::is_same_v<T, double>) {
            // Use SIMD-optimized version for double
            simd::vector_scale(std::span<const double>(values_.data(), values_.size()), static_cast<double>(scalar),
                               std::span<double>(result_values.data(), result_values.size()));
        } else {
            // Fallback for other types
            std::transform(values_.begin(), values_.end(), result_values.begin(),
                           [scalar](const T& val) { return val * scalar; });
        }

        return Result<TimeSeries<T>>::success(TimeSeries<T>{timestamps_, std::move(result_values), name_ + "_scale"});
    }

    /**
     * @brief Dot product with another time series (SIMD-optimized)
     */
    [[nodiscard]] Result<T> dot(const TimeSeries<T>& other) const
        requires std::floating_point<T>
    {
        if (size() != other.size()) {
            return Result<T>::error(ErrorCode::InvalidInput, "TimeSeries sizes must match for dot product");
        }

        if (empty()) {
            return Result<T>::success(T{0});
        }

        if constexpr (std::is_same_v<T, double>) {
            // Use SIMD-optimized version for double
            double result = simd::dot_product(std::span<const double>(values_.data(), values_.size()),
                                              std::span<const double>(other.values_.data(), other.values_.size()));
            return Result<T>::success(static_cast<T>(result));
        } else {
            // Fallback for other types
            T result = std::inner_product(values_.begin(), values_.end(), other.values_.begin(), T{0});
            return Result<T>::success(result);
        }
    }

    /**
     * @brief Correlation with another time series (SIMD-optimized)
     */
    [[nodiscard]] Result<double> correlation(const TimeSeries<T>& other) const
        requires std::floating_point<T>
    {
        if (size() != other.size() || size() < 2) {
            return Result<double>::error(ErrorCode::InsufficientData,
                                         "Need matching series with at least 2 points for correlation");
        }

        if constexpr (std::is_same_v<T, double>) {
            // Use SIMD-optimized version for double
            double result = simd::correlation_simd(std::span<const double>(values_.data(), values_.size()),
                                                   std::span<const double>(other.values_.data(), other.values_.size()));
            return Result<double>::success(result);
        } else {
            // Fallback implementation for other types
            auto mean1_result = mean();
            auto mean2_result = other.mean();

            if (mean1_result.is_error() || mean2_result.is_error()) {
                return Result<double>::error(ErrorCode::InsufficientData, "Failed to calculate means for correlation");
            }

            double mean1 = static_cast<double>(mean1_result.value());
            double mean2 = static_cast<double>(mean2_result.value());

            double numerator = 0.0, sum_sq1 = 0.0, sum_sq2 = 0.0;

            for (size_t i = 0; i < size(); ++i) {
                double diff1 = static_cast<double>(values_[i]) - mean1;
                double diff2 = static_cast<double>(other.values_[i]) - mean2;
                numerator += diff1 * diff2;
                sum_sq1 += diff1 * diff1;
                sum_sq2 += diff2 * diff2;
            }

            double denominator = std::sqrt(sum_sq1 * sum_sq2);
            if (denominator == 0.0) {
                return Result<double>::success(0.0);
            }

            return Result<double>::success(numerator / denominator);
        }
    }

  private:
    DateTime get_period_start(const DateTime& timestamp, Frequency freq) const {
        // Simplified period calculation
        auto date = timestamp.to_date();

        switch (freq) {
            case Frequency::Weekly:
                // Start of week (Monday)
                return DateTime{date};  // Simplified

            case Frequency::Monthly:
                // First day of month
                return DateTime{date.year() / date.month() / std::chrono::day{1}};

            case Frequency::Quarterly:
                // First day of quarter
                {
                    auto month               = date.month();
                    auto quarter_start_month = ((static_cast<unsigned>(month) - 1) / 3) * 3 + 1;
                    return DateTime{date.year() / std::chrono::month{quarter_start_month} / std::chrono::day{1}};
                }

            case Frequency::Yearly:
                // First day of year
                return DateTime{date.year() / std::chrono::January / std::chrono::day{1}};

            default:
                return timestamp;
        }
    }
};

// Type aliases for common time series
using PriceSeries  = TimeSeries<Price>;
using ReturnSeries = TimeSeries<Return>;
using VolumeSeries = TimeSeries<Volume>;

}  // namespace pyfolio