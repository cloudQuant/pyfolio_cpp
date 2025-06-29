#pragma once

#include "../core/datetime.h"
#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../core/types.h"
#include "../math/statistics.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <random>
#include <vector>

namespace pyfolio::risk {

/**
 * @brief VaR calculation methodology
 */
enum class VaRMethod { Historical, Parametric, MonteCarlo, CornishFisher, ExtremeValue };

/**
 * @brief VaR time horizon
 */
enum class VaRHorizon { Daily = 1, Weekly = 7, Monthly = 30, Quarterly = 90, Annual = 252 };

/**
 * @brief Comprehensive VaR results
 */
struct VaRResult {
    double var_estimate;
    double cvar_estimate;  // Conditional VaR (Expected Shortfall)
    double confidence_level;
    VaRMethod method;
    VaRHorizon horizon;
    size_t sample_size;

    // Additional statistics
    double portfolio_volatility;
    double skewness;
    double kurtosis;
    double max_drawdown;

    /**
     * @brief Scale VaR to different time horizon
     */
    VaRResult scale_to_horizon(VaRHorizon new_horizon) const {
        VaRResult scaled    = *this;
        double scale_factor = std::sqrt(static_cast<double>(new_horizon) / static_cast<double>(horizon));
        scaled.var_estimate *= scale_factor;
        scaled.cvar_estimate *= scale_factor;
        scaled.portfolio_volatility *= scale_factor;
        scaled.horizon = new_horizon;
        return scaled;
    }

    /**
     * @brief Get VaR as percentage of portfolio value
     */
    double var_percentage() const { return std::abs(var_estimate) * 100.0; }

    /**
     * @brief Get CVaR as percentage of portfolio value
     */
    double cvar_percentage() const { return std::abs(cvar_estimate) * 100.0; }
};

/**
 * @brief Marginal VaR analysis results
 */
struct MarginalVaRResult {
    std::map<Symbol, double> marginal_var;
    std::map<Symbol, double> component_var;
    std::map<Symbol, double> percentage_contribution;
    double total_var;

    /**
     * @brief Get marginal VaR for a specific asset
     */
    double get_marginal_var(const Symbol& symbol) const {
        auto it = marginal_var.find(symbol);
        return (it != marginal_var.end()) ? it->second : 0.0;
    }

    /**
     * @brief Get component VaR for a specific asset
     */
    double get_component_var(const Symbol& symbol) const {
        auto it = component_var.find(symbol);
        return (it != component_var.end()) ? it->second : 0.0;
    }

    /**
     * @brief Get percentage contribution for a specific asset
     */
    double get_percentage_contribution(const Symbol& symbol) const {
        auto it = percentage_contribution.find(symbol);
        return (it != percentage_contribution.end()) ? it->second : 0.0;
    }
};

/**
 * @brief Stress testing scenarios
 */
struct StressTestScenario {
    std::string name;
    std::map<Symbol, double> shock_factors;       // Multiplicative shocks
    std::map<std::string, double> market_shocks;  // Factor shocks
    double probability;

    /**
     * @brief Apply scenario to return series
     */
    std::vector<double> apply_to_returns(const std::vector<double>& base_returns, const Symbol& symbol) const {
        auto shock_it = shock_factors.find(symbol);
        double shock  = (shock_it != shock_factors.end()) ? shock_it->second : 1.0;

        std::vector<double> shocked_returns;
        shocked_returns.reserve(base_returns.size());

        for (double ret : base_returns) {
            shocked_returns.push_back(ret * shock);
        }

        return shocked_returns;
    }
};

/**
 * @brief Advanced VaR calculator with multiple methodologies
 */
class VaRCalculator {
  private:
    mutable std::mt19937 rng_;

  public:
    VaRCalculator() : rng_(std::random_device{}()) {}

    explicit VaRCalculator(uint32_t seed) : rng_(seed) {}

