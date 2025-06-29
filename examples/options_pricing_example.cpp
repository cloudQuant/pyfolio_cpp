#include <pyfolio/derivatives/options_pricing.h>
#include <iostream>
#include <iomanip>

using namespace pyfolio::derivatives;

/**
 * @brief Demonstrate Black-Scholes option pricing
 */
void demonstrate_black_scholes_pricing() {
    std::cout << "=== Black-Scholes Option Pricing ===\n\n";
    
    // Market data: S=100, vol=20%, r=5%, q=2%
    MarketData market(100.0, 0.20, 0.05, 0.02);
    
    // Option specifications
    std::vector<OptionSpec> options = {
        OptionSpec(105.0, 0.25, OptionType::Call),   // 3-month ATM call
        OptionSpec(105.0, 0.25, OptionType::Put),    // 3-month ATM put
        OptionSpec(110.0, 0.25, OptionType::Call),   // 3-month OTM call
        OptionSpec(95.0, 0.25, OptionType::Put),     // 3-month OTM put
        OptionSpec(105.0, 1.0, OptionType::Call),    // 1-year ATM call
    };
    
    BlackScholesModel bs_model;
    
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Strike  Type  Time   Price    Delta   Gamma    Theta    Vega     Rho\n";
    std::cout << "--------------------------------------------------------------------\n";
    
    for (const auto& option : options) {
        auto result = bs_model.price(option, market);
        if (result.is_ok()) {
            const auto& pricing = result.value();
            
            std::cout << std::setw(6) << option.strike
                     << std::setw(6) << (option.type == OptionType::Call ? "Call" : "Put")
                     << std::setw(6) << option.time_to_expiry
                     << std::setw(9) << pricing.price
                     << std::setw(8) << pricing.greeks.delta
                     << std::setw(8) << pricing.greeks.gamma
                     << std::setw(9) << pricing.greeks.theta
                     << std::setw(8) << pricing.greeks.vega
                     << std::setw(8) << pricing.greeks.rho
                     << std::endl;
        }
    }
    
    std::cout << "\n";
}

/**
 * @brief Demonstrate Heston stochastic volatility model
 */
void demonstrate_heston_model() {
    std::cout << "=== Heston Stochastic Volatility Model ===\n\n";
    
    // Market data
    MarketData market(100.0, 0.20, 0.05, 0.0);
    
    // Heston parameters (typical calibrated values)
    HestonParameters heston_params(
        0.04,   // v0: initial variance (20% vol)
        2.0,    // kappa: mean reversion speed
        0.04,   // theta: long-term variance
        -0.7,   // rho: correlation (negative for equity)
        0.3     // sigma_v: vol of vol
    );
    
    // Option to price
    OptionSpec call_option(105.0, 0.25, OptionType::Call);
    
    HestonModel heston_model;
    
    std::cout << "Pricing 3-month call option (K=105, S=100)\n";
    std::cout << "Heston Parameters:\n";
    std::cout << "  Initial variance (v0): " << heston_params.v0 << "\n";
    std::cout << "  Mean reversion (kappa): " << heston_params.kappa << "\n";
    std::cout << "  Long-term variance (theta): " << heston_params.theta << "\n";
    std::cout << "  Correlation (rho): " << heston_params.rho << "\n";
    std::cout << "  Vol of vol (sigma_v): " << heston_params.sigma_v << "\n\n";
    
    // Compare different simulation sizes
    std::vector<size_t> sim_sizes = {10000, 50000, 100000, 250000};
    
    std::cout << "Simulations    Price    Std Error   Comp Time (ms)\n";
    std::cout << "------------------------------------------------\n";
    
    for (size_t num_sims : sim_sizes) {
        auto result = heston_model.price_monte_carlo(call_option, market, heston_params, num_sims);
        if (result.is_ok()) {
            const auto& pricing = result.value();
            std::cout << std::setw(10) << num_sims
                     << std::setw(10) << std::fixed << std::setprecision(4) << pricing.price
                     << std::setw(12) << std::setprecision(5) << pricing.standard_error
                     << std::setw(12) << std::fixed << std::setprecision(1) << pricing.computation_time_ms
                     << std::endl;
        }
    }
    
    // Calculate Greeks using finite differences
    std::cout << "\nGreeks calculation (50k simulations):\n";
    auto greeks_result = heston_model.calculate_greeks(call_option, market, heston_params);
    if (greeks_result.is_ok()) {
        const auto& greeks = greeks_result.value();
        std::cout << "  Delta: " << std::setprecision(4) << greeks.delta << "\n";
        std::cout << "  Vega:  " << std::setprecision(4) << greeks.vega << "\n";
        std::cout << "  Theta: " << std::setprecision(4) << greeks.theta << "\n";
    }
    
    std::cout << "\n";
}

