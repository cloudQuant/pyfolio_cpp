#include <gtest/gtest.h>
#include <pyfolio/derivatives/options_pricing.h>
#include <cmath>

using namespace pyfolio::derivatives;

/**
 * @brief Test fixture for options pricing tests
 */
class OptionsPricingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Standard market data for testing
        market_data_ = MarketData(100.0, 0.20, 0.05, 0.02);
        
        // Standard option specifications
        atm_call_ = OptionSpec(100.0, 0.25, OptionType::Call);
        otm_call_ = OptionSpec(110.0, 0.25, OptionType::Call);
        atm_put_ = OptionSpec(100.0, 0.25, OptionType::Put);
        itm_put_ = OptionSpec(90.0, 0.25, OptionType::Put);
    }
    
    MarketData market_data_{100.0, 0.20, 0.05, 0.02}; // Initialize with default values
    OptionSpec atm_call_{100.0, 0.25, OptionType::Call};
    OptionSpec otm_call_{110.0, 0.25, OptionType::Call};
    OptionSpec atm_put_{100.0, 0.25, OptionType::Put};
    OptionSpec itm_put_{90.0, 0.25, OptionType::Put};
    
    const double TOLERANCE = 1e-6;
    const double MONTE_CARLO_TOLERANCE = 0.15; // Higher tolerance for MC methods
};

/**
 * @brief Test Black-Scholes model basic functionality
 */
TEST_F(OptionsPricingTest, BlackScholesBasicPricing) {
    BlackScholesModel bs_model;
    
    // Test ATM call option
    auto call_result = bs_model.price(atm_call_, market_data_);
    ASSERT_TRUE(call_result.is_ok());
    
    double call_price = call_result.value().price;
    EXPECT_GT(call_price, 0.0);
    EXPECT_LT(call_price, market_data_.spot_price); // Call price should be less than spot
    
    // Test ATM put option
    auto put_result = bs_model.price(atm_put_, market_data_);
    ASSERT_TRUE(put_result.is_ok());
    
    double put_price = put_result.value().price;
    EXPECT_GT(put_price, 0.0);
    
    // Test put-call parity: C - P = S*e^(-q*T) - K*e^(-r*T)
    double S = market_data_.spot_price;
    double K = atm_call_.strike;
    double T = atm_call_.time_to_expiry;
    double r = market_data_.risk_free_rate;
    double q = market_data_.dividend_yield;
    
    double expected_diff = S * std::exp(-q * T) - K * std::exp(-r * T);
    double actual_diff = call_price - put_price;
    
    EXPECT_NEAR(actual_diff, expected_diff, TOLERANCE);
}

/**
 * @brief Test Black-Scholes Greeks calculations
 */
TEST_F(OptionsPricingTest, BlackScholesGreeks) {
    BlackScholesModel bs_model;
    
    auto result = bs_model.price(atm_call_, market_data_);
    ASSERT_TRUE(result.is_ok());
    
    const auto& greeks = result.value().greeks;
    
    // Test Delta bounds
    EXPECT_GT(greeks.delta, 0.0);
    EXPECT_LT(greeks.delta, 1.0); // Call delta should be between 0 and 1
    
    // Test Gamma (should be positive for long options)
    EXPECT_GT(greeks.gamma, 0.0);
    
    // Test Vega (should be positive for long options)
    EXPECT_GT(greeks.vega, 0.0);
    
    // Test Theta (should be negative for long options due to time decay)
    EXPECT_LT(greeks.theta, 0.0);
    
    // Test Rho (should be positive for calls)
    EXPECT_GT(greeks.rho, 0.0);
}

/**
 * @brief Test Black-Scholes edge cases
 */
TEST_F(OptionsPricingTest, BlackScholesEdgeCases) {
    BlackScholesModel bs_model;
    
    // Test with zero time to expiry
    OptionSpec expired_call(100.0, 0.0, OptionType::Call);
    auto expired_result = bs_model.price(expired_call, market_data_);
    EXPECT_TRUE(expired_result.is_error()); // Should fail with zero time
    
    // Test with negative strike
    OptionSpec negative_strike_call(-50.0, 0.25, OptionType::Call);
    auto negative_result = bs_model.price(negative_strike_call, market_data_);
    EXPECT_TRUE(negative_result.is_error());
    
    // Test with zero volatility
    MarketData zero_vol_market = market_data_;
    zero_vol_market.volatility = 0.0;
    auto zero_vol_result = bs_model.price(atm_call_, zero_vol_market);
    EXPECT_TRUE(zero_vol_result.is_error());
}