    /**
     * @brief Calculate VaR using historical simulation
     */
    Result<VaRResult> calculate_historical_var(const ReturnSeries& returns, double confidence_level = 0.95,
                                               VaRHorizon horizon = VaRHorizon::Daily) const {
        if (returns.empty()) {
            return Result<VaRResult>::error(ErrorCode::InsufficientData, "Return series cannot be empty");
        }

        if (confidence_level <= 0.0 || confidence_level >= 1.0) {
            return Result<VaRResult>::error(ErrorCode::InvalidInput, "Confidence level must be between 0 and 1");
        }

        const auto& values = returns.values();

        // Calculate basic statistics
        auto mean_result = stats::mean(std::span<const double>{values});
        auto std_result  = stats::standard_deviation(std::span<const double>{values});
        auto skew_result = stats::skewness(std::span<const double>{values});
        auto kurt_result = stats::kurtosis(std::span<const double>{values});

        if (mean_result.is_error() || std_result.is_error()) {
            return Result<VaRResult>::error(ErrorCode::CalculationError, "Failed to calculate basic statistics");
        }

        // Calculate VaR and CVaR
        auto var_result  = stats::value_at_risk(std::span<const double>{values}, confidence_level);
        auto cvar_result = stats::conditional_value_at_risk(std::span<const double>{values}, confidence_level);

        if (var_result.is_error() || cvar_result.is_error()) {
            return Result<VaRResult>::error(ErrorCode::CalculationError, "Failed to calculate VaR or CVaR");
        }

        // Calculate maximum drawdown
        double max_dd     = 0.0;
        double peak       = 0.0;
        double cumulative = 0.0;

        for (double ret : values) {
            cumulative += ret;
            peak            = std::max(peak, cumulative);
            double drawdown = peak - cumulative;
            max_dd          = std::max(max_dd, drawdown);
        }

        VaRResult result;
        result.var_estimate         = var_result.value();
        result.cvar_estimate        = cvar_result.value();
        result.confidence_level     = confidence_level;
        result.method               = VaRMethod::Historical;
        result.horizon              = horizon;
        result.sample_size          = values.size();
        result.portfolio_volatility = std_result.value();
        result.skewness             = skew_result.is_ok() ? skew_result.value() : 0.0;
        result.kurtosis             = kurt_result.is_ok() ? kurt_result.value() : 3.0;
        result.max_drawdown         = max_dd;

        return Result<VaRResult>::success(result);
    }

    /**
     * @brief Calculate VaR using parametric (normal distribution) method
     */
    Result<VaRResult> calculate_parametric_var(const ReturnSeries& returns, double confidence_level = 0.95,
                                               VaRHorizon horizon = VaRHorizon::Daily) const {
        if (returns.empty()) {
            return Result<VaRResult>::error(ErrorCode::InsufficientData, "Return series cannot be empty");
        }

        const auto& values = returns.values();

        auto mean_result = stats::mean(std::span<const double>{values});
        auto std_result  = stats::standard_deviation(std::span<const double>{values});

        if (mean_result.is_error() || std_result.is_error()) {
            return Result<VaRResult>::error(ErrorCode::CalculationError,
                                            "Failed to calculate mean or standard deviation");
        }

        double mean    = mean_result.value();
        double std_dev = std_result.value();

        // Normal distribution VaR
        double z_score      = stats::normal_ppf(1.0 - confidence_level);
        double var_estimate = mean + z_score * std_dev;

        // For parametric CVaR (normal distribution)
        double phi_z         = stats::normal_pdf(z_score);
        double cvar_estimate = mean - std_dev * phi_z / (1.0 - confidence_level);

        VaRResult result;
        result.var_estimate         = var_estimate;
        result.cvar_estimate        = cvar_estimate;
        result.confidence_level     = confidence_level;
        result.method               = VaRMethod::Parametric;
        result.horizon              = horizon;
        result.sample_size          = values.size();
        result.portfolio_volatility = std_dev;

        // Calculate skewness and kurtosis for diagnostics
        auto skew_result = stats::skewness(std::span<const double>{values});
        auto kurt_result = stats::kurtosis(std::span<const double>{values});
        result.skewness  = skew_result.is_ok() ? skew_result.value() : 0.0;
        result.kurtosis  = kurt_result.is_ok() ? kurt_result.value() : 3.0;

        return Result<VaRResult>::success(result);
    }