/**
 * @brief Demonstrate Local Volatility model
 */
void demonstrate_local_volatility_model() {
    std::cout << "=== Local Volatility Model ===\n\n";
    
    // Market data
    MarketData market(100.0, 0.20, 0.05, 0.0);
    
    // Create a simple volatility surface (vol smile)
    std::vector<double> strikes = {80, 90, 100, 110, 120};
    std::vector<double> times = {0.25, 0.5, 1.0};
    std::vector<std::vector<double>> volatilities = {
        {0.25, 0.22, 0.20, 0.22, 0.25},  // 3-month
        {0.24, 0.21, 0.19, 0.21, 0.24},  // 6-month  
        {0.23, 0.20, 0.18, 0.20, 0.23}   // 1-year
    };
    
    LocalVolatilityModel lv_model;
    lv_model.set_volatility_surface(strikes, times, volatilities);
    
    // Price options across different strikes
    std::vector<double> test_strikes = {90, 95, 100, 105, 110};
    OptionSpec base_option(100.0, 0.25, OptionType::Call);
    
    std::cout << "3-month Call Options (Local Vol vs Constant Vol):\n";
    std::cout << "Strike   Local Vol Price   Constant Vol Price   Difference\n";
    std::cout << "--------------------------------------------------------\n";
    
    BlackScholesModel bs_model;
    
    for (double K : test_strikes) {
        OptionSpec option(K, 0.25, OptionType::Call);
        
        // Local volatility price
        auto lv_result = lv_model.price_monte_carlo(option, market, 50000);
        
        // Black-Scholes price with constant volatility
        auto bs_result = bs_model.price(option, market);
        
        if (lv_result.is_ok() && bs_result.is_ok()) {
            double lv_price = lv_result.value().price;
            double bs_price = bs_result.value().price;
            double diff = lv_price - bs_price;
            
            std::cout << std::setw(6) << std::fixed << std::setprecision(0) << K
                     << std::setw(16) << std::setprecision(4) << lv_price
                     << std::setw(19) << bs_price
                     << std::setw(12) << std::showpos << diff << std::noshowpos
                     << std::endl;
        }
    }
    
    // Demonstrate PDE pricing vs Monte Carlo
    std::cout << "\nPDE vs Monte Carlo pricing comparison:\n";
    OptionSpec atm_option(100.0, 0.25, OptionType::Call);
    
    auto mc_result = lv_model.price_monte_carlo(atm_option, market, 100000);
    auto pde_result = lv_model.price_pde(atm_option, market, 200, 1000);
    
    if (mc_result.is_ok() && pde_result.is_ok()) {
        std::cout << "Monte Carlo (100k sims): " << std::setprecision(4) << mc_result.value().price 
                  << " (time: " << mc_result.value().computation_time_ms << "ms)\n";
        std::cout << "PDE Solver (200x1000):   " << pde_result.value().price 
                  << " (time: " << pde_result.value().computation_time_ms << "ms)\n";
    }
    
    std::cout << "\n";
}

/**
 * @brief Demonstrate American options with binomial trees
 */
