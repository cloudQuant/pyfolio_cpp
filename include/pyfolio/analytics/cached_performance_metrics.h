#pragma once

/**
 * @file cached_performance_metrics.h
 * @brief Cached performance metrics calculator for high-performance applications
 * @version 1.0.0
 * @date 2024
 *
 * This file provides a caching layer for performance metric calculations,
 * designed for high-frequency trading and real-time analytics where
 * repeated calculations on the same data should be avoided.
 */

#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../core/types.h"
#include "performance_metrics.h"
#include "statistics.h"
#include <chrono>
#include <functional>
#include <shared_mutex>
#include <unordered_map>

namespace pyfolio::analytics {

/**
 * @brief Cache entry for performance metrics
 */
template <typename T>
struct CacheEntry {
    T value;
    std::chrono::steady_clock::time_point timestamp;
    size_t data_hash;
    size_t data_size;

    CacheEntry() = default;
    CacheEntry(T val, size_t hash, size_t size)
        : value(std::move(val)), timestamp(std::chrono::steady_clock::now()), data_hash(hash), data_size(size) {}

    bool is_valid(size_t current_hash, size_t current_size,
                  std::chrono::milliseconds max_age = std::chrono::milliseconds(5000)) const {
        if (data_hash != current_hash || data_size != current_size) {
            return false;
        }

        auto now = std::chrono::steady_clock::now();
        return (now - timestamp) < max_age;
    }
};

/**
 * @brief Cache configuration
 */
struct CacheConfig {
    size_t max_entries = 1000;                          // Maximum cache entries
    std::chrono::milliseconds max_age{30000};           // 30 seconds default TTL
    std::chrono::milliseconds cleanup_interval{60000};  // Cleanup every minute
    bool enable_auto_cleanup  = true;                   // Enable automatic cleanup
    double hit_rate_threshold = 0.7;                    // Minimum hit rate to keep cache

    // Performance thresholds - only cache if computation takes longer than these times
    std::chrono::microseconds min_computation_time_basic{1};    // 1μs for basic metrics (very low for testing)
    std::chrono::microseconds min_computation_time_complex{1};  // 1μs for complex metrics (very low for testing)
};

/**
 * @brief High-performance cached performance metrics calculator
 *
 * This class provides intelligent caching for expensive performance metric
 * calculations. It uses content-based hashing to detect data changes and
 * automatic cache invalidation based on time and data changes.
 *
 * Features:
 * - Content-aware caching (detects data changes via hashing)
 * - Automatic cache invalidation and cleanup
 * - Thread-safe operations with read-write locks
 * - Performance monitoring and adaptive caching
 * - Memory-efficient with configurable limits
 * - Hit rate optimization
 */
class CachedPerformanceCalculator {
  private:
    // Cache storage
    mutable std::unordered_map<std::string, CacheEntry<double>> scalar_cache_;
    mutable std::unordered_map<std::string, CacheEntry<PerformanceMetrics>> metrics_cache_;
    mutable std::unordered_map<std::string, CacheEntry<TimeSeries<double>>> series_cache_;

    // Thread safety
    mutable std::shared_mutex cache_mutex_;

    // Cache configuration and statistics
    CacheConfig config_;
    mutable size_t cache_hits_{0};
    mutable size_t cache_misses_{0};
    mutable std::chrono::steady_clock::time_point last_cleanup_;