    /**
     * @brief Calculate VaR using Cornish-Fisher expansion (adjusts for skewness and kurtosis)
     */
    Result<VaRResult> calculate_cornish_fisher_var(const ReturnSeries& returns, double confidence_level = 0.95,
                                                   VaRHorizon horizon = VaRHorizon::Daily) const {
        if (returns.empty()) {
            return Result<VaRResult>::error(ErrorCode::InsufficientData, "Return series cannot be empty");
        }

        const auto& values = returns.values();

        auto mean_result = stats::mean(std::span<const double>{values});
        auto std_result  = stats::standard_deviation(std::span<const double>{values});
        auto skew_result = stats::skewness(std::span<const double>{values});
        auto kurt_result = stats::kurtosis(std::span<const double>{values});

        if (mean_result.is_error() || std_result.is_error() || skew_result.is_error() || kurt_result.is_error()) {
            return Result<VaRResult>::error(ErrorCode::CalculationError, "Failed to calculate required statistics");
        }

        double mean            = mean_result.value();
        double std_dev         = std_result.value();
        double skewness        = skew_result.value();
        double kurtosis        = kurt_result.value();
        double excess_kurtosis = kurtosis - 3.0;

        // Normal quantile
        double z = stats::normal_ppf(1.0 - confidence_level);

        // Cornish-Fisher expansion
        double cf_adjustment = (1.0 / 6.0) * skewness * (z * z - 1.0) +
                               (1.0 / 24.0) * excess_kurtosis * (z * z * z - 3.0 * z) -
                               (1.0 / 36.0) * skewness * skewness * (2.0 * z * z * z - 5.0 * z);

        double z_cf         = z + cf_adjustment;
        double var_estimate = mean + z_cf * std_dev;

        // Approximate CVaR using adjusted quantile
        double phi_z_cf      = stats::normal_pdf(z_cf);
        double cvar_estimate = mean - std_dev * phi_z_cf / (1.0 - confidence_level);

        VaRResult result;
        result.var_estimate         = var_estimate;
        result.cvar_estimate        = cvar_estimate;
        result.confidence_level     = confidence_level;
        result.method               = VaRMethod::CornishFisher;
        result.horizon              = horizon;
        result.sample_size          = values.size();
        result.portfolio_volatility = std_dev;
        result.skewness             = skewness;
        result.kurtosis             = kurtosis;

        return Result<VaRResult>::success(result);
    }

    /**
     * @brief Calculate VaR using Monte Carlo simulation
     */
    Result<VaRResult> calculate_monte_carlo_var(const ReturnSeries& returns, double confidence_level = 0.95,
                                                VaRHorizon horizon     = VaRHorizon::Daily,
                                                size_t num_simulations = 10000) const {
        if (returns.empty()) {
            return Result<VaRResult>::error(ErrorCode::InsufficientData, "Return series cannot be empty");
        }

        const auto& values = returns.values();

        auto mean_result = stats::mean(std::span<const double>{values});
        auto std_result  = stats::standard_deviation(std::span<const double>{values});

        if (mean_result.is_error() || std_result.is_error()) {
            return Result<VaRResult>::error(ErrorCode::CalculationError,
                                            "Failed to calculate mean or standard deviation");
        }

        double mean    = mean_result.value();
        double std_dev = std_result.value();

        // Generate Monte Carlo simulations
        std::vector<double> simulated_returns;
        simulated_returns.reserve(num_simulations);

        std::normal_distribution<double> normal_dist(mean, std_dev);

        for (size_t i = 0; i < num_simulations; ++i) {
            simulated_returns.push_back(normal_dist(rng_));
        }

        // Calculate VaR and CVaR from simulated returns
        auto var_result = stats::value_at_risk(std::span<const double>{simulated_returns}, confidence_level);
        auto cvar_result =
            stats::conditional_value_at_risk(std::span<const double>{simulated_returns}, confidence_level);

        if (var_result.is_error() || cvar_result.is_error()) {
            return Result<VaRResult>::error(ErrorCode::CalculationError,
                                            "Failed to calculate VaR from Monte Carlo simulation");
        }

        VaRResult result;
        result.var_estimate         = var_result.value();
        result.cvar_estimate        = cvar_result.value();
        result.confidence_level     = confidence_level;
        result.method               = VaRMethod::MonteCarlo;
        result.horizon              = horizon;
        result.sample_size          = num_simulations;
        result.portfolio_volatility = std_dev;

        // Calculate skewness and kurtosis from original data
        auto skew_result = stats::skewness(std::span<const double>{values});
        auto kurt_result = stats::kurtosis(std::span<const double>{values});
        result.skewness  = skew_result.is_ok() ? skew_result.value() : 0.0;
        result.kurtosis  = kurt_result.is_ok() ? kurt_result.value() : 3.0;

        return Result<VaRResult>::success(result);
    }

