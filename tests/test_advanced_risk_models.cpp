#include <gtest/gtest.h>
#include <pyfolio/risk/advanced_risk_models.h>
#include <pyfolio/core/time_series.h>
#include <vector>
#include <random>
#include <cmath>

using namespace pyfolio::risk;
using namespace pyfolio;

/**
 * @brief Test fixture for advanced risk models
 */
class AdvancedRiskModelsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate sample data for testing
        sample_returns_ = generate_test_returns(500);
        extreme_returns_ = generate_extreme_returns(1000);
    }
    
    /**
     * @brief Generate sample returns with GARCH-like properties
     */
    TimeSeries<double> generate_test_returns(size_t n_obs) {
        std::mt19937 gen(42); // Fixed seed for reproducibility
        std::normal_distribution<double> normal(0.0, 1.0);
        
        std::vector<double> returns;
        std::vector<DateTime> dates;
        
        // GARCH(1,1) simulation
        double omega = 0.00001;
        double alpha = 0.05;
        double beta = 0.90;
        double h_t = omega / (1.0 - alpha - beta);
        
        for (size_t t = 0; t < n_obs; ++t) {
            double epsilon = normal(gen);
            double return_t = std::sqrt(h_t) * epsilon;
            returns.push_back(return_t);
            dates.push_back(DateTime::parse("2020-01-01").value().add_days(t));
            
            // Update volatility
            h_t = omega + alpha * return_t * return_t + beta * h_t;
        }
        
        return TimeSeries<double>(dates, returns);
    }
    
    /**
     * @brief Generate returns with extreme events
     */
    TimeSeries<double> generate_extreme_returns(size_t n_obs) {
        std::mt19937 gen(123);
        std::normal_distribution<double> normal(0.0, 0.015);
        std::normal_distribution<double> extreme(-0.08, 0.03);
        std::uniform_real_distribution<double> uniform(0.0, 1.0);
        
        std::vector<double> returns;
        std::vector<DateTime> dates;
        
        for (size_t t = 0; t < n_obs; ++t) {
            double return_t;
            if (uniform(gen) < 0.02) { // 2% extreme events
                return_t = extreme(gen);
            } else {
                return_t = normal(gen);
            }
            returns.push_back(return_t);
            dates.push_back(DateTime::parse("2020-01-01").value().add_days(t));
        }
        
        return TimeSeries<double>(dates, returns);
    }
    
    TimeSeries<double> sample_returns_;
    TimeSeries<double> extreme_returns_;
    
    const double TOLERANCE = 1e-6;
    const double VAR_TOLERANCE = 0.05; // 5% tolerance for VaR estimates
};

/**
 * @brief Test GARCH model basic functionality
 */
TEST_F(AdvancedRiskModelsTest, GARCHModelBasicFitting) {
    GARCHModel garch(GARCHType::GARCH, 1, 1);
    
    auto fit_result = garch.fit(sample_returns_, "normal");
    ASSERT_TRUE(fit_result.is_ok());
    
    const auto& params = fit_result.value();
    
    // Check parameter constraints
    EXPECT_GT(params.omega, 0.0);
    EXPECT_GE(params.alpha[0], 0.0);
    EXPECT_GE(params.beta[0], 0.0);
    EXPECT_LT(params.alpha[0] + params.beta[0], 1.0); // Stationarity
    
    // Check information criteria
    EXPECT_LT(params.aic, 0.0); // Typically negative for returns
    EXPECT_LT(params.bic, 0.0);
    EXPECT_GT(params.bic, params.aic); // BIC penalizes more
}

/**
 * @brief Test GARCH volatility forecasting
 */
TEST_F(AdvancedRiskModelsTest, GARCHVolatilityForecasting) {
    GARCHModel garch(GARCHType::GARCH, 1, 1);
    
    auto fit_result = garch.fit(sample_returns_);
    ASSERT_TRUE(fit_result.is_ok());
    
    auto forecast_result = garch.forecast_volatility(5);
    ASSERT_TRUE(forecast_result.is_ok());
    
    const auto& forecasts = forecast_result.value();
    EXPECT_EQ(forecasts.size(), 5);
    
    // All forecasts should be positive
    for (double vol : forecasts) {
        EXPECT_GT(vol, 0.0);
        EXPECT_LT(vol, 1.0); // Reasonable volatility bounds
    }
}