void demonstrate_american_options() {
    std::cout << "=== American Options Pricing ===\n\n";
    
    // Market data favoring early exercise (high dividend yield)
    MarketData market(100.0, 0.25, 0.08, 0.06);
    
    BinomialTreeModel tree_model;
    BlackScholesModel bs_model;
    
    std::vector<double> strikes = {95, 100, 105, 110};
    std::vector<double> times = {0.25, 0.5, 1.0};
    
    std::cout << "American vs European Put Options (S=100, r=8%, q=6%, vol=25%):\n";
    std::cout << "Strike  Time   American   European   Early Ex Premium\n";
    std::cout << "---------------------------------------------------\n";
    
    for (double T : times) {
        for (double K : strikes) {
            OptionSpec american_put(K, T, OptionType::Put);
            american_put.style = ExerciseStyle::American;
            
            OptionSpec european_put(K, T, OptionType::Put);
            european_put.style = ExerciseStyle::European;
            
            auto american_result = tree_model.price(american_put, market, 1000);
            auto european_result = bs_model.price(european_put, market);
            
            if (american_result.is_ok() && european_result.is_ok()) {
                double american_price = american_result.value().price;
                double european_price = european_result.value().price;
                double early_exercise_premium = american_price - european_price;
                
                std::cout << std::setw(6) << std::fixed << std::setprecision(0) << K
                         << std::setw(7) << std::setprecision(2) << T
                         << std::setw(11) << std::setprecision(4) << american_price
                         << std::setw(11) << european_price
                         << std::setw(12) << early_exercise_premium
                         << std::endl;
            }
        }
    }
    
    std::cout << "\n";
}

/**
 * @brief Demonstrate exotic options pricing
 */
void demonstrate_exotic_options() {
    std::cout << "=== Exotic Options Pricing ===\n\n";
    
    MarketData market(100.0, 0.20, 0.05, 0.02);
    ExoticOptionsModel exotic_model;
    BlackScholesModel bs_model;
    
    // Asian options
    std::cout << "Asian vs European Options:\n";
    std::cout << "Type      Strike   Asian Price   European Price   Difference\n";
    std::cout << "--------------------------------------------------------\n";
    
    std::vector<std::pair<double, OptionType>> asian_specs = {
        {100.0, OptionType::Call},
        {100.0, OptionType::Put},
        {105.0, OptionType::Call},
        {95.0, OptionType::Put}
    };
    
    for (const auto& [strike, type] : asian_specs) {
        OptionSpec asian_option(strike, 0.25, type);
        OptionSpec european_option(strike, 0.25, type);
        
        auto asian_result = exotic_model.price_asian_option(asian_option, market, 50000);
        auto european_result = bs_model.price(european_option, market);
        
        if (asian_result.is_ok() && european_result.is_ok()) {
            double asian_price = asian_result.value().price;
            double european_price = european_result.value().price;
            double diff = asian_price - european_price;
            
            std::cout << std::setw(8) << (type == OptionType::Call ? "Call" : "Put")
                     << std::setw(9) << std::fixed << std::setprecision(0) << strike
                     << std::setw(13) << std::setprecision(4) << asian_price
                     << std::setw(16) << european_price
                     << std::setw(12) << std::showpos << diff << std::noshowpos
                     << std::endl;
        }
    }
    
    // Barrier options
    std::cout << "\nBarrier Options (Barrier = 110):\n";
    std::cout << "Type         Price    vs European   Knock-out Probability\n";
    std::cout << "-------------------------------------------------------\n";
    
    OptionSpec barrier_option(105.0, 0.25, OptionType::Call);
    double barrier_level = 110.0;
    
    auto up_out_result = exotic_model.price_barrier_option(barrier_option, market, barrier_level, 
                                                          BarrierType::UpAndOut, 50000);
    auto up_in_result = exotic_model.price_barrier_option(barrier_option, market, barrier_level, 
                                                         BarrierType::UpAndIn, 50000);
    auto european_result = bs_model.price(barrier_option, market);
    
    if (up_out_result.is_ok() && up_in_result.is_ok() && european_result.is_ok()) {
        double up_out_price = up_out_result.value().price;
        double up_in_price = up_in_result.value().price;
        double european_price = european_result.value().price;
        
        // Up-and-out + Up-and-in should approximately equal European
        double total_barrier = up_out_price + up_in_price;
        double knockout_prob = 1.0 - (up_out_price / european_price);
        
        std::cout << std::setw(12) << "Up-and-Out"
                 << std::setw(9) << std::fixed << std::setprecision(4) << up_out_price
                 << std::setw(12) << std::setprecision(4) << (up_out_price - european_price)
                 << std::setw(16) << std::setprecision(2) << (knockout_prob * 100) << "%"
                 << std::endl;
                 
        std::cout << std::setw(12) << "Up-and-In"
                 << std::setw(9) << up_in_price
                 << std::setw(12) << (up_in_price - european_price)
                 << std::endl;
                 
        std::cout << std::setw(12) << "European"
                 << std::setw(9) << european_price
                 << std::setw(12) << "0.0000"
                 << std::endl;
                 
        std::cout << std::setw(12) << "Sum"
                 << std::setw(9) << total_barrier
                 << std::setw(12) << (total_barrier - european_price)
                 << std::endl;
    }
    
    std::cout << "\n";
}

