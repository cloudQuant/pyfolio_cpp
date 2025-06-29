#include "../../include/pyfolio/math/simd_math.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace pyfolio::simd {

// Global SIMD capabilities detection
namespace detail {

SIMDCapabilities::SIMDCapabilities() noexcept {
#ifdef PYFOLIO_HAS_AVX2
// Check for AVX2 support at runtime
#if defined(__GNUC__) || defined(__clang__)
    __builtin_cpu_init();
    has_avx2 = __builtin_cpu_supports("avx2");
#else
    has_avx2 = true;  // Assume support for MSVC
#endif
#else
    has_avx2 = false;
#endif

#ifdef PYFOLIO_HAS_SSE2
    has_sse2 = true;  // SSE2 is baseline for x86-64
#else
    has_sse2 = false;
#endif

#ifdef PYFOLIO_HAS_NEON
    has_neon = true;  // NEON is standard on modern ARM
#else
    has_neon = false;
#endif
}

const SIMDCapabilities& SIMDCapabilities::get() noexcept {
    static const SIMDCapabilities caps;
    return caps;
}

}  // namespace detail

// Public API implementations that dispatch to optimal SIMD version

void vector_add(std::span<const double> a, std::span<const double> b, std::span<double> result) noexcept {
    const size_t n = std::min({a.size(), b.size(), result.size()});
    if (n == 0)
        return;

    const auto& caps = detail::SIMDCapabilities::get();

#ifdef PYFOLIO_HAS_AVX2
    if (caps.has_avx2) {
        detail::avx2::vector_add_avx2(a.data(), b.data(), result.data(), n);
        return;
    }
#endif

#ifdef PYFOLIO_HAS_SSE2
    if (caps.has_sse2) {
        detail::sse2::vector_add_sse2(a.data(), b.data(), result.data(), n);
        return;
    }
#endif

#ifdef PYFOLIO_HAS_NEON
    if (caps.has_neon) {
        detail::neon::vector_add_neon(a.data(), b.data(), result.data(), n);
        return;
    }
#endif

    // Fallback to scalar implementation
    detail::scalar::vector_add_scalar(a.data(), b.data(), result.data(), n);
}

void vector_subtract(std::span<const double> a, std::span<const double> b, std::span<double> result) noexcept {
    const size_t n = std::min({a.size(), b.size(), result.size()});
    if (n == 0)
        return;

    const auto& caps = detail::SIMDCapabilities::get();

#ifdef PYFOLIO_HAS_AVX2
    if (caps.has_avx2) {
        detail::avx2::vector_subtract_avx2(a.data(), b.data(), result.data(), n);
        return;
    }
#endif

#ifdef PYFOLIO_HAS_SSE2
    if (caps.has_sse2) {
        detail::sse2::vector_subtract_sse2(a.data(), b.data(), result.data(), n);
        return;
    }
#endif

#ifdef PYFOLIO_HAS_NEON
    if (caps.has_neon) {
        detail::neon::vector_subtract_neon(a.data(), b.data(), result.data(), n);
        return;
    }
#endif

    detail::scalar::vector_subtract_scalar(a.data(), b.data(), result.data(), n);
}

void vector_multiply(std::span<const double> a, std::span<const double> b, std::span<double> result) noexcept {
    const size_t n = std::min({a.size(), b.size(), result.size()});
    if (n == 0)
        return;

    const auto& caps = detail::SIMDCapabilities::get();

#ifdef PYFOLIO_HAS_AVX2
    if (caps.has_avx2) {
        detail::avx2::vector_multiply_avx2(a.data(), b.data(), result.data(), n);
        return;
    }
#endif

#ifdef PYFOLIO_HAS_SSE2
    if (caps.has_sse2) {
        detail::sse2::vector_multiply_sse2(a.data(), b.data(), result.data(), n);
        return;
    }
#endif

#ifdef PYFOLIO_HAS_NEON
    if (caps.has_neon) {
        detail::neon::vector_multiply_neon(a.data(), b.data(), result.data(), n);
        return;
    }
#endif

    detail::scalar::vector_multiply_scalar(a.data(), b.data(), result.data(), n);
}