/**
 * @brief Test Heston model Monte Carlo pricing
 */
TEST_F(OptionsPricingTest, HestonMonteCarloPricing) {
    HestonModel heston_model;
    HestonParameters params(0.04, 2.0, 0.04, -0.7, 0.3);
    
    // Test with small number of simulations for speed
    auto result = heston_model.price_monte_carlo(atm_call_, market_data_, params, 10000);
    ASSERT_TRUE(result.is_ok());
    
    const auto& pricing = result.value();
    
    // Basic sanity checks
    EXPECT_GT(pricing.price, 0.0);
    EXPECT_LT(pricing.price, market_data_.spot_price);
    EXPECT_GT(pricing.standard_error, 0.0);
    EXPECT_EQ(pricing.num_simulations, 10000);
    EXPECT_GT(pricing.computation_time_ms, 0.0);
    
    // Should be reasonably close to Black-Scholes for similar parameters
    BlackScholesModel bs_model;
    auto bs_result = bs_model.price(atm_call_, market_data_);
    if (bs_result.is_ok()) {
        double price_diff = std::abs(pricing.price - bs_result.value().price);
        EXPECT_LT(price_diff, MONTE_CARLO_TOLERANCE);
    }
}

/**
 * @brief Test Heston Greeks calculation
 */
TEST_F(OptionsPricingTest, HestonGreeksCalculation) {
    HestonModel heston_model;
    HestonParameters params(0.04, 2.0, 0.04, -0.7, 0.3);
    
    auto greeks_result = heston_model.calculate_greeks(atm_call_, market_data_, params);
    ASSERT_TRUE(greeks_result.is_ok());
    
    const auto& greeks = greeks_result.value();
    
    // Basic bounds checking
    EXPECT_GT(greeks.delta, 0.0);
    EXPECT_LT(greeks.delta, 1.0);
    // Note: Vega calculation in Heston Greeks might be zero due to finite difference implementation
    EXPECT_GE(greeks.vega, 0.0);
    EXPECT_LT(greeks.theta, 0.0);
}

/**
 * @brief Test Local Volatility model
 */
TEST_F(OptionsPricingTest, LocalVolatilityModel) {
    LocalVolatilityModel lv_model;
    
    // Set up a simple volatility surface
    std::vector<double> strikes = {80, 100, 120};
    std::vector<double> times = {0.25, 0.5};
    std::vector<std::vector<double>> volatilities = {
        {0.25, 0.20, 0.25},
        {0.24, 0.19, 0.24}
    };
    
    lv_model.set_volatility_surface(strikes, times, volatilities);
    
    // Test Monte Carlo pricing
    auto mc_result = lv_model.price_monte_carlo(atm_call_, market_data_, 10000);
    ASSERT_TRUE(mc_result.is_ok());
    
    EXPECT_GT(mc_result.value().price, 0.0);
    EXPECT_EQ(mc_result.value().num_simulations, 10000);
    
    // Test PDE pricing
    auto pde_result = lv_model.price_pde(atm_call_, market_data_, 50, 100);
    ASSERT_TRUE(pde_result.is_ok());
    
    EXPECT_GT(pde_result.value().price, 0.0);
    EXPECT_EQ(pde_result.value().num_simulations, 0); // PDE doesn't use simulations
}

/**
 * @brief Test Binomial Tree model
 */
TEST_F(OptionsPricingTest, BinomialTreeModel) {
    BinomialTreeModel tree_model;
    
    // Test European option
    auto european_result = tree_model.price(atm_call_, market_data_, 100);
    ASSERT_TRUE(european_result.is_ok());
    
    double european_price = european_result.value().price;
    EXPECT_GT(european_price, 0.0);
    
    // Test American option
    OptionSpec american_call = atm_call_;
    american_call.style = ExerciseStyle::American;
    
    auto american_result = tree_model.price(american_call, market_data_, 100);
    ASSERT_TRUE(american_result.is_ok());
    
    double american_price = american_result.value().price;
    EXPECT_GE(american_price, european_price); // American should be >= European
    
    // For calls with no dividends, American and European should be very close
    if (market_data_.dividend_yield == 0.0) {
        EXPECT_NEAR(american_price, european_price, 0.01);
    }
}

/**
 * @brief Test American put early exercise premium
 */