/**
 * @brief Demonstrate volatility surface calibration
 */
void demonstrate_volatility_calibration() {
    std::cout << "=== Volatility Surface Calibration ===\n\n";
    
    // Market data
    MarketData market(100.0, 0.20, 0.05, 0.02);
    
    // Simulated market quotes (would come from market data in practice)
    std::vector<VolatilitySurfaceCalibrator::MarketQuote> market_quotes = {
        {95.0, 0.25, 7.5, 7.4, 7.6, OptionType::Call},
        {100.0, 0.25, 4.2, 4.1, 4.3, OptionType::Call},
        {105.0, 0.25, 1.8, 1.7, 1.9, OptionType::Call},
        {110.0, 0.25, 0.6, 0.5, 0.7, OptionType::Call},
        {95.0, 0.5, 9.2, 9.0, 9.4, OptionType::Call},
        {100.0, 0.5, 6.1, 5.9, 6.3, OptionType::Call},
        {105.0, 0.5, 3.8, 3.6, 4.0, OptionType::Call},
        {110.0, 0.5, 2.1, 1.9, 2.3, OptionType::Call}
    };
    
    VolatilitySurfaceCalibrator calibrator;
    auto implied_vols_result = calibrator.calibrate_implied_volatilities(market_quotes, market);
    
    if (implied_vols_result.is_ok()) {
        const auto& implied_vols = implied_vols_result.value();
        
        std::cout << "Implied Volatility Calibration Results:\n";
        std::cout << "Strike   Time   Market Price   Implied Vol   Model Price\n";
        std::cout << "------------------------------------------------------\n";
        
        BlackScholesModel bs_model;
        
        for (size_t i = 0; i < market_quotes.size() && i < implied_vols[0].size(); ++i) {
            const auto& quote = market_quotes[i];
            double implied_vol = implied_vols[0][i];
            
            // Calculate model price with implied vol
            MarketData calibrated_market = market;
            calibrated_market.volatility = implied_vol;
            OptionSpec option(quote.strike, quote.time_to_expiry, quote.type);
            
            auto model_result = bs_model.price(option, calibrated_market);
            double model_price = model_result.is_ok() ? model_result.value().price : 0.0;
            
            std::cout << std::setw(6) << std::fixed << std::setprecision(0) << quote.strike
                     << std::setw(7) << std::setprecision(2) << quote.time_to_expiry
                     << std::setw(14) << std::setprecision(2) << quote.market_price
                     << std::setw(13) << std::setprecision(1) << (implied_vol * 100) << "%"
                     << std::setw(13) << std::setprecision(2) << model_price
                     << std::endl;
        }
    }
    
    std::cout << "\n";
}