void vector_scale(std::span<const double> a, double scalar, std::span<double> result) noexcept {
    const size_t n = std::min(a.size(), result.size());
    if (n == 0)
        return;

    const auto& caps = detail::SIMDCapabilities::get();

#ifdef PYFOLIO_HAS_AVX2
    if (caps.has_avx2) {
        detail::avx2::vector_scale_avx2(a.data(), scalar, result.data(), n);
        return;
    }
#endif

#ifdef PYFOLIO_HAS_SSE2
    if (caps.has_sse2) {
        detail::sse2::vector_scale_sse2(a.data(), scalar, result.data(), n);
        return;
    }
#endif

#ifdef PYFOLIO_HAS_NEON
    if (caps.has_neon) {
        detail::neon::vector_scale_neon(a.data(), scalar, result.data(), n);
        return;
    }
#endif

    detail::scalar::vector_scale_scalar(a.data(), scalar, result.data(), n);
}

double dot_product(std::span<const double> a, std::span<const double> b) noexcept {
    const size_t n = std::min(a.size(), b.size());
    if (n == 0)
        return 0.0;

    const auto& caps = detail::SIMDCapabilities::get();

#ifdef PYFOLIO_HAS_AVX2
    if (caps.has_avx2) {
        return detail::avx2::dot_product_avx2(a.data(), b.data(), n);
    }
#endif

#ifdef PYFOLIO_HAS_SSE2
    if (caps.has_sse2) {
        return detail::sse2::dot_product_sse2(a.data(), b.data(), n);
    }
#endif

#ifdef PYFOLIO_HAS_NEON
    if (caps.has_neon) {
        return detail::neon::dot_product_neon(a.data(), b.data(), n);
    }
#endif

    return detail::scalar::dot_product_scalar(a.data(), b.data(), n);
}

double vector_sum(std::span<const double> a) noexcept {
    if (a.empty())
        return 0.0;

    const auto& caps = detail::SIMDCapabilities::get();

#ifdef PYFOLIO_HAS_AVX2
    if (caps.has_avx2) {
        return detail::avx2::vector_sum_avx2(a.data(), a.size());
    }
#endif

#ifdef PYFOLIO_HAS_SSE2
    if (caps.has_sse2) {
        return detail::sse2::vector_sum_sse2(a.data(), a.size());
    }
#endif

#ifdef PYFOLIO_HAS_NEON
    if (caps.has_neon) {
        return detail::neon::vector_sum_neon(a.data(), a.size());
    }
#endif

    return detail::scalar::vector_sum_scalar(a.data(), a.size());
}

double vector_mean(std::span<const double> a) noexcept {
    if (a.empty())
        return 0.0;
    return vector_sum(a) / static_cast<double>(a.size());
}

double vector_variance(std::span<const double> a, double mean) noexcept {
    if (a.size() < 2)
        return 0.0;

    double sum_sq_diff = 0.0;

    // Vectorized computation: (x - mean)^2
    std::vector<double> temp(a.size());
    std::vector<double> mean_vec(a.size(), mean);

    vector_subtract(a, std::span<const double>(mean_vec), std::span<double>(temp));
    vector_multiply(std::span<const double>(temp), std::span<const double>(temp), std::span<double>(temp));
    sum_sq_diff = vector_sum(std::span<const double>(temp));

    return sum_sq_diff / static_cast<double>(a.size() - 1);  // Sample variance
}

double vector_std(std::span<const double> a, double mean) noexcept {
    return std::sqrt(vector_variance(a, mean));
}

void rolling_sum_simd(std::span<const double> data, size_t window, std::span<double> result) noexcept {
    if (data.size() < window || result.empty())
        return;

    // Initialize first window sum
    double sum = vector_sum(data.subspan(0, window));

    // Fill initial NaN values
    const double nan_val = std::numeric_limits<double>::quiet_NaN();
    std::fill_n(result.data(), window - 1, nan_val);

    // Set first complete window result
    if (result.size() > window - 1) {
        result[window - 1] = sum;
    }

    // Rolling computation for remaining windows
    for (size_t i = window; i < data.size() && i < result.size(); ++i) {
        sum       = sum - data[i - window] + data[i];
        result[i] = sum;
    }
}