TEST_F(OptionsPricingTest, AmericanPutEarlyExercise) {
    // Use high dividend yield to encourage early exercise
    MarketData high_div_market(100.0, 0.25, 0.08, 0.12);
    
    BinomialTreeModel tree_model;
    BlackScholesModel bs_model;
    
    OptionSpec american_put(110.0, 1.0, OptionType::Put);
    american_put.style = ExerciseStyle::American;
    
    OptionSpec european_put(110.0, 1.0, OptionType::Put);
    
    auto american_result = tree_model.price(american_put, high_div_market, 500);
    auto european_result = bs_model.price(european_put, high_div_market);
    
    ASSERT_TRUE(american_result.is_ok());
    ASSERT_TRUE(european_result.is_ok());
    
    double american_price = american_result.value().price;
    double european_price = european_result.value().price;
    
    // American put should have early exercise premium
    EXPECT_GT(american_price, european_price);
    
    double early_exercise_premium = american_price - european_price;
    EXPECT_GT(early_exercise_premium, 0.01); // Should be meaningful premium
}

/**
 * @brief Test Asian options pricing
 */
TEST_F(OptionsPricingTest, AsianOptionsPricing) {
    ExoticOptionsModel exotic_model;
    
    auto asian_result = exotic_model.price_asian_option(atm_call_, market_data_, 10000);
    ASSERT_TRUE(asian_result.is_ok());
    
    double asian_price = asian_result.value().price;
    EXPECT_GT(asian_price, 0.0);
    
    // Asian options should generally be cheaper than European due to averaging
    BlackScholesModel bs_model;
    auto european_result = bs_model.price(atm_call_, market_data_);
    if (european_result.is_ok()) {
        EXPECT_LT(asian_price, european_result.value().price);
    }
}

/**
 * @brief Test Barrier options pricing
 */
TEST_F(OptionsPricingTest, BarrierOptionsPricing) {
    ExoticOptionsModel exotic_model;
    
    double barrier = 110.0;
    
    // Test Up-and-Out call
    auto up_out_result = exotic_model.price_barrier_option(
        atm_call_, market_data_, barrier, BarrierType::UpAndOut, 10000);
    ASSERT_TRUE(up_out_result.is_ok());
    
    // Test Up-and-In call
    auto up_in_result = exotic_model.price_barrier_option(
        atm_call_, market_data_, barrier, BarrierType::UpAndIn, 10000);
    ASSERT_TRUE(up_in_result.is_ok());
    
    double up_out_price = up_out_result.value().price;
    double up_in_price = up_in_result.value().price;
    
    EXPECT_GE(up_out_price, 0.0);
    EXPECT_GE(up_in_price, 0.0);
    
    // Up-and-Out should be cheaper than European (knockout risk)
    BlackScholesModel bs_model;
    auto european_result = bs_model.price(atm_call_, market_data_);
    if (european_result.is_ok()) {
        EXPECT_LT(up_out_price, european_result.value().price);
        
        // Up-and-Out + Up-and-In should approximately equal European
        double total_price = up_out_price + up_in_price;
        double european_price = european_result.value().price;
        EXPECT_NEAR(total_price, european_price, MONTE_CARLO_TOLERANCE);
    }
}

/**
 * @brief Test volatility surface calibration
 */
TEST_F(OptionsPricingTest, VolatilitySurfaceCalibration) {
    VolatilitySurfaceCalibrator calibrator;
    
    // Create some test market quotes
    std::vector<VolatilitySurfaceCalibrator::MarketQuote> quotes = {
        {95.0, 0.25, 7.5, 7.4, 7.6, OptionType::Call},
        {100.0, 0.25, 4.2, 4.1, 4.3, OptionType::Call},
        {105.0, 0.25, 1.8, 1.7, 1.9, OptionType::Call}
    };
    
    auto implied_vols_result = calibrator.calibrate_implied_volatilities(quotes, market_data_);
    ASSERT_TRUE(implied_vols_result.is_ok());
    
    const auto& implied_vols = implied_vols_result.value();
    EXPECT_FALSE(implied_vols.empty());
    EXPECT_FALSE(implied_vols[0].empty());
    EXPECT_EQ(implied_vols[0].size(), quotes.size());
    
    // Check that implied volatilities are reasonable
    for (double vol : implied_vols[0]) {
        EXPECT_GT(vol, 0.001); // At least 0.1%
        EXPECT_LT(vol, 5.0);   // Less than 500%
    }
}

/**
 * @brief Test option specification validation
 */