/**
 * @brief Test GARCH residuals and conditional volatility
 */
TEST_F(AdvancedRiskModelsTest, GARCHResidualsAndVolatility) {
    GARCHModel garch(GARCHType::GARCH, 1, 1);
    
    auto fit_result = garch.fit(sample_returns_);
    ASSERT_TRUE(fit_result.is_ok());
    
    auto residuals_result = garch.get_residuals();
    ASSERT_TRUE(residuals_result.is_ok());
    
    auto volatility_result = garch.get_conditional_volatility();
    ASSERT_TRUE(volatility_result.is_ok());
    
    const auto& residuals = residuals_result.value();
    const auto& volatility = volatility_result.value();
    
    EXPECT_EQ(residuals.size(), sample_returns_.size());
    EXPECT_EQ(volatility.size(), sample_returns_.size());
    
    // Check residuals have approximately unit variance
    double residual_var = 0.0;
    for (double res : residuals) {
        residual_var += res * res;
    }
    residual_var /= residuals.size();
    EXPECT_NEAR(residual_var, 1.0, 0.2); // Allow some deviation
    
    // All volatilities should be positive
    for (double vol : volatility) {
        EXPECT_GT(vol, 0.0);
    }
}

/**
 * @brief Test different GARCH model types
 */
TEST_F(AdvancedRiskModelsTest, DifferentGARCHTypes) {
    std::vector<GARCHType> models = {
        GARCHType::GARCH,
        GARCHType::EGARCH,
        GARCHType::GJR_GARCH
    };
    
    for (auto model_type : models) {
        GARCHModel garch(model_type, 1, 1);
        auto fit_result = garch.fit(sample_returns_);
        
        // All models should fit successfully
        EXPECT_TRUE(fit_result.is_ok()) << "Model type: " << static_cast<int>(model_type);
        
        if (fit_result.is_ok()) {
            const auto& params = fit_result.value();
            EXPECT_GT(params.omega, 0.0);
            EXPECT_LT(params.aic, 0.0);
        }
    }
}

/**
 * @brief Test VaR calculation with historical simulation
 */
TEST_F(AdvancedRiskModelsTest, VaRHistoricalSimulation) {
    VaRCalculator var_calc;
    
    std::vector<double> confidence_levels = {0.01, 0.05, 0.10};
    
    for (double cl : confidence_levels) {
        auto var_result = var_calc.calculate_var(sample_returns_, cl, VaRMethod::HistoricalSimulation);
        ASSERT_TRUE(var_result.is_ok()) << "Confidence level: " << cl;
        
        const auto& result = var_result.value();
        
        EXPECT_GT(result.var_estimate, 0.0);
        EXPECT_LT(result.var_estimate, 1.0);
        EXPECT_EQ(result.confidence_level, cl);
        EXPECT_EQ(result.method, VaRMethod::HistoricalSimulation);
        
        // Expected Shortfall should be higher than VaR
        EXPECT_GE(result.expected_shortfall, result.var_estimate);
        
        // Coverage should be approximately correct
        EXPECT_GT(result.coverage_probability, 1.0 - cl - 0.05);
        EXPECT_LT(result.coverage_probability, 1.0 - cl + 0.05);
    }
}

/**
 * @brief Test parametric VaR calculation
 */
TEST_F(AdvancedRiskModelsTest, VaRParametric) {
    VaRCalculator var_calc;
    
    auto var_result = var_calc.calculate_var(sample_returns_, 0.05, VaRMethod::Parametric);
    ASSERT_TRUE(var_result.is_ok());
    
    const auto& result = var_result.value();
    
    EXPECT_GT(result.var_estimate, 0.0);
    EXPECT_EQ(result.method, VaRMethod::Parametric);
    
    // For normal distribution, 5% VaR should be approximately 1.645 * std dev
    auto return_values = sample_returns_.values();
    double mean = std::accumulate(return_values.begin(), return_values.end(), 0.0) / return_values.size();
    double variance = 0.0;
    for (double ret : return_values) {
        variance += (ret - mean) * (ret - mean);
    }
    variance /= (return_values.size() - 1);
    double std_dev = std::sqrt(variance);
    
    double expected_var = 1.645 * std_dev - mean; // Approximate 5% VaR
    EXPECT_NEAR(result.var_estimate, expected_var, expected_var * 0.3); // 30% tolerance
}

