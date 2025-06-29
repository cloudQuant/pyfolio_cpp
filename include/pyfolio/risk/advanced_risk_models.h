#pragma once

/**
 * @file advanced_risk_models.h
 * @brief Advanced risk modeling framework with GARCH, VaR backtesting, and Expected Shortfall
 * @version 1.0.0
 * @date 2024
 * 
 * @section overview Overview
 * This module implements sophisticated risk models for financial portfolio analysis:
 * - GARCH family models for volatility forecasting (GARCH, EGARCH, GJR-GARCH, TGARCH)
 * - Comprehensive VaR estimation with multiple methodologies
 * - VaR backtesting framework with statistical tests
 * - Expected Shortfall (Conditional VaR) calculations
 * - Extreme Value Theory (EVT) for tail risk modeling
 * - Copula-based dependency modeling
 * - Stress testing and scenario analysis
 * 
 * @section features Key Features
 * - **GARCH Models**: Asymmetric volatility, leverage effects, fat tails
 * - **VaR Methods**: Historical, parametric, Monte Carlo, filtered historical
 * - **Backtesting**: Kupiec, Christoffersen, Dynamic Quantile tests
 * - **Expected Shortfall**: Non-parametric and parametric estimation
 * - **EVT Models**: POT (Peaks Over Threshold), Block Maxima, GPD fitting
 * - **Copulas**: Gaussian, t-copula, Archimedean families
 * - **Performance**: SIMD optimization, parallel processing, GPU acceleration
 * 
 * @section usage Usage Example
 * @code{.cpp}
 * #include <pyfolio/risk/advanced_risk_models.h>
 * 
 * using namespace pyfolio::risk;
 * 
 * // Fit GARCH model
 * GARCHModel garch(GARCHType::EGARCH, 1, 1);
 * auto fit_result = garch.fit(returns);
 * 
 * // Calculate VaR
 * VaRCalculator var_calc;
 * auto var_95 = var_calc.calculate_var(returns, 0.05, VaRMethod::HistoricalSimulation);
 * 
 * // Backtest VaR
 * VaRBacktester backtester;
 * auto test_results = backtester.run_comprehensive_tests(returns, var_estimates);
 * 
 * // Expected Shortfall
 * auto es_95 = var_calc.calculate_expected_shortfall(returns, 0.05);
 * @endcode
 */

#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../core/types.h"
#include "../analytics/performance_metrics.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>
#include <chrono>
#include <random>

namespace pyfolio::risk {

/**
 * @brief GARCH model types
 */
enum class GARCHType {
    GARCH,      ///< Standard GARCH(p,q)
    EGARCH,     ///< Exponential GARCH (Nelson 1991)
    GJR_GARCH,  ///< GJR-GARCH (Glosten-Jagannathan-Runkle)
    TGARCH,     ///< Threshold GARCH
    FIGARCH,    ///< Fractionally Integrated GARCH
    COGARCH     ///< Continuous-time GARCH
};

/**
 * @brief VaR calculation methods
 */
enum class VaRMethod {
    HistoricalSimulation,    ///< Non-parametric historical simulation
    Parametric,             ///< Parametric (normal distribution)
    MonteCarlo,             ///< Monte Carlo simulation
    FilteredHistorical,     ///< GARCH-filtered historical simulation
    ExtremeValueTheory,     ///< EVT-based VaR
    CornishFisher,          ///< Cornish-Fisher expansion
    Bootstrap              ///< Bootstrap resampling
};

/**
 * @brief VaR backtesting tests
 */
enum class BacktestType {
    Kupiec,           ///< Kupiec POF test
    Christoffersen,   ///< Christoffersen independence test
    DynamicQuantile,  ///< Dynamic quantile test
    ConditionalCoverage,  ///< Conditional coverage test
    DurationBased,    ///< Duration-based tests
    TrafficLight     ///< Basel traffic light approach
};

/**
 * @brief Copula types for dependency modeling
 */
enum class CopulaType {
    Gaussian,     ///< Gaussian copula
    Student_t,    ///< Student-t copula
    Clayton,      ///< Clayton copula
    Gumbel,       ///< Gumbel copula
    Frank,        ///< Frank copula
    Joe,          ///< Joe copula
    BB1,          ///< BB1 copula
    BB7           ///< BB7 copula
};

/**
 * @brief GARCH model parameters
 */
struct GARCHParameters {
    double omega;                    ///< Constant term
    std::vector<double> alpha;       ///< ARCH coefficients
    std::vector<double> beta;        ///< GARCH coefficients
    std::vector<double> gamma;       ///< Asymmetry coefficients (EGARCH, GJR)
    double shape_parameter;          ///< Shape parameter for distributions
    double leverage_effect;          ///< Leverage effect parameter
    
