#include <chrono>
#include <gtest/gtest.h>
#include <iomanip>
#include <pyfolio/core/time_series.h>
#include <pyfolio/math/simd_math.h>
#include <random>

using namespace pyfolio;
using namespace pyfolio::simd;

class SIMDPerformanceTest : public ::testing::Test {
  protected:
    void SetUp() override {
        std::mt19937 gen(42);
        std::uniform_real_distribution<double> dist(-1.0, 1.0);

        // Generate test data of various sizes
        for (auto size : {100, 1000, 10000, 100000}) {
            std::vector<double> data_a(size), data_b(size);
            for (int i = 0; i < size; ++i) {
                data_a[i] = dist(gen);
                data_b[i] = dist(gen);
            }
            test_data_a_[size] = std::move(data_a);
            test_data_b_[size] = std::move(data_b);
        }
    }

    template <typename Func>
    double measure_time(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(end - start).count();
    }

    std::map<size_t, std::vector<double>> test_data_a_, test_data_b_;
};

TEST_F(SIMDPerformanceTest, VectorAdditionPerformance) {
    std::cout << "\n=== Vector Addition Performance ===\n";
    std::cout << std::setw(10) << "Size" << std::setw(15) << "Scalar (μs)" << std::setw(15) << "SIMD (μs)"
              << std::setw(15) << "Speedup" << "\n";

    for (auto size : {100, 1000, 10000, 100000}) {
        const auto& a = test_data_a_[size];
        const auto& b = test_data_b_[size];
        std::vector<double> result_scalar(size), result_simd(size);

        // Scalar version timing
        double scalar_time = measure_time([&]() {
            for (size_t i = 0; i < size; ++i) {
                result_scalar[i] = a[i] + b[i];
            }
        });

        // SIMD version timing
        double simd_time = measure_time([&]() {
            vector_add(std::span<const double>(a), std::span<const double>(b), std::span<double>(result_simd));
        });

        // Verify results are the same
        for (size_t i = 0; i < size; ++i) {
            EXPECT_NEAR(result_scalar[i], result_simd[i], 1e-15);
        }

        double speedup = scalar_time / simd_time;
        std::cout << std::setw(10) << size << std::setw(15) << std::fixed << std::setprecision(2) << scalar_time
                  << std::setw(15) << simd_time << std::setw(15) << speedup << "x\n";
    }
}

TEST_F(SIMDPerformanceTest, DotProductPerformance) {
    std::cout << "\n=== Dot Product Performance ===\n";
    std::cout << std::setw(10) << "Size" << std::setw(15) << "Scalar (μs)" << std::setw(15) << "SIMD (μs)"
              << std::setw(15) << "Speedup" << "\n";

    for (auto size : {100, 1000, 10000, 100000}) {
        const auto& a = test_data_a_[size];
        const auto& b = test_data_b_[size];

        // Scalar version timing
        double scalar_result = 0.0;
        double scalar_time   = measure_time([&]() {
            double sum = 0.0;
            for (size_t i = 0; i < size; ++i) {
                sum += a[i] * b[i];
            }
            scalar_result = sum;
        });

        // SIMD version timing
        double simd_result = 0.0;
        double simd_time =
            measure_time([&]() { simd_result = dot_product(std::span<const double>(a), std::span<const double>(b)); });

        // Verify results are the same
        EXPECT_NEAR(scalar_result, simd_result, 1e-12);

        double speedup = scalar_time / simd_time;
        std::cout << std::setw(10) << size << std::setw(15) << std::fixed << std::setprecision(2) << scalar_time
                  << std::setw(15) << simd_time << std::setw(15) << speedup << "x\n";
    }
}

TEST_F(SIMDPerformanceTest, VectorSumPerformance) {
    std::cout << "\n=== Vector Sum Performance ===\n";
    std::cout << std::setw(10) << "Size" << std::setw(15) << "Scalar (μs)" << std::setw(15) << "SIMD (μs)"
              << std::setw(15) << "Speedup" << "\n";

    for (auto size : {100, 1000, 10000, 100000}) {
        const auto& a = test_data_a_[size];

        // Scalar version timing
        double scalar_result = 0.0;
        double scalar_time   = measure_time([&]() {
            double sum = 0.0;
            for (size_t i = 0; i < size; ++i) {
                sum += a[i];
            }
            scalar_result = sum;
        });

        // SIMD version timing
        double simd_result = 0.0;
        double simd_time   = measure_time([&]() { simd_result = vector_sum(std::span<const double>(a)); });

        // Verify results are the same
        EXPECT_NEAR(scalar_result, simd_result, 1e-12);

        double speedup = scalar_time / simd_time;
        std::cout << std::setw(10) << size << std::setw(15) << std::fixed << std::setprecision(2) << scalar_time
                  << std::setw(15) << simd_time << std::setw(15) << speedup << "x\n";
    }
}