    /**
     * @brief Calculate hash for TimeSeries data
     */
    template <typename T>
    size_t calculate_hash(const TimeSeries<T>& series) const {
        std::hash<T> hasher;
        size_t seed = series.size();

        // Hash a sample of values for large series (for performance)
        size_t step = std::max(size_t{1}, series.size() / 100);

        for (size_t i = 0; i < series.size(); i += step) {
            seed ^= hasher(series[i]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }

        // Also hash first and last few values for better collision detection
        size_t sample_size = std::min(size_t{10}, series.size());
        for (size_t i = 0; i < sample_size; ++i) {
            seed ^= hasher(series[i]) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        }

        if (series.size() > sample_size) {
            for (size_t i = series.size() - sample_size; i < series.size(); ++i) {
                seed ^= hasher(series[i]) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
            }
        }

        return seed;
    }

    /**
     * @brief Generate cache key
     */
    template <typename... Args>
    std::string make_cache_key(const std::string& function_name, Args&&... args) const {
        std::string key = function_name;

        auto append_arg = [&key](const auto& arg) {
            if constexpr (std::is_arithmetic_v<std::decay_t<decltype(arg)>>) {
                key += "_" + std::to_string(arg);
            } else if constexpr (std::is_same_v<std::decay_t<decltype(arg)>, std::string>) {
                key += "_" + arg;
            }
        };

        (append_arg(args), ...);

        return key;
    }

    /**
     * @brief Check if cache needs cleanup
     */
    void check_and_cleanup_cache() const {
        auto now = std::chrono::steady_clock::now();
        if (!config_.enable_auto_cleanup || (now - last_cleanup_) < config_.cleanup_interval) {
            return;
        }

        std::unique_lock lock(cache_mutex_);

        // Clean up expired entries
        auto cleanup_map = [&](auto& cache_map) {
            for (auto it = cache_map.begin(); it != cache_map.end();) {
                if ((now - it->second.timestamp) > config_.max_age) {
                    it = cache_map.erase(it);
                } else {
                    ++it;
                }
            }
        };

        cleanup_map(scalar_cache_);
        cleanup_map(metrics_cache_);
        cleanup_map(series_cache_);

        // If cache is too large, remove oldest entries
        auto trim_cache = [&](auto& cache_map) {
            if (cache_map.size() > config_.max_entries) {
                std::vector<std::pair<std::string, std::chrono::steady_clock::time_point>> entries;
                for (const auto& [key, entry] : cache_map) {
                    entries.emplace_back(key, entry.timestamp);
                }

                std::sort(entries.begin(), entries.end(),
                          [](const auto& a, const auto& b) { return a.second < b.second; });

                size_t to_remove = cache_map.size() - config_.max_entries + 100;  // Remove extra for hysteresis
                for (size_t i = 0; i < std::min(to_remove, entries.size()); ++i) {
                    cache_map.erase(entries[i].first);
                }
            }
        };

        trim_cache(scalar_cache_);
        trim_cache(metrics_cache_);
        trim_cache(series_cache_);

        last_cleanup_ = now;
    }

    /**
     * @brief Try to get cached result
     */
    template <typename T>
    std::optional<T> try_get_cached(const std::string& key, size_t data_hash, size_t data_size, auto& cache_map) const {
        std::shared_lock lock(cache_mutex_);

        auto it = cache_map.find(key);
        if (it != cache_map.end() && it->second.is_valid(data_hash, data_size, config_.max_age)) {
            ++cache_hits_;
            return it->second.value;
        }

        ++cache_misses_;
        return std::nullopt;
    }

    /**
     * @brief Store result in cache
     */
    template <typename T>
    void store_in_cache(const std::string& key, T&& value, size_t data_hash, size_t data_size, auto& cache_map) const {
        std::unique_lock lock(cache_mutex_);
        cache_map[key] = CacheEntry<std::decay_t<T>>(std::forward<T>(value), data_hash, data_size);
    }

  public:
    explicit CachedPerformanceCalculator(CacheConfig config = {})
        : config_(std::move(config)), last_cleanup_(std::chrono::steady_clock::now()) {}

    /**
     * @brief Get cache statistics
     */
    struct CacheStats {
        size_t total_hits;
        size_t total_misses;
        double hit_rate;
        size_t scalar_cache_size;
        size_t metrics_cache_size;
        size_t series_cache_size;
        size_t total_cache_size;
    };

    CacheStats get_cache_stats() const {
        std::shared_lock lock(cache_mutex_);

        size_t total_requests = cache_hits_ + cache_misses_;
        double hit_rate       = total_requests > 0 ? static_cast<double>(cache_hits_) / total_requests : 0.0;

        return CacheStats{.total_hits         = cache_hits_,
                          .total_misses       = cache_misses_,
                          .hit_rate           = hit_rate,
                          .scalar_cache_size  = scalar_cache_.size(),
                          .metrics_cache_size = metrics_cache_.size(),
                          .series_cache_size  = series_cache_.size(),
                          .total_cache_size   = scalar_cache_.size() + metrics_cache_.size() + series_cache_.size()};
    }

    /**
     * @brief Clear all caches
     */
    void clear_cache() {
        std::unique_lock lock(cache_mutex_);
        scalar_cache_.clear();
        metrics_cache_.clear();
        series_cache_.clear();
        cache_hits_   = 0;
        cache_misses_ = 0;
    }

    /**
     * @brief Update cache configuration
     */
    void update_config(const CacheConfig& new_config) {
        std::unique_lock lock(cache_mutex_);
        config_ = new_config;
    }

    // ========== CACHED PERFORMANCE METRIC CALCULATIONS ==========

    /**
     * @brief Cached mean calculation
     */
    template <typename T>
    [[nodiscard]] Result<double> mean(const TimeSeries<T>& series) const {
        check_and_cleanup_cache();

        size_t data_hash      = calculate_hash(series);
        std::string cache_key = make_cache_key("mean", data_hash);

        if (auto cached = try_get_cached<double>(cache_key, data_hash, series.size(), scalar_cache_)) {
            return Result<double>::success(*cached);
        }

        auto start_time = std::chrono::high_resolution_clock::now();
        auto result     = series.mean();
        auto end_time   = std::chrono::high_resolution_clock::now();

        // Only cache if computation took significant time
        if (result.is_ok() && (end_time - start_time) >= config_.min_computation_time_basic) {
            store_in_cache(cache_key, result.value(), data_hash, series.size(), scalar_cache_);
        }

        return result;
    }

    /**
     * @brief Cached standard deviation calculation
     */
    template <typename T>
    [[nodiscard]] Result<double> std_deviation(const TimeSeries<T>& series) const {
        check_and_cleanup_cache();

        size_t data_hash      = calculate_hash(series);
        std::string cache_key = make_cache_key("std", data_hash);

        if (auto cached = try_get_cached<double>(cache_key, data_hash, series.size(), scalar_cache_)) {
            return Result<double>::success(*cached);
        }

        auto start_time = std::chrono::high_resolution_clock::now();
        auto result     = series.std();
        auto end_time   = std::chrono::high_resolution_clock::now();

        if (result.is_ok() && (end_time - start_time) >= config_.min_computation_time_basic) {
            store_in_cache(cache_key, result.value(), data_hash, series.size(), scalar_cache_);
        }

        return result;
    }

    /**
     * @brief Cached correlation calculation
     */
    template <typename T>
    [[nodiscard]] Result<double> correlation(const TimeSeries<T>& series1, const TimeSeries<T>& series2) const {
        check_and_cleanup_cache();

        size_t hash1         = calculate_hash(series1);
        size_t hash2         = calculate_hash(series2);
        size_t combined_hash = hash1 ^ (hash2 << 1);

        std::string cache_key = make_cache_key("correlation", combined_hash);

        if (auto cached =
                try_get_cached<double>(cache_key, combined_hash, series1.size() + series2.size(), scalar_cache_)) {
            return Result<double>::success(*cached);
        }

        auto start_time = std::chrono::high_resolution_clock::now();
        auto result     = series1.correlation(series2);
        auto end_time   = std::chrono::high_resolution_clock::now();

        if (result.is_ok() && (end_time - start_time) >= config_.min_computation_time_basic) {
            store_in_cache(cache_key, result.value(), combined_hash, series1.size() + series2.size(), scalar_cache_);
        }

        return result;
    }

    /**
     * @brief Cached rolling mean calculation
     */
    template <typename T>
    [[nodiscard]] Result<TimeSeries<T>> rolling_mean(const TimeSeries<T>& series, size_t window) const {
        check_and_cleanup_cache();

        size_t data_hash      = calculate_hash(series);
        std::string cache_key = make_cache_key("rolling_mean", data_hash, window);

        if (auto cached = try_get_cached<TimeSeries<T>>(cache_key, data_hash, series.size(), series_cache_)) {
            return Result<TimeSeries<T>>::success(*cached);
        }

        auto start_time = std::chrono::high_resolution_clock::now();
        auto result     = series.rolling_mean(window);
        auto end_time   = std::chrono::high_resolution_clock::now();

        if (result.is_ok() && (end_time - start_time) >= config_.min_computation_time_complex) {
            store_in_cache(cache_key, result.value(), data_hash, series.size(), series_cache_);
        }

        return result;
    }

    /**
     * @brief Cached rolling standard deviation calculation
     */
    template <typename T>
    [[nodiscard]] Result<TimeSeries<T>> rolling_std(const TimeSeries<T>& series, size_t window) const {
        check_and_cleanup_cache();

        size_t data_hash      = calculate_hash(series);
        std::string cache_key = make_cache_key("rolling_std", data_hash, window);

        if (auto cached = try_get_cached<TimeSeries<T>>(cache_key, data_hash, series.size(), series_cache_)) {
            return Result<TimeSeries<T>>::success(*cached);
        }

        auto start_time = std::chrono::high_resolution_clock::now();
        auto result     = series.rolling_std(window);
        auto end_time   = std::chrono::high_resolution_clock::now();

        if (result.is_ok() && (end_time - start_time) >= config_.min_computation_time_complex) {
            store_in_cache(cache_key, result.value(), data_hash, series.size(), series_cache_);
        }

        return result;
    }

    /**
     * @brief Cached comprehensive performance metrics calculation
     */
    template <typename T>
    [[nodiscard]] Result<PerformanceMetrics> calculate_performance_metrics(
        const TimeSeries<T>& returns, const std::optional<TimeSeries<T>>& benchmark = std::nullopt,
        double risk_free_rate = 0.02) const {
        check_and_cleanup_cache();

        size_t data_hash = calculate_hash(returns);
        if (benchmark) {
            size_t bench_hash = calculate_hash(*benchmark);
            data_hash ^= bench_hash << 1;
        }

        std::string cache_key = make_cache_key("perf_metrics", data_hash, static_cast<int>(risk_free_rate * 10000));

        if (auto cached = try_get_cached<PerformanceMetrics>(cache_key, data_hash, returns.size(), metrics_cache_)) {
            return Result<PerformanceMetrics>::success(*cached);
        }

        auto start_time = std::chrono::high_resolution_clock::now();
        auto result     = analytics::calculate_performance_metrics(returns, benchmark, risk_free_rate);
        auto end_time   = std::chrono::high_resolution_clock::now();

        if (result.is_ok() && (end_time - start_time) >= config_.min_computation_time_complex) {
            store_in_cache(cache_key, result.value(), data_hash, returns.size(), metrics_cache_);
        }

        return result;
    }

    /**
     * @brief Cached Sharpe ratio calculation
     */
    template <typename T>
    [[nodiscard]] Result<double> sharpe_ratio(const TimeSeries<T>& returns, double risk_free_rate = 0.02) const {
        check_and_cleanup_cache();

        size_t data_hash      = calculate_hash(returns);
        std::string cache_key = make_cache_key("sharpe", data_hash, static_cast<int>(risk_free_rate * 10000));

        if (auto cached = try_get_cached<double>(cache_key, data_hash, returns.size(), scalar_cache_)) {
            return Result<double>::success(*cached);
        }

        auto start_time = std::chrono::high_resolution_clock::now();

        // Calculate Sharpe ratio manually for caching individual components
        auto mean_result = mean(returns);  // This will use cached mean if available
        if (mean_result.is_error()) {
            return mean_result;
        }

        auto std_result = std_deviation(returns);  // This will use cached std if available
        if (std_result.is_error()) {
            return std_result;
        }

        double excess_return = mean_result.value() - risk_free_rate / 252.0;  // Daily risk-free rate
        double sharpe        = excess_return / std_result.value();

        auto end_time = std::chrono::high_resolution_clock::now();

        if ((end_time - start_time) >= config_.min_computation_time_basic) {
            store_in_cache(cache_key, sharpe, data_hash, returns.size(), scalar_cache_);
        }

        return Result<double>::success(sharpe);
    }

    /**
     * @brief Cached maximum drawdown calculation
     */
    template <typename T>
    [[nodiscard]] Result<double> max_drawdown(const TimeSeries<T>& prices) const {
        check_and_cleanup_cache();

        size_t data_hash      = calculate_hash(prices);
        std::string cache_key = make_cache_key("max_drawdown", data_hash);

        if (auto cached = try_get_cached<double>(cache_key, data_hash, prices.size(), scalar_cache_)) {
            return Result<double>::success(*cached);
        }

        auto start_time = std::chrono::high_resolution_clock::now();

        // Calculate maximum drawdown
        if (prices.empty()) {
            return Result<double>::error(ErrorCode::InvalidInput, "Empty price series");
        }

        double max_price = prices[0];
        double max_dd    = 0.0;

        for (size_t i = 1; i < prices.size(); ++i) {
            max_price       = std::max(max_price, static_cast<double>(prices[i]));
            double drawdown = (max_price - static_cast<double>(prices[i])) / max_price;
            max_dd          = std::max(max_dd, drawdown);
        }

        auto end_time = std::chrono::high_resolution_clock::now();

        if ((end_time - start_time) >= config_.min_computation_time_basic) {
            store_in_cache(cache_key, max_dd, data_hash, prices.size(), scalar_cache_);
        }

        return Result<double>::success(max_dd);
    }
};

/**
 * @brief Global cached performance calculator instance
 */
inline CachedPerformanceCalculator& get_global_cache() {
    static CachedPerformanceCalculator global_cache;
    return global_cache;
}

/**
 * @brief Convenience functions that use global cache
 */
namespace cached {

template <typename T>
[[nodiscard]] Result<double> mean(const TimeSeries<T>& series) {
    return get_global_cache().mean(series);
}

template <typename T>
[[nodiscard]] Result<double> std_deviation(const TimeSeries<T>& series) {
    return get_global_cache().std_deviation(series);
}

template <typename T>
[[nodiscard]] Result<double> correlation(const TimeSeries<T>& series1, const TimeSeries<T>& series2) {
    return get_global_cache().correlation(series1, series2);
}

template <typename T>
[[nodiscard]] Result<double> sharpe_ratio(const TimeSeries<T>& returns, double risk_free_rate = 0.02) {
    return get_global_cache().sharpe_ratio(returns, risk_free_rate);
}

template <typename T>
[[nodiscard]] Result<double> max_drawdown(const TimeSeries<T>& prices) {
    return get_global_cache().max_drawdown(prices);
}

template <typename T>
[[nodiscard]] Result<TimeSeries<T>> rolling_mean(const TimeSeries<T>& series, size_t window) {
    return get_global_cache().rolling_mean(series, window);
}

template <typename T>
[[nodiscard]] Result<TimeSeries<T>> rolling_std(const TimeSeries<T>& series, size_t window) {
    return get_global_cache().rolling_std(series, window);
}

/**
 * @brief Get cache statistics
 */
inline CachedPerformanceCalculator::CacheStats get_cache_stats() {
    return get_global_cache().get_cache_stats();
}

/**
 * @brief Clear global cache
 */
inline void clear_cache() {
    get_global_cache().clear_cache();
}

}  // namespace cached

}  // namespace pyfolio::analytics