void exponential_moving_average_simd(std::span<const double> data, double alpha, std::span<double> result) noexcept {
    if (data.empty() || result.empty())
        return;

    const size_t n    = std::min(data.size(), result.size());
    const double beta = 1.0 - alpha;

    // Initialize with first value
    result[0] = data[0];

    // Compute EMA iteratively (cannot easily vectorize due to data dependency)
    for (size_t i = 1; i < n; ++i) {
        result[i] = alpha * data[i] + beta * result[i - 1];
    }
}

void calculate_returns_simd(std::span<const double> prices, std::span<double> returns) noexcept {
    if (prices.size() < 2 || returns.empty())
        return;

    const size_t n = std::min(prices.size() - 1, returns.size());

    // Vectorized computation: (price[i] - price[i-1]) / price[i-1]
    std::vector<double> price_diff(n);

    // Calculate price differences: price[i] - price[i-1]
    vector_subtract(prices.subspan(1, n), prices.subspan(0, n), std::span<double>(price_diff));

    // Divide by previous prices: diff / price[i-1]
    vector_multiply(std::span<const double>(price_diff), prices.subspan(0, n), returns.subspan(0, n));
}

void cumulative_product_simd(std::span<const double> data, std::span<double> result) noexcept {
    if (data.empty() || result.empty())
        return;

    const size_t n = std::min(data.size(), result.size());

    // Initialize with first value
    result[0] = data[0];

    // Cumulative product cannot be easily vectorized due to data dependency
    for (size_t i = 1; i < n; ++i) {
        result[i] = result[i - 1] * data[i];
    }
}

double correlation_simd(std::span<const double> x, std::span<const double> y) noexcept {
    const size_t n = std::min(x.size(), y.size());
    if (n < 2)
        return 0.0;

    const double mean_x = vector_mean(x.subspan(0, n));
    const double mean_y = vector_mean(y.subspan(0, n));

    return covariance_simd(x.subspan(0, n), y.subspan(0, n), mean_x, mean_y) /
           (vector_std(x.subspan(0, n), mean_x) * vector_std(y.subspan(0, n), mean_y));
}

double covariance_simd(std::span<const double> x, std::span<const double> y, double mean_x, double mean_y) noexcept {
    const size_t n = std::min(x.size(), y.size());
    if (n < 2)
        return 0.0;

    // Vectorized computation: (x - mean_x) * (y - mean_y)
    std::vector<double> x_centered(n), y_centered(n), products(n);
    std::vector<double> mean_x_vec(n, mean_x), mean_y_vec(n, mean_y);

    vector_subtract(x, std::span<const double>(mean_x_vec), std::span<double>(x_centered));
    vector_subtract(y, std::span<const double>(mean_y_vec), std::span<double>(y_centered));
    vector_multiply(std::span<const double>(x_centered), std::span<const double>(y_centered),
                    std::span<double>(products));

    return vector_sum(std::span<const double>(products)) / static_cast<double>(n - 1);
}