/**
 * @brief Test Monte Carlo VaR calculation
 */
TEST_F(AdvancedRiskModelsTest, VaRMonteCarlo) {
    VaRCalculator var_calc;
    
    auto var_result = var_calc.calculate_var(sample_returns_, 0.05, VaRMethod::MonteCarlo);
    ASSERT_TRUE(var_result.is_ok());
    
    const auto& result = var_result.value();
    
    EXPECT_GT(result.var_estimate, 0.0);
    EXPECT_EQ(result.method, VaRMethod::MonteCarlo);
    
    // Should be reasonably close to parametric VaR
    auto parametric_result = var_calc.calculate_var(sample_returns_, 0.05, VaRMethod::Parametric);
    if (parametric_result.is_ok()) {
        double parametric_var = parametric_result.value().var_estimate;
        EXPECT_NEAR(result.var_estimate, parametric_var, parametric_var * 0.5); // 50% tolerance for MC
    }
}

/**
 * @brief Test Expected Shortfall calculation
 */
TEST_F(AdvancedRiskModelsTest, ExpectedShortfall) {
    VaRCalculator var_calc;
    
    auto es_result = var_calc.calculate_expected_shortfall(sample_returns_, 0.05);
    ASSERT_TRUE(es_result.is_ok());
    
    double expected_shortfall = es_result.value();
    EXPECT_GT(expected_shortfall, 0.0);
    
    // ES should be higher than VaR
    auto var_result = var_calc.calculate_var(sample_returns_, 0.05);
    if (var_result.is_ok()) {
        EXPECT_GE(expected_shortfall, var_result.value().var_estimate);
    }
}

/**
 * @brief Test rolling VaR calculation
 */
TEST_F(AdvancedRiskModelsTest, RollingVaR) {
    VaRCalculator var_calc;
    
    auto rolling_var_result = var_calc.calculate_rolling_var(sample_returns_, 0.05, 100);
    ASSERT_TRUE(rolling_var_result.is_ok());
    
    const auto& rolling_var = rolling_var_result.value();
    
    // Should have fewer observations than original (due to window)
    EXPECT_LT(rolling_var.size(), sample_returns_.size());
    EXPECT_GT(rolling_var.size(), 0);
    
    // All VaR estimates should be positive
    auto var_values = rolling_var.values();
    for (double var : var_values) {
        EXPECT_GT(var, 0.0);
        EXPECT_LT(var, 1.0);
    }
}

/**
 * @brief Test VaR backtesting - Kupiec test
 */
TEST_F(AdvancedRiskModelsTest, VaRBacktestingKupiec) {
    VaRCalculator var_calc;
    VaRBacktester backtester;
    
    // Calculate constant VaR
    auto var_result = var_calc.calculate_var(sample_returns_, 0.05);
    ASSERT_TRUE(var_result.is_ok());
    
    double constant_var = var_result.value().var_estimate;
    
    // Create VaR forecast series
    auto return_values = sample_returns_.values();
    std::vector<double> var_forecasts(return_values.size(), constant_var);
    // Create dates for the forecast series
    std::vector<DateTime> forecast_dates;
    for (size_t i = 0; i < var_forecasts.size(); ++i) {
        forecast_dates.push_back(DateTime::parse("2020-01-01").value().add_days(i));
    }
    TimeSeries<double> var_forecast_ts(forecast_dates, var_forecasts);
    
    auto kupiec_result = backtester.kupiec_test(sample_returns_, var_forecast_ts, 0.05);
    ASSERT_TRUE(kupiec_result.is_ok());
    
    const auto& test = kupiec_result.value();
    
    EXPECT_EQ(test.test_type, BacktestType::Kupiec);
    EXPECT_GE(test.test_statistic, 0.0);
    EXPECT_GE(test.p_value, 0.0);
    EXPECT_LE(test.p_value, 1.0);
    EXPECT_GT(test.critical_value, 0.0);
    EXPECT_EQ(test.total_observations, sample_returns_.size());
    
    // Violation rate should be reasonable
    EXPECT_GE(test.violation_rate, 0.0);
    EXPECT_LE(test.violation_rate, 1.0);
}