    // Model diagnostics
    double log_likelihood;           ///< Log-likelihood value
    double aic;                     ///< Akaike Information Criterion
    double bic;                     ///< Bayesian Information Criterion
    std::vector<double> standard_errors;  ///< Parameter standard errors
    
    GARCHParameters() : omega(0.0), shape_parameter(0.0), leverage_effect(0.0),
                       log_likelihood(0.0), aic(0.0), bic(0.0) {}
};

/**
 * @brief VaR estimation results
 */
struct VaRResult {
    double var_estimate;             ///< VaR estimate
    double confidence_level;         ///< Confidence level (e.g., 0.95)
    VaRMethod method;               ///< Estimation method used
    double expected_shortfall;       ///< Expected Shortfall (CVaR)
    double standard_error;          ///< Standard error of estimate
    std::vector<double> var_contributions;  ///< Component VaR
    
    // Model validation metrics
    double coverage_probability;     ///< Empirical coverage probability
    double maximum_loss;            ///< Maximum observed loss
    double tail_index;              ///< Extreme value tail index
    
    VaRResult() : var_estimate(0.0), confidence_level(0.95), method(VaRMethod::HistoricalSimulation),
                 expected_shortfall(0.0), standard_error(0.0), coverage_probability(0.0),
                 maximum_loss(0.0), tail_index(0.0) {}
};

/**
 * @brief VaR backtesting results
 */
struct BacktestResult {
    BacktestType test_type;          ///< Type of backtest
    double test_statistic;           ///< Test statistic value
    double p_value;                 ///< P-value of test
    double critical_value;          ///< Critical value at 5% level
    bool reject_null;               ///< Whether to reject null hypothesis
    std::string interpretation;      ///< Human-readable interpretation
    
    // Additional statistics
    size_t violations;              ///< Number of VaR violations
    size_t total_observations;      ///< Total observations
    double violation_rate;          ///< Empirical violation rate
    double expected_violations;     ///< Expected number of violations
    
    BacktestResult() : test_type(BacktestType::Kupiec), test_statistic(0.0), p_value(0.0),
                      critical_value(0.0), reject_null(false), violations(0),
                      total_observations(0), violation_rate(0.0), expected_violations(0.0) {}
};

/**
 * @brief Extreme Value Theory parameters
 */
struct EVTParameters {
    double xi;                      ///< Shape parameter (tail index)
    double sigma;                   ///< Scale parameter
    double mu;                      ///< Location parameter (for GEV)
    double threshold;               ///< Threshold (for POT)
    size_t n_exceedances;          ///< Number of threshold exceedances
    double threshold_quantile;      ///< Threshold as quantile
    
    // Goodness of fit
    double anderson_darling;        ///< Anderson-Darling test statistic
    double kolmogorov_smirnov;     ///< Kolmogorov-Smirnov test statistic
    double log_likelihood;         ///< Log-likelihood
    
