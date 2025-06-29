#include <pyfolio/risk/advanced_risk_models.h>
#include <pyfolio/core/time_series.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <random>
#include <cmath>

using namespace pyfolio::risk;
using namespace pyfolio;

/**
 * @brief Generate sample financial return data with volatility clustering
 */
TimeSeries<double> generate_sample_returns(size_t n_observations = 1000, double annual_vol = 0.20) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> normal(0.0, 1.0);
    
    std::vector<double> returns;
    std::vector<std::string> dates;
    
    // GARCH(1,1) process parameters
    double omega = 0.00001;
    double alpha = 0.05;
    double beta = 0.90;
    double persistence = alpha + beta;
    
    // Initialize volatility
    double h_t = omega / (1.0 - persistence);
    double vol_t = std::sqrt(h_t);
    
    for (size_t t = 0; t < n_observations; ++t) {
        // Generate return
        double epsilon = normal(gen);
        double return_t = vol_t * epsilon;
        returns.push_back(return_t);
        
        // Generate date string
        dates.push_back("2020-01-01"); // Simplified date
        
        // Update volatility for next period
        h_t = omega + alpha * return_t * return_t + beta * h_t;
        vol_t = std::sqrt(h_t);
    }
    
    // Convert string dates to DateTime objects
    std::vector<DateTime> datetime_stamps;
    for (const auto& date_str : dates) {
        auto dt_result = DateTime::parse(date_str);
        if (dt_result.is_ok()) {
            datetime_stamps.push_back(dt_result.value());
        } else {
            datetime_stamps.push_back(DateTime(2020, 1, 1)); // Fallback
        }
    }
    
    return TimeSeries<double>(datetime_stamps, returns);
}

/**
 * @brief Demonstrate GARCH model fitting and forecasting
 */
void demonstrate_garch_modeling() {
    std::cout << "=== GARCH Model Demonstration ===\n\n";
    
    // Generate sample data
    auto returns = generate_sample_returns(1000, 0.20);
    
    std::cout << "Sample Data Statistics:\n";
    auto return_values = returns.values();
    double mean = std::accumulate(return_values.begin(), return_values.end(), 0.0) / return_values.size();
    
    double variance = 0.0;
    for (double ret : return_values) {
        variance += (ret - mean) * (ret - mean);
    }
    variance /= (return_values.size() - 1);
    
    std::cout << "Mean Return: " << std::fixed << std::setprecision(6) << mean << "\n";
    std::cout << "Volatility: " << std::setprecision(4) << std::sqrt(variance * 252) << " (annualized)\n";
    std::cout << "Observations: " << return_values.size() << "\n\n";
    
    // Fit different GARCH models
    std::vector<std::pair<std::string, GARCHType>> models = {
        {"GARCH(1,1)", GARCHType::GARCH},
        {"EGARCH(1,1)", GARCHType::EGARCH},
        {"GJR-GARCH(1,1)", GARCHType::GJR_GARCH}
    };
    
    std::cout << "GARCH Model Estimation Results:\n";
    std::cout << "Model            Log-Likelihood    AIC       BIC       Persistence\n";
    std::cout << "----------------------------------------------------------------\n";
    
    for (const auto& [name, type] : models) {
        GARCHModel garch(type, 1, 1);
        auto fit_result = garch.fit(returns, "normal");
        
        if (fit_result.is_ok()) {
            const auto& params = fit_result.value();
            double persistence = 0.0;
            if (!params.alpha.empty() && !params.beta.empty()) {
                persistence = params.alpha[0] + params.beta[0];
            }
            
            std::cout << std::setw(16) << name
                      << std::setw(15) << std::setprecision(2) << params.log_likelihood
                      << std::setw(10) << std::setprecision(1) << params.aic
                      << std::setw(10) << params.bic
                      << std::setw(12) << std::setprecision(3) << persistence
                      << "\n";
            
            // Volatility forecasting demonstration
            if (name == "GARCH(1,1)") {
                auto forecast_result = garch.forecast_volatility(10);
                if (forecast_result.is_ok()) {
                    const auto& forecasts = forecast_result.value();
                    std::cout << "\nVolatility Forecasts (next 10 periods):\n";
                    for (size_t i = 0; i < forecasts.size(); ++i) {
                        std::cout << "t+" << (i+1) << ": " << std::setprecision(4) 
                                  << forecasts[i] * std::sqrt(252) << " (annualized)\n";
                    }
                }
            }
        } else {
            std::cout << std::setw(16) << name << " [ESTIMATION FAILED]\n";
        }
    }
    
    std::cout << "\n";
}