// Implementation of SIMD-specific functions
namespace detail {

// Scalar fallback implementations
namespace scalar {

void vector_add_scalar(const double* a, const double* b, double* result, size_t n) noexcept {
    for (size_t i = 0; i < n; ++i) {
        result[i] = a[i] + b[i];
    }
}

void vector_subtract_scalar(const double* a, const double* b, double* result, size_t n) noexcept {
    for (size_t i = 0; i < n; ++i) {
        result[i] = a[i] - b[i];
    }
}

void vector_multiply_scalar(const double* a, const double* b, double* result, size_t n) noexcept {
    for (size_t i = 0; i < n; ++i) {
        result[i] = a[i] * b[i];
    }
}

void vector_scale_scalar(const double* a, double scalar, double* result, size_t n) noexcept {
    for (size_t i = 0; i < n; ++i) {
        result[i] = a[i] * scalar;
    }
}

double dot_product_scalar(const double* a, const double* b, size_t n) noexcept {
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

double vector_sum_scalar(const double* data, size_t n) noexcept {
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        sum += data[i];
    }
    return sum;
}

}  // namespace scalar

// AVX2 implementations for x86-64
#ifdef PYFOLIO_HAS_AVX2
namespace avx2 {

void vector_add_avx2(const double* a, const double* b, double* result, size_t n) noexcept {
    const size_t simd_n = (n / AVX2_DOUBLES) * AVX2_DOUBLES;

    // Process 4 doubles at a time with AVX2
    for (size_t i = 0; i < simd_n; i += AVX2_DOUBLES) {
        __m256d va = _mm256_load_pd(&a[i]);
        __m256d vb = _mm256_load_pd(&b[i]);
        __m256d vr = _mm256_add_pd(va, vb);
        _mm256_store_pd(&result[i], vr);
    }

    // Handle remaining elements
    for (size_t i = simd_n; i < n; ++i) {
        result[i] = a[i] + b[i];
    }
}

void vector_subtract_avx2(const double* a, const double* b, double* result, size_t n) noexcept {
    const size_t simd_n = (n / AVX2_DOUBLES) * AVX2_DOUBLES;

    for (size_t i = 0; i < simd_n; i += AVX2_DOUBLES) {
        __m256d va = _mm256_load_pd(&a[i]);
        __m256d vb = _mm256_load_pd(&b[i]);
        __m256d vr = _mm256_sub_pd(va, vb);
        _mm256_store_pd(&result[i], vr);
    }

    for (size_t i = simd_n; i < n; ++i) {
        result[i] = a[i] - b[i];
    }
}

void vector_multiply_avx2(const double* a, const double* b, double* result, size_t n) noexcept {
    const size_t simd_n = (n / AVX2_DOUBLES) * AVX2_DOUBLES;

    for (size_t i = 0; i < simd_n; i += AVX2_DOUBLES) {
        __m256d va = _mm256_load_pd(&a[i]);
        __m256d vb = _mm256_load_pd(&b[i]);
        __m256d vr = _mm256_mul_pd(va, vb);
        _mm256_store_pd(&result[i], vr);
    }

    for (size_t i = simd_n; i < n; ++i) {
        result[i] = a[i] * b[i];
    }
}

void vector_scale_avx2(const double* a, double scalar, double* result, size_t n) noexcept {
    const size_t simd_n = (n / AVX2_DOUBLES) * AVX2_DOUBLES;
    const __m256d vs    = _mm256_set1_pd(scalar);

    for (size_t i = 0; i < simd_n; i += AVX2_DOUBLES) {
        __m256d va = _mm256_load_pd(&a[i]);
        __m256d vr = _mm256_mul_pd(va, vs);
        _mm256_store_pd(&result[i], vr);
    }

    for (size_t i = simd_n; i < n; ++i) {
        result[i] = a[i] * scalar;
    }
}

double dot_product_avx2(const double* a, const double* b, size_t n) noexcept {
    const size_t simd_n = (n / AVX2_DOUBLES) * AVX2_DOUBLES;
    __m256d sum_vec     = _mm256_setzero_pd();

    for (size_t i = 0; i < simd_n; i += AVX2_DOUBLES) {
        __m256d va   = _mm256_load_pd(&a[i]);
        __m256d vb   = _mm256_load_pd(&b[i]);
        __m256d prod = _mm256_mul_pd(va, vb);
        sum_vec      = _mm256_add_pd(sum_vec, prod);
    }

    // Horizontal sum of 4 doubles
    double sum_array[4];
    _mm256_store_pd(sum_array, sum_vec);
    double sum = sum_array[0] + sum_array[1] + sum_array[2] + sum_array[3];

    // Handle remaining elements
    for (size_t i = simd_n; i < n; ++i) {
        sum += a[i] * b[i];
    }

    return sum;
}

double vector_sum_avx2(const double* data, size_t n) noexcept {
    const size_t simd_n = (n / AVX2_DOUBLES) * AVX2_DOUBLES;
    __m256d sum_vec     = _mm256_setzero_pd();

    for (size_t i = 0; i < simd_n; i += AVX2_DOUBLES) {
        __m256d vd = _mm256_load_pd(&data[i]);
        sum_vec    = _mm256_add_pd(sum_vec, vd);
    }

    // Horizontal sum
    double sum_array[4];
    _mm256_store_pd(sum_array, sum_vec);
    double sum = sum_array[0] + sum_array[1] + sum_array[2] + sum_array[3];

    // Handle remaining elements
    for (size_t i = simd_n; i < n; ++i) {
        sum += data[i];
    }

    return sum;
}

}  // namespace avx2
#endif  // PYFOLIO_HAS_AVX2

// SSE2 implementations (similar structure but with 2 doubles per operation)
#ifdef PYFOLIO_HAS_SSE2
namespace sse2 {

void vector_add_sse2(const double* a, const double* b, double* result, size_t n) noexcept {
    const size_t simd_n = (n / SSE2_DOUBLES) * SSE2_DOUBLES;

    for (size_t i = 0; i < simd_n; i += SSE2_DOUBLES) {
        __m128d va = _mm_load_pd(&a[i]);
        __m128d vb = _mm_load_pd(&b[i]);
        __m128d vr = _mm_add_pd(va, vb);
        _mm_store_pd(&result[i], vr);
    }

    for (size_t i = simd_n; i < n; ++i) {
        result[i] = a[i] + b[i];
    }
}

void vector_subtract_sse2(const double* a, const double* b, double* result, size_t n) noexcept {
    const size_t simd_n = (n / SSE2_DOUBLES) * SSE2_DOUBLES;

    for (size_t i = 0; i < simd_n; i += SSE2_DOUBLES) {
        __m128d va = _mm_load_pd(&a[i]);
        __m128d vb = _mm_load_pd(&b[i]);
        __m128d vr = _mm_sub_pd(va, vb);
        _mm_store_pd(&result[i], vr);
    }

    for (size_t i = simd_n; i < n; ++i) {
        result[i] = a[i] - b[i];
    }
}

void vector_multiply_sse2(const double* a, const double* b, double* result, size_t n) noexcept {
    const size_t simd_n = (n / SSE2_DOUBLES) * SSE2_DOUBLES;

    for (size_t i = 0; i < simd_n; i += SSE2_DOUBLES) {
        __m128d va = _mm_load_pd(&a[i]);
        __m128d vb = _mm_load_pd(&b[i]);
        __m128d vr = _mm_mul_pd(va, vb);
        _mm_store_pd(&result[i], vr);
    }

    for (size_t i = simd_n; i < n; ++i) {
        result[i] = a[i] * b[i];
    }
}

void vector_scale_sse2(const double* a, double scalar, double* result, size_t n) noexcept {
    const size_t simd_n = (n / SSE2_DOUBLES) * SSE2_DOUBLES;
    const __m128d vs    = _mm_set1_pd(scalar);

    for (size_t i = 0; i < simd_n; i += SSE2_DOUBLES) {
        __m128d va = _mm_load_pd(&a[i]);
        __m128d vr = _mm_mul_pd(va, vs);
        _mm_store_pd(&result[i], vr);
    }

    for (size_t i = simd_n; i < n; ++i) {
        result[i] = a[i] * scalar;
    }
}

double dot_product_sse2(const double* a, const double* b, size_t n) noexcept {
    const size_t simd_n = (n / SSE2_DOUBLES) * SSE2_DOUBLES;
    __m128d sum_vec     = _mm_setzero_pd();

    for (size_t i = 0; i < simd_n; i += SSE2_DOUBLES) {
        __m128d va   = _mm_load_pd(&a[i]);
        __m128d vb   = _mm_load_pd(&b[i]);
        __m128d prod = _mm_mul_pd(va, vb);
        sum_vec      = _mm_add_pd(sum_vec, prod);
    }

    // Extract and sum the two doubles
    double sum_array[2];
    _mm_store_pd(sum_array, sum_vec);
    double sum = sum_array[0] + sum_array[1];

    for (size_t i = simd_n; i < n; ++i) {
        sum += a[i] * b[i];
    }

    return sum;
}

double vector_sum_sse2(const double* data, size_t n) noexcept {
    const size_t simd_n = (n / SSE2_DOUBLES) * SSE2_DOUBLES;
    __m128d sum_vec     = _mm_setzero_pd();

    for (size_t i = 0; i < simd_n; i += SSE2_DOUBLES) {
        __m128d vd = _mm_load_pd(&data[i]);
        sum_vec    = _mm_add_pd(sum_vec, vd);
    }

    double sum_array[2];
    _mm_store_pd(sum_array, sum_vec);
    double sum = sum_array[0] + sum_array[1];

    for (size_t i = simd_n; i < n; ++i) {
        sum += data[i];
    }

    return sum;
}

}  // namespace sse2
#endif  // PYFOLIO_HAS_SSE2

// NEON implementations for ARM
#ifdef PYFOLIO_HAS_NEON
namespace neon {

void vector_add_neon(const double* a, const double* b, double* result, size_t n) noexcept {
    const size_t simd_n = (n / NEON_DOUBLES) * NEON_DOUBLES;

    for (size_t i = 0; i < simd_n; i += NEON_DOUBLES) {
        float64x2_t va = vld1q_f64(&a[i]);
        float64x2_t vb = vld1q_f64(&b[i]);
        float64x2_t vr = vaddq_f64(va, vb);
        vst1q_f64(&result[i], vr);
    }

    for (size_t i = simd_n; i < n; ++i) {
        result[i] = a[i] + b[i];
    }
}

void vector_subtract_neon(const double* a, const double* b, double* result, size_t n) noexcept {
    const size_t simd_n = (n / NEON_DOUBLES) * NEON_DOUBLES;

    for (size_t i = 0; i < simd_n; i += NEON_DOUBLES) {
        float64x2_t va = vld1q_f64(&a[i]);
        float64x2_t vb = vld1q_f64(&b[i]);
        float64x2_t vr = vsubq_f64(va, vb);
        vst1q_f64(&result[i], vr);
    }

    for (size_t i = simd_n; i < n; ++i) {
        result[i] = a[i] - b[i];
    }
}

void vector_multiply_neon(const double* a, const double* b, double* result, size_t n) noexcept {
    const size_t simd_n = (n / NEON_DOUBLES) * NEON_DOUBLES;

    for (size_t i = 0; i < simd_n; i += NEON_DOUBLES) {
        float64x2_t va = vld1q_f64(&a[i]);
        float64x2_t vb = vld1q_f64(&b[i]);
        float64x2_t vr = vmulq_f64(va, vb);
        vst1q_f64(&result[i], vr);
    }

    for (size_t i = simd_n; i < n; ++i) {
        result[i] = a[i] * b[i];
    }
}

void vector_scale_neon(const double* a, double scalar, double* result, size_t n) noexcept {
    const size_t simd_n  = (n / NEON_DOUBLES) * NEON_DOUBLES;
    const float64x2_t vs = vdupq_n_f64(scalar);

    for (size_t i = 0; i < simd_n; i += NEON_DOUBLES) {
        float64x2_t va = vld1q_f64(&a[i]);
        float64x2_t vr = vmulq_f64(va, vs);
        vst1q_f64(&result[i], vr);
    }

    for (size_t i = simd_n; i < n; ++i) {
        result[i] = a[i] * scalar;
    }
}

double dot_product_neon(const double* a, const double* b, size_t n) noexcept {
    const size_t simd_n = (n / NEON_DOUBLES) * NEON_DOUBLES;
    float64x2_t sum_vec = vdupq_n_f64(0.0);

    for (size_t i = 0; i < simd_n; i += NEON_DOUBLES) {
        float64x2_t va   = vld1q_f64(&a[i]);
        float64x2_t vb   = vld1q_f64(&b[i]);
        float64x2_t prod = vmulq_f64(va, vb);
        sum_vec          = vaddq_f64(sum_vec, prod);
    }

    // Extract and sum the two doubles
    double sum = vgetq_lane_f64(sum_vec, 0) + vgetq_lane_f64(sum_vec, 1);

    for (size_t i = simd_n; i < n; ++i) {
        sum += a[i] * b[i];
    }

    return sum;
}

double vector_sum_neon(const double* data, size_t n) noexcept {
    const size_t simd_n = (n / NEON_DOUBLES) * NEON_DOUBLES;
    float64x2_t sum_vec = vdupq_n_f64(0.0);

    for (size_t i = 0; i < simd_n; i += NEON_DOUBLES) {
        float64x2_t vd = vld1q_f64(&data[i]);
        sum_vec        = vaddq_f64(sum_vec, vd);
    }

    double sum = vgetq_lane_f64(sum_vec, 0) + vgetq_lane_f64(sum_vec, 1);

    for (size_t i = simd_n; i < n; ++i) {
        sum += data[i];
    }

    return sum;
}

}  // namespace neon
#endif  // PYFOLIO_HAS_NEON

}  // namespace detail

}  // namespace pyfolio::simd