    EVTParameters() : xi(0.0), sigma(1.0), mu(0.0), threshold(0.0), n_exceedances(0),
                     threshold_quantile(0.95), anderson_darling(0.0),
                     kolmogorov_smirnov(0.0), log_likelihood(0.0) {}
};

/**
 * @brief GARCH model implementation with multiple variants
 */
class GARCHModel {
public:
    /**
     * @brief Constructor
     * @param type GARCH model type
     * @param p ARCH order
     * @param q GARCH order
     */
    GARCHModel(GARCHType type = GARCHType::GARCH, int p = 1, int q = 1);
    
    /**
     * @brief Fit GARCH model to time series
     * @param returns Return series
     * @param distribution Error distribution ("normal", "t", "ged")
     * @return Estimation result
     */
    [[nodiscard]] Result<GARCHParameters> fit(const TimeSeries<double>& returns, 
                                             const std::string& distribution = "normal");
    
    /**
     * @brief Forecast conditional volatility
     * @param steps Number of steps ahead
     * @return Volatility forecasts
     */
    [[nodiscard]] Result<std::vector<double>> forecast_volatility(int steps = 1) const;
    
    /**
     * @brief Calculate residuals from fitted model
     * @return Standardized residuals
     */
    [[nodiscard]] Result<std::vector<double>> get_residuals() const;
    
    /**
     * @brief Calculate conditional volatility series
     * @return Conditional volatility estimates
     */
    [[nodiscard]] Result<std::vector<double>> get_conditional_volatility() const;
    
    /**
     * @brief Model diagnostics and tests
     * @return Diagnostic results
     */
    [[nodiscard]] Result<std::unordered_map<std::string, double>> run_diagnostics() const;
    
    /**
     * @brief Simulate from fitted GARCH model
     * @param n_simulations Number of simulations
     * @param horizon Simulation horizon
     * @return Simulated paths
     */
    [[nodiscard]] Result<std::vector<std::vector<double>>> simulate(
        size_t n_simulations = 1000, size_t horizon = 252) const;

private:
    GARCHType model_type_;
    int p_order_;                   ///< ARCH order
    int q_order_;                   ///< GARCH order
    std::string distribution_;      ///< Error distribution
    GARCHParameters parameters_;    ///< Fitted parameters
    std::vector<double> returns_;   ///< Original return series
    std::vector<double> residuals_; ///< Standardized residuals
    std::vector<double> volatility_; ///< Conditional volatility
    bool is_fitted_;               ///< Whether model is fitted
    
    // Maximum likelihood estimation
    [[nodiscard]] Result<GARCHParameters> estimate_mle(const std::vector<double>& returns);
    
    // Log-likelihood calculation
    [[nodiscard]] double calculate_log_likelihood(const std::vector<double>& params,
                                                  const std::vector<double>& returns) const;
    
    // Volatility filtering
    void filter_volatility(const std::vector<double>& returns, 
                          const GARCHParameters& params);
    
    // ARCH/GARCH effects tests
    [[nodiscard]] double ljung_box_test(const std::vector<double>& residuals, int lags = 10) const;
};

/**
 * @brief Value-at-Risk calculator with multiple methods
 */
class VaRCalculator {
public:
    /**
     * @brief Constructor
     */
    VaRCalculator();
    
    /**
     * @brief Calculate VaR using specified method
     * @param returns Return series
     * @param confidence_level Confidence level (e.g., 0.95)
     * @param method Calculation method
     * @param window_size Rolling window size (0 for expanding window)
     * @return VaR estimates
     */
    [[nodiscard]] Result<VaRResult> calculate_var(const TimeSeries<double>& returns,
                                                 double confidence_level = 0.05,
                                                 VaRMethod method = VaRMethod::HistoricalSimulation,
                                                 size_t window_size = 250);
    
    /**
     * @brief Calculate Expected Shortfall (Conditional VaR)
     * @param returns Return series
     * @param confidence_level Confidence level
     * @param method Calculation method
     * @return Expected Shortfall estimate
     */
    [[nodiscard]] Result<double> calculate_expected_shortfall(const TimeSeries<double>& returns,
                                                             double confidence_level = 0.05,
                                                             VaRMethod method = VaRMethod::HistoricalSimulation);
    