/**
 * @brief Demonstrate VaR calculation with multiple methods
 */
void demonstrate_var_calculation() {
    std::cout << "=== Value-at-Risk (VaR) Calculation ===\n\n";
    
    // Generate sample data with some extreme events
    auto returns = generate_sample_returns(500, 0.25);
    
    // VaR Calculator
    VaRCalculator var_calc;
    
    std::vector<std::pair<std::string, VaRMethod>> methods = {
        {"Historical Simulation", VaRMethod::HistoricalSimulation},
        {"Parametric (Normal)", VaRMethod::Parametric},
        {"Monte Carlo", VaRMethod::MonteCarlo},
        {"Filtered Historical", VaRMethod::FilteredHistorical},
        {"Cornish-Fisher", VaRMethod::CornishFisher}
    };
    
    std::vector<double> confidence_levels = {0.01, 0.05, 0.10};
    
    std::cout << "VaR Estimates by Method and Confidence Level:\n\n";
    
    for (double cl : confidence_levels) {
        std::cout << "Confidence Level: " << std::setprecision(0) << (1-cl)*100 << "%\n";
        std::cout << "Method                  VaR (%)    ES (%)     Coverage (%)\n";
        std::cout << "--------------------------------------------------------\n";
        
        for (const auto& [name, method] : methods) {
            auto var_result = var_calc.calculate_var(returns, cl, method);
            
            if (var_result.is_ok()) {
                const auto& result = var_result.value();
                
                std::cout << std::setw(23) << name
                          << std::setw(11) << std::setprecision(2) << result.var_estimate * 100
                          << std::setw(11) << result.expected_shortfall * 100
                          << std::setw(12) << std::setprecision(1) << result.coverage_probability * 100
                          << "\n";
            } else {
                std::cout << std::setw(23) << name << " [CALCULATION FAILED]\n";
            }
        }
        std::cout << "\n";
    }
}

/**
 * @brief Demonstrate VaR backtesting
 */
void demonstrate_var_backtesting() {
    std::cout << "=== VaR Backtesting ===\n\n";
    
    // Generate in-sample and out-of-sample data
    auto full_data = generate_sample_returns(1000, 0.20);
    auto full_values = full_data.values();
    
    // Split data: 750 for estimation, 250 for backtesting
    std::vector<double> estimation_data(full_values.begin(), full_values.begin() + 750);
    std::vector<double> backtesting_data(full_values.begin() + 750, full_values.end());
    
    // Create DateTime objects for estimation data
    std::vector<DateTime> estimation_dates;
    for (size_t i = 0; i < estimation_data.size(); ++i) {
        estimation_dates.push_back(DateTime(2020, 1, 1));
    }
    TimeSeries<double> estimation_ts(estimation_dates, estimation_data);
    
    // Create DateTime objects for backtesting data
    std::vector<DateTime> backtesting_dates;
    for (size_t i = 0; i < backtesting_data.size(); ++i) {
        backtesting_dates.push_back(DateTime(2020, 1, 1));
    }
    TimeSeries<double> backtesting_ts(backtesting_dates, backtesting_data);
    
    // Calculate VaR for backtesting period (simplified - using constant VaR)
    VaRCalculator var_calc;
    auto var_result = var_calc.calculate_var(estimation_ts, 0.05, VaRMethod::HistoricalSimulation);
    
    if (var_result.is_ok()) {
        double constant_var = var_result.value().var_estimate;
        
        // Create VaR forecast series (constant)
        std::vector<double> var_forecasts(backtesting_data.size(), constant_var);
        // Create DateTime objects for VaR forecast series
        std::vector<DateTime> var_forecast_dates;
        for (size_t i = 0; i < var_forecasts.size(); ++i) {
            var_forecast_dates.push_back(DateTime(2020, 1, 1));
        }
        TimeSeries<double> var_forecast_ts(var_forecast_dates, var_forecasts);
        
        std::cout << "Backtesting Results (5% VaR):\n";
        std::cout << "VaR Estimate: " << std::setprecision(3) << constant_var * 100 << "%\n";
        std::cout << "Backtesting Period: " << backtesting_data.size() << " observations\n\n";
        
        // Run backtesting
        VaRBacktester backtester;
        
        // Kupiec test
        auto kupiec_result = backtester.kupiec_test(backtesting_ts, var_forecast_ts, 0.05);
        if (kupiec_result.is_ok()) {
            const auto& test = kupiec_result.value();
            std::cout << "Kupiec POF Test:\n";
            std::cout << "  Violations: " << test.violations << " out of " << test.total_observations << "\n";
            std::cout << "  Violation Rate: " << std::setprecision(2) << test.violation_rate * 100 << "%\n";
            std::cout << "  Expected Rate: " << 5.0 << "%\n";
            std::cout << "  Test Statistic: " << std::setprecision(3) << test.test_statistic << "\n";
            std::cout << "  P-value: " << test.p_value << "\n";
            std::cout << "  Result: " << test.interpretation << "\n\n";
        }
        
        // Traffic light test
        auto traffic_result = backtester.traffic_light_test(backtesting_ts, var_forecast_ts, 0.05);
        if (traffic_result.is_ok()) {
            std::cout << "Basel Traffic Light Test: " << traffic_result.value() << "\n\n";
        }
    }
}