/**
 * @brief Test comprehensive VaR backtesting
 */
TEST_F(AdvancedRiskModelsTest, ComprehensiveBacktesting) {
    VaRCalculator var_calc;
    VaRBacktester backtester;
    
    auto var_result = var_calc.calculate_var(sample_returns_, 0.05);
    ASSERT_TRUE(var_result.is_ok());
    
    double constant_var = var_result.value().var_estimate;
    auto return_values = sample_returns_.values();
    std::vector<double> var_forecasts(return_values.size(), constant_var);
    // Create dates for the forecast series
    std::vector<DateTime> forecast_dates2;
    for (size_t i = 0; i < var_forecasts.size(); ++i) {
        forecast_dates2.push_back(DateTime::parse("2020-01-01").value().add_days(i));
    }
    TimeSeries<double> var_forecast_ts(forecast_dates2, var_forecasts);
    
    auto comprehensive_result = backtester.run_comprehensive_tests(sample_returns_, var_forecast_ts, 0.05);
    ASSERT_TRUE(comprehensive_result.is_ok());
    
    const auto& tests = comprehensive_result.value();
    EXPECT_GT(tests.size(), 0);
    
    // Should include at least Kupiec test
    bool has_kupiec = false;
    for (const auto& test : tests) {
        if (test.test_type == BacktestType::Kupiec) {
            has_kupiec = true;
            EXPECT_GE(test.test_statistic, 0.0);
            EXPECT_GE(test.p_value, 0.0);
            EXPECT_LE(test.p_value, 1.0);
        }
    }
    EXPECT_TRUE(has_kupiec);
}

/**
 * @brief Test Extreme Value Theory fitting
 */
TEST_F(AdvancedRiskModelsTest, ExtremeValueTheory) {
    ExtremeValueTheory evt;
    
    auto evt_result = evt.fit_pot_model(extreme_returns_, 0.95);
    ASSERT_TRUE(evt_result.is_ok());
    
    const auto& params = evt_result.value();
    
    EXPECT_GT(params.threshold, 0.0);
    EXPECT_EQ(params.threshold_quantile, 0.95);
    EXPECT_GT(params.n_exceedances, 0);
    EXPECT_GT(params.sigma, 0.0); // Scale parameter should be positive
    
    // Test extreme quantile calculation
    auto quantile_result = evt.calculate_extreme_quantile(0.001);
    ASSERT_TRUE(quantile_result.is_ok());
    
    double extreme_quantile = quantile_result.value();
    EXPECT_GT(extreme_quantile, params.threshold); // Should exceed threshold
    
    // Test EVT Expected Shortfall
    auto es_result = evt.calculate_evt_expected_shortfall(0.001);
    if (es_result.is_ok()) {
        EXPECT_GT(es_result.value(), extreme_quantile); // ES > VaR
    }
}

/**
 * @brief Test EVT with insufficient exceedances
 */
TEST_F(AdvancedRiskModelsTest, EVTInsufficientExceedances) {
    ExtremeValueTheory evt;
    
    // Use very high threshold that results in few exceedances
    auto evt_result = evt.fit_pot_model(sample_returns_, 0.999);
    
    // Should fail due to insufficient exceedances
    EXPECT_TRUE(evt_result.is_error());
}

/**
 * @brief Test block maxima EVT approach
 */
TEST_F(AdvancedRiskModelsTest, EVTBlockMaxima) {
    ExtremeValueTheory evt;
    
    auto block_result = evt.fit_block_maxima(extreme_returns_, 22);
    ASSERT_TRUE(block_result.is_ok());
    
    const auto& params = block_result.value();
    EXPECT_GT(params.sigma, 0.0); // Scale parameter should be positive
}