    /**
     * @brief Calculate marginal VaR for portfolio components
     */
    Result<MarginalVaRResult> calculate_marginal_var(const std::map<Symbol, ReturnSeries>& asset_returns,
                                                     const std::map<Symbol, double>& portfolio_weights,
                                                     double confidence_level = 0.95) const {
        if (asset_returns.empty() || portfolio_weights.empty()) {
            return Result<MarginalVaRResult>::error(ErrorCode::InvalidInput,
                                                    "Asset returns and portfolio weights cannot be empty");
        }

        // Create portfolio return series
        auto portfolio_returns_result = create_portfolio_return_series(asset_returns, portfolio_weights);
        if (portfolio_returns_result.is_error()) {
            return Result<MarginalVaRResult>::error(portfolio_returns_result.error().code,
                                                    portfolio_returns_result.error().message);
        }

        const auto& portfolio_returns = portfolio_returns_result.value();

        // Calculate portfolio VaR
        auto portfolio_var_result = calculate_historical_var(portfolio_returns, confidence_level);
        if (portfolio_var_result.is_error()) {
            return Result<MarginalVaRResult>::error(portfolio_var_result.error().code,
                                                    portfolio_var_result.error().message);
        }

        double portfolio_var = portfolio_var_result.value().var_estimate;

        MarginalVaRResult result;
        result.total_var = portfolio_var;

        // Calculate marginal VaR for each asset
        for (const auto& [symbol, weight] : portfolio_weights) {
            // Create modified portfolio with small weight change
            const double delta    = 0.001;  // 0.1% weight change
            auto modified_weights = portfolio_weights;
            modified_weights[symbol] += delta;

            // Renormalize weights
            double total_weight = 0.0;
            for (auto& [sym, w] : modified_weights) {
                total_weight += w;
            }
            for (auto& [sym, w] : modified_weights) {
                w /= total_weight;
            }

            // Calculate VaR for modified portfolio
            auto modified_returns_result = create_portfolio_return_series(asset_returns, modified_weights);
            if (modified_returns_result.is_ok()) {
                auto modified_var_result = calculate_historical_var(modified_returns_result.value(), confidence_level);
                if (modified_var_result.is_ok()) {
                    double modified_var = modified_var_result.value().var_estimate;

                    // Marginal VaR = (VaR_new - VaR_old) / delta_weight
                    double marginal_var         = (modified_var - portfolio_var) / delta;
                    result.marginal_var[symbol] = marginal_var;

                    // Component VaR = weight * marginal VaR
                    double component_var         = weight * marginal_var;
                    result.component_var[symbol] = component_var;

                    // Percentage contribution
                    if (portfolio_var != 0.0) {
                        result.percentage_contribution[symbol] = (component_var / portfolio_var) * 100.0;
                    }
                }
            }
        }

        return Result<MarginalVaRResult>::success(std::move(result));
    }

    /**
     * @brief Perform stress testing with predefined scenarios
     */
    Result<std::map<std::string, VaRResult>> stress_test(const std::map<Symbol, ReturnSeries>& asset_returns,
                                                         const std::map<Symbol, double>& portfolio_weights,
                                                         const std::vector<StressTestScenario>& scenarios,
                                                         double confidence_level = 0.95) const {
        std::map<std::string, VaRResult> stress_results;

        for (const auto& scenario : scenarios) {
            // Apply scenario shocks to each asset
            std::map<Symbol, ReturnSeries> shocked_returns;

            for (const auto& [symbol, returns] : asset_returns) {
                auto shocked_values     = scenario.apply_to_returns(returns.values(), symbol);
                shocked_returns[symbol] = ReturnSeries{returns.timestamps(), shocked_values, symbol};
            }

            // Create stressed portfolio returns
            auto stressed_portfolio_result = create_portfolio_return_series(shocked_returns, portfolio_weights);
            if (stressed_portfolio_result.is_ok()) {
                auto stressed_var_result =
                    calculate_historical_var(stressed_portfolio_result.value(), confidence_level);
                if (stressed_var_result.is_ok()) {
                    stress_results[scenario.name] = stressed_var_result.value();
                }
            }
        }

        return Result<std::map<std::string, VaRResult>>::success(std::move(stress_results));
    }