    /**
     * @brief Calculate component VaR for portfolio
     * @param returns Asset return matrix
     * @param weights Portfolio weights
     * @param confidence_level Confidence level
     * @return Component VaR contributions
     */
    [[nodiscard]] Result<std::vector<double>> calculate_component_var(
        const std::vector<TimeSeries<double>>& returns,
        const std::vector<double>& weights,
        double confidence_level = 0.05);
    
    /**
     * @brief Calculate marginal VaR
     * @param returns Asset return matrix
     * @param weights Portfolio weights
     * @param confidence_level Confidence level
     * @return Marginal VaR for each asset
     */
    [[nodiscard]] Result<std::vector<double>> calculate_marginal_var(
        const std::vector<TimeSeries<double>>& returns,
        const std::vector<double>& weights,
        double confidence_level = 0.05);
    
    /**
     * @brief Calculate rolling VaR estimates
     * @param returns Return series
     * @param confidence_level Confidence level
     * @param window_size Rolling window size
     * @param method Calculation method
     * @return Rolling VaR series
     */
    [[nodiscard]] Result<TimeSeries<double>> calculate_rolling_var(const TimeSeries<double>& returns,
                                                          double confidence_level = 0.05,
                                                          size_t window_size = 250,
                                                          VaRMethod method = VaRMethod::HistoricalSimulation);

private:
    std::unique_ptr<GARCHModel> garch_model_;
    mutable std::mt19937 rng_;
    
    // Different VaR calculation methods
    [[nodiscard]] Result<double> historical_simulation_var(const std::vector<double>& returns,
                                                          double confidence_level) const;
    
    [[nodiscard]] Result<double> parametric_var(const std::vector<double>& returns,
                                               double confidence_level) const;
    
    [[nodiscard]] Result<double> monte_carlo_var(const std::vector<double>& returns,
                                                double confidence_level,
                                                size_t n_simulations = 10000) const;
    
    [[nodiscard]] Result<double> filtered_historical_var(const std::vector<double>& returns,
                                                        double confidence_level) const;
    
    [[nodiscard]] Result<double> evt_var(const std::vector<double>& returns,
                                        double confidence_level) const;
    
    [[nodiscard]] Result<double> cornish_fisher_var(const std::vector<double>& returns,
                                                   double confidence_level) const;
};

/**
 * @brief VaR backtesting framework
 */
class VaRBacktester {
public:
    /**
     * @brief Constructor
     */
    VaRBacktester();
    
    /**
     * @brief Run Kupiec POF test
     * @param returns Actual returns
     * @param var_forecasts VaR forecasts
     * @param confidence_level Confidence level
     * @return Test result
     */
    [[nodiscard]] Result<BacktestResult> kupiec_test(const TimeSeries<double>& returns,
                                                    const TimeSeries<double>& var_forecasts,
                                                    double confidence_level = 0.05) const;
    
    /**
     * @brief Run Christoffersen independence test
     * @param returns Actual returns
     * @param var_forecasts VaR forecasts
     * @param confidence_level Confidence level
     * @return Test result
     */
    [[nodiscard]] Result<BacktestResult> christoffersen_test(const TimeSeries<double>& returns,
                                                            const TimeSeries<double>& var_forecasts,
                                                            double confidence_level = 0.05) const;
    
    /**
     * @brief Run dynamic quantile test
     * @param returns Actual returns
     * @param var_forecasts VaR forecasts
     * @param confidence_level Confidence level
     * @return Test result
     */
    [[nodiscard]] Result<BacktestResult> dynamic_quantile_test(const TimeSeries<double>& returns,
                                                              const TimeSeries<double>& var_forecasts,
                                                              double confidence_level = 0.05) const;
    