/**
 * @brief Test invalid inputs and edge cases
 */
TEST_F(AdvancedRiskModelsTest, InvalidInputs) {
    // Test GARCH with invalid orders
    EXPECT_THROW(GARCHModel(GARCHType::GARCH, -1, 1), std::invalid_argument);
    EXPECT_THROW(GARCHModel(GARCHType::GARCH, 1, -1), std::invalid_argument);
    EXPECT_THROW(GARCHModel(GARCHType::GARCH, 10, 10), std::invalid_argument); // Too high order
    
    // Test VaR with insufficient data
    std::vector<double> short_data = {0.01, -0.02, 0.005};
    std::vector<DateTime> short_dates = {
        DateTime::parse("2020-01-01").value(),
        DateTime::parse("2020-01-02").value(),
        DateTime::parse("2020-01-03").value()
    };
    TimeSeries<double> short_ts(short_dates, short_data);
    
    VaRCalculator var_calc;
    auto var_result = var_calc.calculate_var(short_ts, 0.05);
    EXPECT_TRUE(var_result.is_error());
    
    // Test GARCH with insufficient data
    GARCHModel garch;
    auto fit_result = garch.fit(short_ts);
    EXPECT_TRUE(fit_result.is_error());
}

/**
 * @brief Test confidence level validation
 */
TEST_F(AdvancedRiskModelsTest, ConfidenceLevelValidation) {
    VaRCalculator var_calc;
    
    // Test extreme confidence levels
    std::vector<double> confidence_levels = {0.001, 0.01, 0.05, 0.10, 0.25};
    
    for (double cl : confidence_levels) {
        auto var_result = var_calc.calculate_var(sample_returns_, cl);
        if (var_result.is_ok()) {
            EXPECT_GT(var_result.value().var_estimate, 0.0);
            EXPECT_EQ(var_result.value().confidence_level, cl);
        }
    }
}

/**
 * @brief Test VaR ordering property
 */
TEST_F(AdvancedRiskModelsTest, VaROrderingProperty) {
    VaRCalculator var_calc;
    
    // VaR should increase as confidence level decreases (more extreme)
    auto var_5pct = var_calc.calculate_var(sample_returns_, 0.05);
    auto var_1pct = var_calc.calculate_var(sample_returns_, 0.01);
    
    if (var_5pct.is_ok() && var_1pct.is_ok()) {
        // 1% VaR should be higher than 5% VaR
        EXPECT_GE(var_1pct.value().var_estimate, var_5pct.value().var_estimate);
    }
}

/**
 * @brief Performance test for large datasets
 */
TEST_F(AdvancedRiskModelsTest, LargeDatasetPerformance) {
    // Generate large dataset
    auto large_data = generate_test_returns(5000);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    VaRCalculator var_calc;
    auto var_result = var_calc.calculate_var(large_data, 0.05);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_TRUE(var_result.is_ok());
    EXPECT_LT(duration.count(), 1000); // Should complete within 1 second
}

/**
 * @brief Test numerical stability
 */
TEST_F(AdvancedRiskModelsTest, NumericalStability) {
    // Create data with very small returns
    std::vector<double> tiny_returns(1000, 1e-8);
    for (size_t i = 0; i < tiny_returns.size(); i += 10) {
        tiny_returns[i] = -1e-6; // Some small negative returns
    }
    
    // Create dates for tiny returns
    std::vector<DateTime> tiny_dates;
    for (size_t i = 0; i < tiny_returns.size(); ++i) {
        tiny_dates.push_back(DateTime::parse("2020-01-01").value().add_days(i));
    }
    TimeSeries<double> tiny_ts(tiny_dates, tiny_returns);
    
    VaRCalculator var_calc;
    auto var_result = var_calc.calculate_var(tiny_ts, 0.05);
    
    if (var_result.is_ok()) {
        EXPECT_GT(var_result.value().var_estimate, 0.0);
        EXPECT_LT(var_result.value().var_estimate, 1.0);
    }
}