  private:
    /**
     * @brief Create portfolio return series from asset returns and weights
     */
    Result<ReturnSeries> create_portfolio_return_series(const std::map<Symbol, ReturnSeries>& asset_returns,
                                                        const std::map<Symbol, double>& portfolio_weights) const {
        if (asset_returns.empty()) {
            return Result<ReturnSeries>::error(ErrorCode::InvalidInput, "Asset returns cannot be empty");
        }

        // Find common time period
        size_t min_size = std::numeric_limits<size_t>::max();
        for (const auto& [symbol, returns] : asset_returns) {
            min_size = std::min(min_size, returns.size());
        }

        if (min_size == 0) {
            return Result<ReturnSeries>::error(ErrorCode::InsufficientData, "No common return data available");
        }

        // Get timestamps from first series
        const auto& first_series = asset_returns.begin()->second;
        std::vector<DateTime> timestamps(first_series.timestamps().end() - min_size, first_series.timestamps().end());

        // Calculate portfolio returns
        std::vector<double> portfolio_returns;
        portfolio_returns.reserve(min_size);

        for (size_t i = 0; i < min_size; ++i) {
            double portfolio_return = 0.0;

            for (const auto& [symbol, weight] : portfolio_weights) {
                auto asset_it = asset_returns.find(symbol);
                if (asset_it != asset_returns.end()) {
                    const auto& asset_values = asset_it->second.values();
                    size_t asset_size        = asset_values.size();
                    if (i < asset_size) {
                        size_t index = asset_size - min_size + i;
                        portfolio_return += weight * asset_values[index];
                    }
                }
            }

            portfolio_returns.push_back(portfolio_return);
        }

        return Result<ReturnSeries>::success(ReturnSeries{timestamps, portfolio_returns, "Portfolio"});
    }
};

/**
 * @brief Common stress test scenarios
 */
namespace stress_scenarios {

/**
 * @brief Create 2008 financial crisis scenario
 */
inline StressTestScenario financial_crisis_2008() {
    StressTestScenario scenario;
    scenario.name          = "Financial Crisis 2008";
    scenario.shock_factors = {
        {"SPY", 0.63}, // S&P 500 down 37%
        {"IWM", 0.66}, // Russell 2000 down 34%
        {"EFA", 0.57}, // EAFE down 43%
        {"TLT", 1.20}, // Long bonds up 20%
        {"GLD", 1.05}, // Gold up 5%
    };
    scenario.probability = 0.01;  // 1% annual probability
    return scenario;
}

/**
 * @brief Create COVID-19 market crash scenario
 */
inline StressTestScenario covid_crash_2020() {
    StressTestScenario scenario;
    scenario.name          = "COVID-19 Crash 2020";
    scenario.shock_factors = {
        {"SPY", 0.66}, // S&P 500 down 34%
        {"IWM", 0.59}, // Russell 2000 down 41%
        {"EFA", 0.68}, // EAFE down 32%
        {"TLT", 1.11}, // Long bonds up 11%
        {"GLD", 1.00}, // Gold flat
    };
    scenario.probability = 0.005;  // 0.5% annual probability
    return scenario;
}

/**
 * @brief Create interest rate shock scenario
 */
inline StressTestScenario interest_rate_shock() {
    StressTestScenario scenario;
    scenario.name          = "Interest Rate Shock +300bp";
    scenario.market_shocks = {
        {"10Y_YIELD", 3.0}  // 300 basis points
    };
    scenario.shock_factors = {
        {      "TLT", 0.85}, // Long bonds down 15%
        {    "REITs", 0.90}, // REITs down 10%
        {"Utilities", 0.95}  // Utilities down 5%
    };
    scenario.probability = 0.02;  // 2% annual probability
    return scenario;
}

/**
 * @brief Get all common stress scenarios
 */
inline std::vector<StressTestScenario> get_common_scenarios() {
    return {financial_crisis_2008(), covid_crash_2020(), interest_rate_shock()};
}

}  // namespace stress_scenarios

/**
 * @brief Calculate VaR using historical simulation
 */
inline Result<double> historical_var(const ReturnSeries& returns,
                                     double confidence_level = constants::DEFAULT_CONFIDENCE_LEVEL) {
    VaRCalculator calculator;
    auto result = calculator.calculate_historical_var(returns, confidence_level);
    if (result.is_ok()) {
        return Result<double>::success(result.value().var_estimate);
    }
    return Result<double>::error(result.error().code, result.error().message);
}

/**
 * @brief Calculate Conditional Value at Risk (Expected Shortfall)
 */
inline Result<double> conditional_var(const ReturnSeries& returns,
                                      double confidence_level = constants::DEFAULT_CONFIDENCE_LEVEL) {
    VaRCalculator calculator;
    auto result = calculator.calculate_historical_var(returns, confidence_level);
    if (result.is_ok()) {
        return Result<double>::success(result.value().cvar_estimate);
    }
    return Result<double>::error(result.error().code, result.error().message);
}

}  // namespace pyfolio::risk