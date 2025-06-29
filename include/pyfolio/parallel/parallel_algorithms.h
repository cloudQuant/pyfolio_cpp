#pragma once

/**
 * @file parallel_algorithms.h
 * @brief Parallel processing algorithms for large dataset operations
 * @version 1.0.0
 * @date 2024
 *
 * This file provides thread-based parallel processing capabilities for
 * computationally intensive operations on large financial datasets.
 * Designed for high-performance computing in quantitative finance.
 */

#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../core/types.h"
#include <algorithm>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <numeric>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

namespace pyfolio::parallel {

/**
 * @brief Parallel processing configuration
 */
struct ParallelConfig {
    size_t max_threads        = std::thread::hardware_concurrency();
    size_t min_chunk_size     = 1000;  // Minimum elements per thread
    size_t chunk_size_factor  = 4;     // Multiplier for optimal chunk sizing
    bool enable_vectorization = true;  // Enable SIMD within parallel tasks
    bool adaptive_chunking    = true;  // Automatically adjust chunk sizes

    // Performance thresholds
    size_t parallel_threshold     = 10000;  // Minimum size for parallel execution
    double cpu_utilization_target = 0.8;    // Target CPU utilization
};

/**
 * @brief Thread pool for reusable worker threads
 */
class ThreadPool {
  private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_ = false;

  public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency()) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(queue_mutex_);
                        condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });

                        if (stop_ && tasks_.empty())
                            return;

                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }

                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }

        condition_.notify_all();

        for (std::thread& worker : workers_) {
            worker.join();
        }
    }

    template <typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result<F, Args...>::type> {
        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<return_type> result = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);

            if (stop_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }

            tasks_.emplace([task]() { (*task)(); });
        }

        condition_.notify_one();
        return result;
    }

    size_t size() const { return workers_.size(); }
};

/**
 * @brief Global thread pool instance
 */
inline ThreadPool& get_global_thread_pool() {
    static ThreadPool pool;
    return pool;
}

/**
 * @brief Parallel algorithms for financial data processing
 */
class ParallelAlgorithms {
  private:
    ParallelConfig config_;

    /**
     * @brief Calculate optimal chunk size for parallel processing
     */
    size_t calculate_chunk_size(size_t total_size, size_t num_threads) const {
        if (!config_.adaptive_chunking) {
            return std::max(config_.min_chunk_size, total_size / num_threads);
        }

        // Adaptive chunking based on data size and thread count
        size_t base_chunk    = total_size / (num_threads * config_.chunk_size_factor);
        size_t optimal_chunk = std::max(config_.min_chunk_size, base_chunk);

        // Ensure chunk size is reasonable for cache efficiency
        return std::min(optimal_chunk, total_size / 2);
    }

    /**
     * @brief Determine if parallel execution is beneficial
     */
    bool should_use_parallel(size_t data_size) const {
        return data_size >= config_.parallel_threshold && config_.max_threads > 1;
    }

  public:
    explicit ParallelAlgorithms(ParallelConfig config = {}) : config_(std::move(config)) {}

    /**
     * @brief Parallel map operation
     */
    template <typename T, typename UnaryOp>
    [[nodiscard]] Result<std::vector<T>> parallel_map(const std::vector<T>& input, UnaryOp op) const {
        if (input.empty()) {
            return Result<std::vector<T>>::success(std::vector<T>{});
        }

        if (!should_use_parallel(input.size())) {
            // Serial execution for small datasets
            std::vector<T> result;
            result.reserve(input.size());

            for (const auto& item : input) {
                result.push_back(op(item));
            }

            return Result<std::vector<T>>::success(std::move(result));
        }

        // Parallel execution
        const size_t num_threads = std::min(config_.max_threads, input.size());
        const size_t chunk_size  = calculate_chunk_size(input.size(), num_threads);

        std::vector<T> result(input.size());
        std::vector<std::future<void>> futures;

        auto& pool = get_global_thread_pool();

        for (size_t i = 0; i < input.size(); i += chunk_size) {
            size_t end = std::min(i + chunk_size, input.size());

            futures.push_back(pool.enqueue([&, i, end]() {
                for (size_t j = i; j < end; ++j) {
                    result[j] = op(input[j]);
                }
            }));
        }

        // Wait for all tasks to complete
        for (auto& future : futures) {
            future.wait();
        }

        return Result<std::vector<T>>::success(std::move(result));
    }