/**
 * @brief Performance comparison across different models
 */
void demonstrate_performance_comparison() {
    std::cout << "=== Performance Comparison ===\n\n";
    
    MarketData market(100.0, 0.20, 0.05, 0.02);
    OptionSpec call_option(105.0, 0.25, OptionType::Call);
    
    // Black-Scholes (analytical)
    BlackScholesModel bs_model;
    auto bs_start = std::chrono::high_resolution_clock::now();
    auto bs_result = bs_model.price(call_option, market);
    auto bs_end = std::chrono::high_resolution_clock::now();
    auto bs_time = std::chrono::duration_cast<std::chrono::microseconds>(bs_end - bs_start);
    
    // Binomial tree
    BinomialTreeModel tree_model;
    auto tree_start = std::chrono::high_resolution_clock::now();
    auto tree_result = tree_model.price(call_option, market, 1000);
    auto tree_end = std::chrono::high_resolution_clock::now();
    auto tree_time = std::chrono::duration_cast<std::chrono::microseconds>(tree_end - tree_start);
    
    // Heston Monte Carlo
    HestonModel heston_model;
    HestonParameters heston_params(0.04, 2.0, 0.04, -0.7, 0.3);
    auto heston_start = std::chrono::high_resolution_clock::now();
    auto heston_result = heston_model.price_monte_carlo(call_option, market, heston_params, 50000);
    auto heston_end = std::chrono::high_resolution_clock::now();
    auto heston_time = std::chrono::duration_cast<std::chrono::milliseconds>(heston_end - heston_start);
    
    std::cout << "Method              Price      Time          Accuracy\n";
    std::cout << "---------------------------------------------------\n";
    
    if (bs_result.is_ok()) {
        std::cout << std::setw(18) << "Black-Scholes"
                 << std::setw(10) << std::fixed << std::setprecision(4) << bs_result.value().price
                 << std::setw(10) << bs_time.count() << " μs"
                 << std::setw(12) << "Analytical"
                 << std::endl;
    }
    
    if (tree_result.is_ok()) {
        std::cout << std::setw(18) << "Binomial (1000)"
                 << std::setw(10) << tree_result.value().price
                 << std::setw(10) << tree_time.count() << " μs"
                 << std::setw(12) << "High"
                 << std::endl;
    }
    
    if (heston_result.is_ok()) {
        std::cout << std::setw(18) << "Heston MC (50k)"
                 << std::setw(10) << heston_result.value().price
                 << std::setw(8) << heston_time.count() << " ms"
                 << std::setw(12) << "±" << std::setprecision(3) << heston_result.value().standard_error
                 << std::endl;
    }
    
    std::cout << "\n";
}

/**
 * @brief Main demonstration function
 */
int main() {
    std::cout << "PyFolio C++ Advanced Options Pricing Models\n";
    std::cout << "===========================================\n\n";
    
    try {
        demonstrate_black_scholes_pricing();
        demonstrate_heston_model();
        demonstrate_local_volatility_model();
        demonstrate_american_options();
        demonstrate_exotic_options();
        demonstrate_volatility_calibration();
        demonstrate_performance_comparison();
        
        std::cout << "All demonstrations completed successfully!\n";
        std::cout << "\nKey Features Demonstrated:\n";
        std::cout << "✓ Black-Scholes analytical pricing with full Greeks\n";
        std::cout << "✓ Heston stochastic volatility with Monte Carlo\n";
        std::cout << "✓ Local volatility model with PDE and MC methods\n";
        std::cout << "✓ American options with binomial trees\n";
        std::cout << "✓ Exotic options (Asian, Barrier) pricing\n";
        std::cout << "✓ Volatility surface calibration\n";
        std::cout << "✓ Performance benchmarking across models\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error during demonstration: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}