    /**
     * @brief Run comprehensive backtesting suite
     * @param returns Actual returns
     * @param var_forecasts VaR forecasts
     * @param confidence_level Confidence level
     * @return All test results
     */
    [[nodiscard]] Result<std::vector<BacktestResult>> run_comprehensive_tests(
        const TimeSeries<double>& returns,
        const TimeSeries<double>& var_forecasts,
        double confidence_level = 0.05) const;
    
    /**
     * @brief Calculate backtesting loss functions
     * @param returns Actual returns
     * @param var_forecasts VaR forecasts
     * @param confidence_level Confidence level
     * @return Loss function values
     */
    [[nodiscard]] Result<std::unordered_map<std::string, double>> calculate_loss_functions(
        const TimeSeries<double>& returns,
        const TimeSeries<double>& var_forecasts,
        double confidence_level = 0.05) const;
    
    /**
     * @brief Basel traffic light test
     * @param returns Actual returns
     * @param var_forecasts VaR forecasts
     * @param confidence_level Confidence level
     * @return Traffic light color and zone
     */
    [[nodiscard]] Result<std::string> traffic_light_test(const TimeSeries<double>& returns,
                                                        const TimeSeries<double>& var_forecasts,
                                                        double confidence_level = 0.05) const;

private:
    // Helper functions for violation analysis
    [[nodiscard]] std::vector<bool> identify_violations(const TimeSeries<double>& returns,
                                                       const TimeSeries<double>& var_forecasts) const;
    
    [[nodiscard]] double calculate_violation_rate(const std::vector<bool>& violations) const;
    
    [[nodiscard]] std::vector<int> calculate_violation_durations(const std::vector<bool>& violations) const;
    
    // Statistical test implementations
    [[nodiscard]] double chi_square_critical_value(double alpha, int df) const;
    [[nodiscard]] double chi_square_p_value(double test_stat, int df) const;
};

/**
 * @brief Extreme Value Theory implementation
 */
class ExtremeValueTheory {
public:
    /**
     * @brief Constructor
     */
    ExtremeValueTheory();
    
    /**
     * @brief Fit Peaks Over Threshold (POT) model
     * @param data Time series data
     * @param threshold_quantile Threshold as quantile (e.g., 0.95)
     * @return EVT parameters
     */
    [[nodiscard]] Result<EVTParameters> fit_pot_model(const TimeSeries<double>& data,
                                                     double threshold_quantile = 0.95);
    
    /**
     * @brief Fit Block Maxima model (GEV distribution)
     * @param data Time series data
     * @param block_size Block size for maxima extraction
     * @return EVT parameters
     */
    [[nodiscard]] Result<EVTParameters> fit_block_maxima(const TimeSeries<double>& data,
                                                        size_t block_size = 22);
    
    /**
     * @brief Calculate extreme quantiles using fitted EVT model
     * @param confidence_level Confidence level
     * @return Extreme quantile estimate
     */
    [[nodiscard]] Result<double> calculate_extreme_quantile(double confidence_level) const;
    
    /**
     * @brief Calculate Expected Shortfall using EVT
     * @param confidence_level Confidence level
     * @return EVT-based Expected Shortfall
     */
    [[nodiscard]] Result<double> calculate_evt_expected_shortfall(double confidence_level) const;
    
    /**
     * @brief Test goodness of fit for EVT model
     * @return Goodness of fit statistics
     */
    [[nodiscard]] Result<std::unordered_map<std::string, double>> test_goodness_of_fit() const;

private:
    EVTParameters parameters_;
    std::vector<double> exceedances_;
    std::vector<double> block_maxima_;
    bool is_fitted_;
    
    // Maximum likelihood estimation for GPD
    [[nodiscard]] Result<EVTParameters> estimate_gpd_mle(const std::vector<double>& exceedances);
    
    // Maximum likelihood estimation for GEV
    [[nodiscard]] Result<EVTParameters> estimate_gev_mle(const std::vector<double>& maxima);
    
    // Threshold selection methods
    [[nodiscard]] double select_optimal_threshold(const std::vector<double>& data) const;
    