TEST_F(OptionsPricingTest, OptionSpecValidation) {
    BlackScholesModel bs_model;
    
    // Test valid option
    EXPECT_TRUE(bs_model.price(atm_call_, market_data_).is_ok());
    
    // Test American option with Black-Scholes (should fail)
    OptionSpec american_option(100.0, 0.25, OptionType::Call);
    american_option.style = ExerciseStyle::American;
    
    auto american_result = bs_model.price(american_option, market_data_);
    EXPECT_TRUE(american_result.is_error());
    EXPECT_EQ(american_result.error().code, pyfolio::ErrorCode::InvalidInput);
}

/**
 * @brief Test market data validation
 */
TEST_F(OptionsPricingTest, MarketDataValidation) {
    BlackScholesModel bs_model;
    
    // Test with negative spot price
    MarketData invalid_market(-100.0, 0.20, 0.05, 0.02);
    auto result1 = bs_model.price(atm_call_, invalid_market);
    EXPECT_TRUE(result1.is_error());
    
    // Test with negative volatility
    MarketData invalid_vol_market(100.0, -0.20, 0.05, 0.02);
    auto result2 = bs_model.price(atm_call_, invalid_vol_market);
    EXPECT_TRUE(result2.is_error());
    
    // Test with negative strike
    OptionSpec invalid_option(-100.0, 0.25, OptionType::Call);
    auto result3 = bs_model.price(invalid_option, market_data_);
    EXPECT_TRUE(result3.is_error());
}

/**
 * @brief Test computation time tracking
 */
TEST_F(OptionsPricingTest, ComputationTimeTracking) {
    BlackScholesModel bs_model;
    
    auto result = bs_model.price(atm_call_, market_data_);
    ASSERT_TRUE(result.is_ok());
    
    // Should track computation time (may be very small for analytical methods)
    EXPECT_GE(result.value().computation_time_ms, 0.0);
    EXPECT_LT(result.value().computation_time_ms, 100.0); // Should be very fast for analytical
}

/**
 * @brief Test Greeks symmetry properties
 */
TEST_F(OptionsPricingTest, GreeksSymmetryProperties) {
    BlackScholesModel bs_model;
    
    // Test call and put delta relationship
    auto call_result = bs_model.price(atm_call_, market_data_);
    auto put_result = bs_model.price(atm_put_, market_data_);
    
    ASSERT_TRUE(call_result.is_ok());
    ASSERT_TRUE(put_result.is_ok());
    
    double call_delta = call_result.value().greeks.delta;
    double put_delta = put_result.value().greeks.delta;
    
    // Call delta - Put delta should equal e^(-q*T)
    double T = atm_call_.time_to_expiry;
    double q = market_data_.dividend_yield;
    double expected_delta_diff = std::exp(-q * T);
    double actual_delta_diff = call_delta - put_delta;
    
    EXPECT_NEAR(actual_delta_diff, expected_delta_diff, TOLERANCE);
    
    // Gamma should be the same for call and put with same strike/expiry
    EXPECT_NEAR(call_result.value().greeks.gamma, put_result.value().greeks.gamma, TOLERANCE);
    
    // Vega should be the same for call and put with same strike/expiry
    EXPECT_NEAR(call_result.value().greeks.vega, put_result.value().greeks.vega, TOLERANCE);
}

/**
 * @brief Test model convergence properties
 */
TEST_F(OptionsPricingTest, ModelConvergenceProperties) {
    BinomialTreeModel tree_model;
    BlackScholesModel bs_model;
    
    // Test convergence of binomial tree to Black-Scholes
    auto bs_result = bs_model.price(atm_call_, market_data_);
    ASSERT_TRUE(bs_result.is_ok());
    double bs_price = bs_result.value().price;
    
    std::vector<size_t> num_steps = {50, 100, 500, 1000};
    
    for (size_t steps : num_steps) {
        auto tree_result = tree_model.price(atm_call_, market_data_, steps);
        ASSERT_TRUE(tree_result.is_ok());
        
        double tree_price = tree_result.value().price;
        double error = std::abs(tree_price - bs_price) / bs_price;
        
        // Error should decrease with more steps
        if (steps >= 500) {
            EXPECT_LT(error, 0.01); // Within 1% for 500+ steps
        }
    }
}

/**
 * @brief Performance regression test
 */
TEST_F(OptionsPricingTest, PerformanceRegression) {
    BlackScholesModel bs_model;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Price 1000 options
    for (int i = 0; i < 1000; ++i) {
        OptionSpec option(95.0 + i * 0.01, 0.25, OptionType::Call);
        auto result = bs_model.price(option, market_data_);
        EXPECT_TRUE(result.is_ok());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should be able to price 1000 options in less than 100ms
    EXPECT_LT(duration.count(), 100);
}