    /**
     * @brief Parallel reduce operation
     */
    template <typename T, typename BinaryOp>
    [[nodiscard]] Result<T> parallel_reduce(const std::vector<T>& input, T init, BinaryOp op) const {
        if (input.empty()) {
            return Result<T>::success(init);
        }

        if (!should_use_parallel(input.size())) {
            // Serial execution
            return Result<T>::success(std::accumulate(input.begin(), input.end(), init, op));
        }

        // Parallel execution
        const size_t num_threads = std::min(config_.max_threads, input.size());
        const size_t chunk_size  = calculate_chunk_size(input.size(), num_threads);

        std::vector<std::future<T>> futures;
        auto& pool = get_global_thread_pool();

        for (size_t i = 0; i < input.size(); i += chunk_size) {
            size_t end = std::min(i + chunk_size, input.size());

            futures.push_back(pool.enqueue([&, i, end]() -> T {
                T local_result = (i == 0) ? init : T{};

                for (size_t j = i; j < end; ++j) {
                    if (i == 0 && j == i) {
                        local_result = op(local_result, input[j]);
                    } else {
                        local_result = (j == i) ? input[j] : op(local_result, input[j]);
                    }
                }

                return local_result;
            }));
        }

        // Combine results from all threads
        T final_result = init;
        bool first     = true;

        for (auto& future : futures) {
            T partial_result = future.get();
            if (first && !futures.empty()) {
                final_result = partial_result;
                first        = false;
            } else {
                final_result = op(final_result, partial_result);
            }
        }

        return Result<T>::success(final_result);
    }

    /**
     * @brief Parallel time series operations
     */
    template <typename T>
    [[nodiscard]] Result<double> parallel_mean(const TimeSeries<T>& series) const {
        if (series.empty()) {
            return Result<double>::error(ErrorCode::InvalidInput, "Empty time series");
        }

        const auto& values = series.values();

        auto sum_result = parallel_reduce(values, 0.0, std::plus<double>());
        if (sum_result.is_error()) {
            return Result<double>::error(sum_result.error().code, sum_result.error().message);
        }

        return Result<double>::success(sum_result.value() / values.size());
    }

    /**
     * @brief Parallel standard deviation calculation
     */
    template <typename T>
    [[nodiscard]] Result<double> parallel_std_deviation(const TimeSeries<T>& series) const {
        auto mean_result = parallel_mean(series);
        if (mean_result.is_error()) {
            return Result<double>::error(mean_result.error().code, mean_result.error().message);
        }

        double mean_val    = mean_result.value();
        const auto& values = series.values();

        // Parallel variance calculation
        auto variance_sum_result = parallel_reduce(values, 0.0, [mean_val](double acc, double val) {
            double diff = val - mean_val;
            return acc + diff * diff;
        });

        if (variance_sum_result.is_error()) {
            return Result<double>::error(variance_sum_result.error().code, variance_sum_result.error().message);
        }

        double variance = variance_sum_result.value() / values.size();
        return Result<double>::success(std::sqrt(variance));
    }

