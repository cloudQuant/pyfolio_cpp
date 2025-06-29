#pragma once

#include "../core/types.h"
#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

// SIMD intrinsics headers
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#define PYFOLIO_HAS_AVX2 1
#define PYFOLIO_HAS_SSE2 1
#elif defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#define PYFOLIO_HAS_NEON 1
#endif

namespace pyfolio::simd {

/**
 * @brief SIMD-optimized mathematical operations for financial computations
 *
 * This module provides vectorized implementations of common operations
 * used in portfolio analysis, dramatically improving performance for
 * large datasets.
 */

// Configuration constants
constexpr size_t SIMD_ALIGNMENT = 32;  // AVX2 alignment
constexpr size_t AVX2_DOUBLES   = 4;   // 4 doubles per AVX2 register
constexpr size_t SSE2_DOUBLES   = 2;   // 2 doubles per SSE2 register
constexpr size_t NEON_DOUBLES   = 2;   // 2 doubles per NEON register

/**
 * @brief Check if pointer is properly aligned for SIMD operations
 */
inline bool is_aligned(const void* ptr, size_t alignment = SIMD_ALIGNMENT) noexcept {
    return (reinterpret_cast<uintptr_t>(ptr) % alignment) == 0;
}

/**
 * @brief SIMD-optimized vector addition: result[i] = a[i] + b[i]
 */
void vector_add(std::span<const double> a, std::span<const double> b, std::span<double> result) noexcept;

/**
 * @brief SIMD-optimized vector subtraction: result[i] = a[i] - b[i]
 */
void vector_subtract(std::span<const double> a, std::span<const double> b, std::span<double> result) noexcept;

/**
 * @brief SIMD-optimized vector multiplication: result[i] = a[i] * b[i]
 */
void vector_multiply(std::span<const double> a, std::span<const double> b, std::span<double> result) noexcept;

/**
 * @brief SIMD-optimized scalar multiplication: result[i] = a[i] * scalar
 */
void vector_scale(std::span<const double> a, double scalar, std::span<double> result) noexcept;

/**
 * @brief SIMD-optimized dot product: returns sum(a[i] * b[i])
 */
double dot_product(std::span<const double> a, std::span<const double> b) noexcept;

/**
 * @brief SIMD-optimized sum: returns sum(a[i])
 */
double vector_sum(std::span<const double> a) noexcept;

/**
 * @brief SIMD-optimized mean calculation
 */
double vector_mean(std::span<const double> a) noexcept;

/**
 * @brief SIMD-optimized variance calculation (sample variance)
 */
double vector_variance(std::span<const double> a, double mean) noexcept;

/**
 * @brief SIMD-optimized standard deviation calculation
 */
double vector_std(std::span<const double> a, double mean) noexcept;

/**
 * @brief SIMD-optimized rolling sum with window size
 */
void rolling_sum_simd(std::span<const double> data, size_t window, std::span<double> result) noexcept;

/**
 * @brief SIMD-optimized exponential moving average
 */
void exponential_moving_average_simd(std::span<const double> data, double alpha, std::span<double> result) noexcept;

/**
 * @brief SIMD-optimized returns calculation: result[i] = (data[i] - data[i-1]) / data[i-1]
 */
void calculate_returns_simd(std::span<const double> prices, std::span<double> returns) noexcept;

/**
 * @brief SIMD-optimized cumulative product calculation
 */
void cumulative_product_simd(std::span<const double> data, std::span<double> result) noexcept;

/**
 * @brief SIMD-optimized correlation coefficient calculation
 */
double correlation_simd(std::span<const double> x, std::span<const double> y) noexcept;

/**
 * @brief SIMD-optimized covariance calculation
 */
double covariance_simd(std::span<const double> x, std::span<const double> y, double mean_x, double mean_y) noexcept;

// Implementation details for different SIMD instruction sets
namespace detail {

#ifdef PYFOLIO_HAS_AVX2
/**
 * @brief AVX2 implementations for x86-64 processors
 */
namespace avx2 {
void vector_add_avx2(const double* a, const double* b, double* result, size_t n) noexcept;
void vector_subtract_avx2(const double* a, const double* b, double* result, size_t n) noexcept;
void vector_multiply_avx2(const double* a, const double* b, double* result, size_t n) noexcept;
void vector_scale_avx2(const double* a, double scalar, double* result, size_t n) noexcept;
double dot_product_avx2(const double* a, const double* b, size_t n) noexcept;
double vector_sum_avx2(const double* data, size_t n) noexcept;
}  // namespace avx2
#endif

#ifdef PYFOLIO_HAS_SSE2
/**
 * @brief SSE2 implementations for x86 processors
 */
namespace sse2 {
void vector_add_sse2(const double* a, const double* b, double* result, size_t n) noexcept;
void vector_subtract_sse2(const double* a, const double* b, double* result, size_t n) noexcept;
void vector_multiply_sse2(const double* a, const double* b, double* result, size_t n) noexcept;
void vector_scale_sse2(const double* a, double scalar, double* result, size_t n) noexcept;
double dot_product_sse2(const double* a, const double* b, size_t n) noexcept;
double vector_sum_sse2(const double* data, size_t n) noexcept;
}  // namespace sse2
#endif

#ifdef PYFOLIO_HAS_NEON
/**
 * @brief NEON implementations for ARM processors
 */
namespace neon {
void vector_add_neon(const double* a, const double* b, double* result, size_t n) noexcept;
void vector_subtract_neon(const double* a, const double* b, double* result, size_t n) noexcept;
void vector_multiply_neon(const double* a, const double* b, double* result, size_t n) noexcept;
void vector_scale_neon(const double* a, double scalar, double* result, size_t n) noexcept;
double dot_product_neon(const double* a, const double* b, size_t n) noexcept;
double vector_sum_neon(const double* data, size_t n) noexcept;
}  // namespace neon
#endif

/**
 * @brief Scalar fallback implementations
 */
namespace scalar {
void vector_add_scalar(const double* a, const double* b, double* result, size_t n) noexcept;
void vector_subtract_scalar(const double* a, const double* b, double* result, size_t n) noexcept;
void vector_multiply_scalar(const double* a, const double* b, double* result, size_t n) noexcept;
void vector_scale_scalar(const double* a, double scalar, double* result, size_t n) noexcept;
double dot_product_scalar(const double* a, const double* b, size_t n) noexcept;
double vector_sum_scalar(const double* data, size_t n) noexcept;
}  // namespace scalar

/**
 * @brief Runtime SIMD capability detection
 */
struct SIMDCapabilities {
    bool has_avx2;
    bool has_sse2;
    bool has_neon;