TEST_F(SIMDPerformanceTest, TimeSeriesArithmeticPerformance) {
    std::cout << "\n=== TimeSeries Arithmetic Performance ===\n";
    std::cout << std::setw(10) << "Size" << std::setw(20) << "Standard Addition (μs)" << std::setw(20)
              << "SIMD Addition (μs)" << std::setw(15) << "Speedup" << "\n";

    for (auto size : {100, 1000, 10000}) {
        // Create test time series
        std::vector<DateTime> dates;
        DateTime base_date = DateTime::parse("2024-01-01").value();
        for (size_t i = 0; i < size; ++i) {
            dates.push_back(base_date.add_days(i));
        }

        TimeSeries<double> ts_a(dates, test_data_a_[size], "test_a");
        TimeSeries<double> ts_b(dates, test_data_b_[size], "test_b");

        // Standard addition timing (element-wise manually)
        TimeSeries<double> result_standard;
        double standard_time = measure_time([&]() {
            std::vector<double> result_values(size);
            for (size_t i = 0; i < size; ++i) {
                result_values[i] = test_data_a_[size][i] + test_data_b_[size][i];
            }
            result_standard = TimeSeries<double>(dates, result_values, "result");
        });

        // SIMD addition timing (using overloaded operator)
        TimeSeries<double> result_simd;
        double simd_time = measure_time([&]() {
            auto simd_result = ts_a + ts_b;
            EXPECT_TRUE(simd_result.is_ok());
            result_simd = simd_result.value();
        });

        // Verify results are equivalent
        EXPECT_EQ(result_standard.size(), result_simd.size());
        for (size_t i = 0; i < size; ++i) {
            EXPECT_NEAR(result_standard[i], result_simd[i], 1e-15);
        }

        double speedup = standard_time / simd_time;
        std::cout << std::setw(10) << size << std::setw(20) << std::fixed << std::setprecision(2) << standard_time
                  << std::setw(20) << simd_time << std::setw(15) << speedup << "x\n";
    }
}

TEST_F(SIMDPerformanceTest, CorrelationPerformance) {
    std::cout << "\n=== Correlation Calculation Performance ===\n";
    std::cout << std::setw(10) << "Size" << std::setw(15) << "Standard (μs)" << std::setw(15) << "SIMD (μs)"
              << std::setw(15) << "Speedup" << "\n";

    for (auto size : {100, 1000, 10000}) {
        // Create test time series
        std::vector<DateTime> dates;
        DateTime base_date = DateTime::parse("2024-01-01").value();
        for (size_t i = 0; i < size; ++i) {
            dates.push_back(base_date.add_days(i));
        }

        TimeSeries<double> ts_a(dates, test_data_a_[size], "test_a");
        TimeSeries<double> ts_b(dates, test_data_b_[size], "test_b");

        // Standard correlation timing
        double standard_result = 0.0;
        double standard_time   = measure_time([&]() {
            // Simplified manual correlation calculation
            double mean_a = 0.0, mean_b = 0.0;
            for (size_t i = 0; i < size; ++i) {
                mean_a += test_data_a_[size][i];
                mean_b += test_data_b_[size][i];
            }
            mean_a /= size;
            mean_b /= size;

            double numerator = 0.0, sum_sq_a = 0.0, sum_sq_b = 0.0;
            for (size_t i = 0; i < size; ++i) {
                double diff_a = test_data_a_[size][i] - mean_a;
                double diff_b = test_data_b_[size][i] - mean_b;
                numerator += diff_a * diff_b;
                sum_sq_a += diff_a * diff_a;
                sum_sq_b += diff_b * diff_b;
            }

            standard_result = numerator / std::sqrt(sum_sq_a * sum_sq_b);
        });

        // SIMD correlation timing
        double simd_result = 0.0;
        double simd_time   = measure_time([&]() {
            auto corr_result = ts_a.correlation(ts_b);
            EXPECT_TRUE(corr_result.is_ok());
            simd_result = corr_result.value();
        });

        // Verify results are equivalent
        EXPECT_NEAR(standard_result, simd_result, 1e-12);

        double speedup = standard_time / simd_time;
        std::cout << std::setw(10) << size << std::setw(15) << std::fixed << std::setprecision(2) << standard_time
                  << std::setw(15) << simd_time << std::setw(15) << speedup << "x\n";
    }
}

TEST_F(SIMDPerformanceTest, SIMDCapabilityDetection) {
    const auto& caps = detail::SIMDCapabilities::get();

    std::cout << "\n=== SIMD Capabilities ===\n";
    std::cout << "AVX2 Support: " << (caps.has_avx2 ? "Yes" : "No") << "\n";
    std::cout << "SSE2 Support: " << (caps.has_sse2 ? "Yes" : "No") << "\n";
    std::cout << "NEON Support: " << (caps.has_neon ? "Yes" : "No") << "\n";

    // Basic functionality test
    std::vector<double> a = {1.0, 2.0, 3.0, 4.0};
    std::vector<double> b = {5.0, 6.0, 7.0, 8.0};
    std::vector<double> result(4);

    vector_add(std::span<const double>(a), std::span<const double>(b), std::span<double>(result));

    EXPECT_DOUBLE_EQ(result[0], 6.0);
    EXPECT_DOUBLE_EQ(result[1], 8.0);
    EXPECT_DOUBLE_EQ(result[2], 10.0);
    EXPECT_DOUBLE_EQ(result[3], 12.0);

    std::cout << "SIMD functionality verified ✓\n";
}