    /**
     * @brief Parallel correlation calculation
     */
    template <typename T>
    [[nodiscard]] Result<double> parallel_correlation(const TimeSeries<T>& series1,
                                                      const TimeSeries<T>& series2) const {
        if (series1.size() != series2.size()) {
            return Result<double>::error(ErrorCode::InvalidInput, "Series size mismatch");
        }

        if (series1.empty()) {
            return Result<double>::error(ErrorCode::InvalidInput, "Empty time series");
        }

        // Calculate means in parallel
        auto mean1_result = parallel_mean(series1);
        auto mean2_result = parallel_mean(series2);

        if (mean1_result.is_error())
            return Result<double>::error(mean1_result.error().code, mean1_result.error().message);
        if (mean2_result.is_error())
            return Result<double>::error(mean2_result.error().code, mean2_result.error().message);

        double mean1 = mean1_result.value();
        double mean2 = mean2_result.value();

        const auto& values1 = series1.values();
        const auto& values2 = series2.values();

        // Create combined data for parallel processing
        std::vector<std::pair<double, double>> combined_data;
        combined_data.reserve(values1.size());

        for (size_t i = 0; i < values1.size(); ++i) {
            combined_data.emplace_back(values1[i], values2[i]);
        }

        // Parallel covariance calculation
        std::vector<double> covariance_terms;
        covariance_terms.reserve(combined_data.size());

        for (const auto& pair : combined_data) {
            covariance_terms.push_back((pair.first - mean1) * (pair.second - mean2));
        }

        auto covariance_result = parallel_reduce(covariance_terms, 0.0, std::plus<double>());

        // Parallel variance calculations
        auto variance1_result = parallel_reduce(values1, 0.0, [mean1](double acc, double val) {
            double diff = val - mean1;
            return acc + diff * diff;
        });

        auto variance2_result = parallel_reduce(values2, 0.0, [mean2](double acc, double val) {
            double diff = val - mean2;
            return acc + diff * diff;
        });

        if (covariance_result.is_error())
            return Result<double>::error(covariance_result.error().code, covariance_result.error().message);
        if (variance1_result.is_error())
            return Result<double>::error(variance1_result.error().code, variance1_result.error().message);
        if (variance2_result.is_error())
            return Result<double>::error(variance2_result.error().code, variance2_result.error().message);

        double covariance = covariance_result.value() / values1.size();
        double std1       = std::sqrt(variance1_result.value() / values1.size());
        double std2       = std::sqrt(variance2_result.value() / values2.size());

        if (std1 == 0.0 || std2 == 0.0) {
            return Result<double>::success(0.0);
        }

        return Result<double>::success(covariance / (std1 * std2));
    }

    /**
     * @brief Parallel rolling window operations
     */
    template <typename T, typename WindowOp>
    [[nodiscard]] Result<TimeSeries<T>> parallel_rolling_operation(const TimeSeries<T>& series, size_t window_size,
                                                                   WindowOp op) const {
        if (window_size == 0 || window_size > series.size()) {
            return Result<TimeSeries<T>>::error(ErrorCode::InvalidInput, "Invalid window size");
        }

        const size_t result_size = series.size() - window_size + 1;
        const auto& timestamps   = series.timestamps();
        const auto& values       = series.values();

        if (!should_use_parallel(result_size)) {
            // Serial execution for small datasets
            std::vector<DateTime> result_dates;
            std::vector<T> result_values;

            result_dates.reserve(result_size);
            result_values.reserve(result_size);

            for (size_t i = 0; i < result_size; ++i) {
                auto window_begin = values.begin() + i;
                auto window_end   = window_begin + window_size;

                result_dates.push_back(timestamps[i + window_size - 1]);
                result_values.push_back(op(window_begin, window_end));
            }

            return TimeSeries<T>::create(result_dates, result_values,
                                         series.name() + "_rolling_" + std::to_string(window_size));
        }

        // Parallel execution
        std::vector<DateTime> result_dates(result_size);
        std::vector<T> result_values(result_size);

        const size_t num_threads = std::min(config_.max_threads, result_size);
        const size_t chunk_size  = calculate_chunk_size(result_size, num_threads);

        std::vector<std::future<void>> futures;
        auto& pool = get_global_thread_pool();

        for (size_t i = 0; i < result_size; i += chunk_size) {
            size_t end = std::min(i + chunk_size, result_size);

            futures.push_back(pool.enqueue([&, i, end]() {
                for (size_t j = i; j < end; ++j) {
                    auto window_begin = values.begin() + j;
                    auto window_end   = window_begin + window_size;

                    result_dates[j]  = timestamps[j + window_size - 1];
                    result_values[j] = op(window_begin, window_end);
                }
            }));
        }

        // Wait for completion
        for (auto& future : futures) {
            future.wait();
        }

        return TimeSeries<T>::create(result_dates, result_values,
                                     series.name() + "_rolling_" + std::to_string(window_size));
    }