/**
 * @brief Demonstrate Extreme Value Theory
 */
void demonstrate_extreme_value_theory() {
    std::cout << "=== Extreme Value Theory (EVT) ===\n\n";
    
    // Generate data with fat tails
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // Mix of normal and extreme events
    std::normal_distribution<double> normal(0.0, 0.01);
    std::normal_distribution<double> extreme(-0.05, 0.02);
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    
    std::vector<double> returns;
    for (int i = 0; i < 1000; ++i) {
        if (uniform(gen) < 0.05) { // 5% extreme events
            returns.push_back(extreme(gen));
        } else {
            returns.push_back(normal(gen));
        }
    }
    
    // Convert to DateTime objects
    std::vector<DateTime> datetime_stamps;
    for (size_t i = 0; i < returns.size(); ++i) {
        datetime_stamps.push_back(DateTime(2020, 1, 1));
    }
    TimeSeries<double> data(datetime_stamps, returns);
    
    std::cout << "Sample Statistics:\n";
    auto min_return = *std::min_element(returns.begin(), returns.end());
    auto max_return = *std::max_element(returns.begin(), returns.end());
    
    std::cout << "Minimum Return: " << std::setprecision(3) << min_return * 100 << "%\n";
    std::cout << "Maximum Return: " << max_return * 100 << "%\n";
    std::cout << "Observations: " << returns.size() << "\n\n";
    
    // Fit EVT model
    ExtremeValueTheory evt;
    auto evt_result = evt.fit_pot_model(data, 0.95);
    
    if (evt_result.is_ok()) {
        const auto& params = evt_result.value();
        
        std::cout << "EVT Model Parameters (Peaks Over Threshold):\n";
        std::cout << "Threshold: " << std::setprecision(3) << params.threshold * 100 << "%\n";
        std::cout << "Shape Parameter (ξ): " << std::setprecision(4) << params.xi << "\n";
        std::cout << "Scale Parameter (σ): " << params.sigma << "\n";
        std::cout << "Number of Exceedances: " << params.n_exceedances << "\n";
        std::cout << "Threshold Quantile: " << std::setprecision(1) << params.threshold_quantile * 100 << "%\n\n";
        
        // Calculate extreme quantiles
        std::vector<double> extreme_confidence_levels = {0.001, 0.005, 0.01};
        
        std::cout << "Extreme Quantile Estimates:\n";
        std::cout << "Confidence Level    Extreme VaR (%)\n";
        std::cout << "-----------------------------------\n";
        
        for (double cl : extreme_confidence_levels) {
            auto quantile_result = evt.calculate_extreme_quantile(cl);
            if (quantile_result.is_ok()) {
                std::cout << std::setw(15) << std::setprecision(1) << (1-cl)*100 << "%"
                          << std::setw(16) << std::setprecision(2) << quantile_result.value() * 100
                          << "\n";
            }
        }
        
        // EVT-based Expected Shortfall
        auto es_result = evt.calculate_evt_expected_shortfall(0.01);
        if (es_result.is_ok()) {
            std::cout << "\nEVT Expected Shortfall (99%): " << std::setprecision(2) 
                      << es_result.value() * 100 << "%\n";
        }
    } else {
        std::cout << "EVT fitting failed: " << evt_result.error().message << "\n";
    }
    
    std::cout << "\n";
}

/**
 * @brief Demonstrate stress testing
 */