    SIMDCapabilities() noexcept;
    static const SIMDCapabilities& get() noexcept;
};

}  // namespace detail

/**
 * @brief SIMD-accelerated TimeSeries operations
 *
 * These functions provide drop-in replacements for common TimeSeries
 * operations with automatic SIMD optimization.
 */
namespace timeseries {

/**
 * @brief Fast element-wise addition of two time series
 */
template <typename T>
void add_series(const std::vector<T>& a, const std::vector<T>& b, std::vector<T>& result)
    requires std::floating_point<T>
{
    size_t n = std::min({a.size(), b.size(), result.size()});
    if constexpr (std::is_same_v<T, double>) {
        vector_add(std::span<const double>(a.data(), n), std::span<const double>(b.data(), n),
                   std::span<double>(result.data(), n));
    } else {
        // Fallback for other types
        std::transform(a.begin(), a.begin() + n, b.begin(), result.begin(), std::plus<T>{});
    }
}

/**
 * @brief Fast rolling statistics calculation
 */
template <typename T>
void rolling_mean_simd(const std::vector<T>& data, size_t window, std::vector<T>& result)
    requires std::floating_point<T>
{
    if constexpr (std::is_same_v<T, double>) {
        rolling_sum_simd(std::span<const double>(data.data(), data.size()), window,
                         std::span<double>(result.data(), result.size()));

        // Convert sums to means
        double window_inv = 1.0 / static_cast<double>(window);
        vector_scale(std::span<double>(result.data() + window - 1, result.size() - window + 1), window_inv,
                     std::span<double>(result.data() + window - 1, result.size() - window + 1));
    } else {
        // Fallback implementation for non-double types
        // ... implement standard rolling mean
    }
}

/**
 * @brief Fast correlation calculation between two series
 */
template <typename T>
double correlation_fast(const std::vector<T>& x, const std::vector<T>& y)
    requires std::floating_point<T>
{
    if constexpr (std::is_same_v<T, double>) {
        return correlation_simd(std::span<const double>(x.data(), x.size()),
                                std::span<const double>(y.data(), y.size()));
    } else {
        // Fallback implementation
        return 0.0;  // TODO: implement fallback
    }
}

}  // namespace timeseries

}  // namespace pyfolio::simd