    /**
     * @brief Parallel rolling mean
     */
    template <typename T>
    [[nodiscard]] Result<TimeSeries<T>> parallel_rolling_mean(const TimeSeries<T>& series, size_t window_size) const {
        return parallel_rolling_operation(series, window_size, [](auto begin, auto end) -> T {
            return std::accumulate(begin, end, T{}) / static_cast<T>(std::distance(begin, end));
        });
    }

    /**
     * @brief Parallel rolling standard deviation
     */
    template <typename T>
    [[nodiscard]] Result<TimeSeries<T>> parallel_rolling_std(const TimeSeries<T>& series, size_t window_size) const {
        return parallel_rolling_operation(series, window_size, [](auto begin, auto end) -> T {
            size_t n = std::distance(begin, end);
            T mean   = std::accumulate(begin, end, T{}) / static_cast<T>(n);

            T variance = std::accumulate(begin, end, T{},
                                         [mean](T acc, T val) {
                                             T diff = val - mean;
                                             return acc + diff * diff;
                                         }) /
                         static_cast<T>(n);

            return std::sqrt(variance);
        });
    }

    /**
     * @brief Get current configuration
     */
    const ParallelConfig& get_config() const { return config_; }

    /**
     * @brief Update configuration
     */
    void update_config(const ParallelConfig& new_config) { config_ = new_config; }

    /**
     * @brief Get performance statistics
     */
    struct PerformanceStats {
        size_t available_threads;
        size_t active_threads;
        double cpu_utilization;
        size_t tasks_completed;
    };

    PerformanceStats get_performance_stats() const {
        return PerformanceStats{
            .available_threads = config_.max_threads,
            .active_threads    = get_global_thread_pool().size(),
            .cpu_utilization   = config_.cpu_utilization_target,
            .tasks_completed   = 0  // Would need thread pool instrumentation
        };
    }
};

/**
 * @brief Global parallel algorithms instance
 */
inline ParallelAlgorithms& get_global_parallel_algorithms() {
    static ParallelAlgorithms algorithms;
    return algorithms;
}

/**
 * @brief Convenience functions for parallel operations
 */
namespace par {

template <typename T>
[[nodiscard]] Result<double> mean(const TimeSeries<T>& series) {
    return get_global_parallel_algorithms().parallel_mean(series);
}

template <typename T>
[[nodiscard]] Result<double> std_deviation(const TimeSeries<T>& series) {
    return get_global_parallel_algorithms().parallel_std_deviation(series);
}

template <typename T>
[[nodiscard]] Result<double> correlation(const TimeSeries<T>& series1, const TimeSeries<T>& series2) {
    return get_global_parallel_algorithms().parallel_correlation(series1, series2);
}

template <typename T>
[[nodiscard]] Result<TimeSeries<T>> rolling_mean(const TimeSeries<T>& series, size_t window_size) {
    return get_global_parallel_algorithms().parallel_rolling_mean(series, window_size);
}

template <typename T>
[[nodiscard]] Result<TimeSeries<T>> rolling_std(const TimeSeries<T>& series, size_t window_size) {
    return get_global_parallel_algorithms().parallel_rolling_std(series, window_size);
}

template <typename T, typename UnaryOp>
[[nodiscard]] Result<std::vector<T>> map(const std::vector<T>& input, UnaryOp op) {
    return get_global_parallel_algorithms().parallel_map(input, op);
}

template <typename T, typename BinaryOp>
[[nodiscard]] Result<T> reduce(const std::vector<T>& input, T init, BinaryOp op) {
    return get_global_parallel_algorithms().parallel_reduce(input, init, op);
}

}  // namespace par

}  // namespace pyfolio::parallel