void demonstrate_stress_testing() {
    std::cout << "=== Stress Testing ===\n\n";
    
    // Generate portfolio returns
    auto portfolio_returns = generate_sample_returns(500, 0.18);
    auto return_values = portfolio_returns.values();
    
    std::cout << "Portfolio Statistics:\n";
    double mean = std::accumulate(return_values.begin(), return_values.end(), 0.0) / return_values.size();
    double variance = 0.0;
    for (double ret : return_values) {
        variance += (ret - mean) * (ret - mean);
    }
    variance /= (return_values.size() - 1);
    
    std::cout << "Mean Daily Return: " << std::setprecision(4) << mean * 100 << "%\n";
    std::cout << "Daily Volatility: " << std::sqrt(variance) * 100 << "%\n";
    std::cout << "Annualized Volatility: " << std::sqrt(variance * 252) * 100 << "%\n\n";
    
    // Historical stress scenarios
    std::cout << "Historical Stress Test Scenarios:\n";
    std::cout << "Scenario                 1-Day Loss    10-Day Loss   Comments\n";
    std::cout << "----------------------------------------------------------------\n";
    
    // Black Monday 1987 (simplified)
    double black_monday_shock = -0.22; // -22% in one day
    std::cout << std::setw(24) << "Black Monday 1987"
              << std::setw(12) << std::setprecision(1) << black_monday_shock * 100 << "%"
              << std::setw(14) << black_monday_shock * std::sqrt(10) * 100 << "%"
              << "    Market crash\n";
    
    // 2008 Financial Crisis
    double crisis_shock = -0.12; // -12% severe daily loss
    std::cout << std::setw(24) << "2008 Financial Crisis"
              << std::setw(12) << crisis_shock * 100 << "%"
              << std::setw(14) << crisis_shock * std::sqrt(10) * 100 << "%"
              << "    Credit crisis\n";
    
    // COVID-19 March 2020
    double covid_shock = -0.15; // -15% pandemic shock
    std::cout << std::setw(24) << "COVID-19 March 2020"
              << std::setw(12) << covid_shock * 100 << "%"
              << std::setw(14) << covid_shock * std::sqrt(10) * 100 << "%"
              << "    Pandemic shock\n";
    
    // Flash Crash 2010
    double flash_crash = -0.09; // -9% algorithmic crash
    std::cout << std::setw(24) << "Flash Crash 2010"
              << std::setw(12) << flash_crash * 100 << "%"
              << std::setw(14) << flash_crash * std::sqrt(10) * 100 << "%"
              << "    Algo trading\n\n";
    
    // Monte Carlo stress testing
    std::cout << "Monte Carlo Stress Testing (10,000 simulations):\n";
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> shock_dist(0.0, 0.05); // 5% volatility shocks
    
    std::vector<double> stress_returns;
    for (int i = 0; i < 10000; ++i) {
        double shock = shock_dist(gen);
        double stressed_return = mean + shock; // Apply shock to mean return
        stress_returns.push_back(stressed_return);
    }
    
    std::sort(stress_returns.begin(), stress_returns.end());
    
    std::vector<double> percentiles = {0.005, 0.01, 0.05, 0.10};
    std::cout << "Percentile    Stress Loss (%)\n";
    std::cout << "-----------------------------\n";
    
    for (double p : percentiles) {
        size_t index = static_cast<size_t>(p * stress_returns.size());
        double stress_loss = -stress_returns[index];
        std::cout << std::setw(9) << std::setprecision(1) << (1-p)*100 << "%"
                  << std::setw(16) << std::setprecision(2) << stress_loss * 100
                  << "\n";
    }
    
    std::cout << "\n";
}

/**
 * @brief Main demonstration function
 */
int main() {
    std::cout << "PyFolio C++ Advanced Risk Models Demonstration\n";
    std::cout << "==============================================\n\n";
    
    try {
        demonstrate_garch_modeling();
        demonstrate_var_calculation();
        demonstrate_var_backtesting();
        demonstrate_extreme_value_theory();
        demonstrate_stress_testing();
        
        std::cout << "All risk modeling demonstrations completed successfully!\n\n";
        std::cout << "Key Features Demonstrated:\n";
        std::cout << "✓ GARCH volatility modeling (GARCH, EGARCH, GJR-GARCH)\n";
        std::cout << "✓ Multiple VaR calculation methods\n";
        std::cout << "✓ Expected Shortfall (Conditional VaR)\n";
        std::cout << "✓ VaR backtesting with statistical tests\n";
        std::cout << "✓ Extreme Value Theory for tail risk\n";
        std::cout << "✓ Historical and Monte Carlo stress testing\n";
        std::cout << "✓ Basel regulatory compliance metrics\n";
        std::cout << "✓ Model validation and diagnostics\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error during demonstration: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}