    // Statistical tests
    [[nodiscard]] double anderson_darling_test(const std::vector<double>& data) const;
    [[nodiscard]] double kolmogorov_smirnov_test(const std::vector<double>& data) const;
};

/**
 * @brief Copula modeling for dependency analysis
 */
class CopulaModel {
public:
    /**
     * @brief Constructor
     * @param type Copula type
     */
    CopulaModel(CopulaType type = CopulaType::Gaussian);
    
    /**
     * @brief Fit copula to data
     * @param data Multivariate data (already transformed to uniform margins)
     * @return Copula parameters
     */
    [[nodiscard]] Result<std::vector<double>> fit(const std::vector<std::vector<double>>& data);
    
    /**
     * @brief Generate random samples from fitted copula
     * @param n_samples Number of samples
     * @return Random samples
     */
    [[nodiscard]] Result<std::vector<std::vector<double>>> sample(size_t n_samples = 1000) const;
    
    /**
     * @brief Calculate copula-based VaR for portfolio
     * @param marginal_vars Marginal VaR estimates
     * @param confidence_level Confidence level
     * @return Portfolio VaR
     */
    [[nodiscard]] Result<double> calculate_portfolio_var(const std::vector<double>& marginal_vars,
                                                        double confidence_level = 0.05) const;
    
    /**
     * @brief Calculate tail dependence coefficients
     * @return Upper and lower tail dependence
     */
    [[nodiscard]] Result<std::pair<double, double>> calculate_tail_dependence() const;

private:
    CopulaType copula_type_;
    std::vector<double> parameters_;
    size_t dimension_;
    bool is_fitted_;
    mutable std::mt19937 rng_;
    
    // Copula-specific implementations
    [[nodiscard]] Result<std::vector<double>> fit_gaussian_copula(
        const std::vector<std::vector<double>>& data);
    
    [[nodiscard]] Result<std::vector<double>> fit_t_copula(
        const std::vector<std::vector<double>>& data);
    
    [[nodiscard]] Result<std::vector<double>> fit_archimedean_copula(
        const std::vector<std::vector<double>>& data);
    
    // Transformation to uniform margins
    [[nodiscard]] std::vector<std::vector<double>> transform_to_uniform(
        const std::vector<std::vector<double>>& data) const;
};

/**
 * @brief Stress testing and scenario analysis
 */
class StressTester {
public:
    /**
     * @brief Constructor
     */
    StressTester();
    
    /**
     * @brief Run historical stress test
     * @param returns Current portfolio returns
     * @param stress_period Historical stress period
     * @return Stress test results
     */
    [[nodiscard]] Result<std::unordered_map<std::string, double>> historical_stress_test(
        const TimeSeries<double>& returns,
        const TimeSeries<double>& stress_period) const;
    
    /**
     * @brief Run Monte Carlo stress test
     * @param returns Portfolio returns
     * @param shock_scenarios Scenario definitions
     * @param n_simulations Number of simulations
     * @return Stress test results
     */
    [[nodiscard]] Result<std::vector<double>> monte_carlo_stress_test(
        const TimeSeries<double>& returns,
        const std::vector<std::unordered_map<std::string, double>>& shock_scenarios,
        size_t n_simulations = 10000) const;
    
    /**
     * @brief Calculate reverse stress test
     * @param returns Portfolio returns
     * @param target_loss Target loss level
     * @return Required scenario to achieve target loss
     */
    [[nodiscard]] Result<std::unordered_map<std::string, double>> reverse_stress_test(
        const TimeSeries<double>& returns,
        double target_loss) const;

private:
    mutable std::mt19937 rng_;
    
    // Scenario generation methods
    [[nodiscard]] std::vector<std::vector<double>> generate_correlated_shocks(
        const std::vector<std::vector<double>>& correlation_matrix,
        size_t n_scenarios) const;
};

} // namespace pyfolio::risk