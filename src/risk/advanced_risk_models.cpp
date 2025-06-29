#include "pyfolio/risk/advanced_risk_models.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <chrono>

namespace pyfolio::risk {

// ============================================================================
// GARCHModel Implementation
// ============================================================================

GARCHModel::GARCHModel(GARCHType type, int p, int q)
    : model_type_(type), p_order_(p), q_order_(q), distribution_("normal"), is_fitted_(false) {
    
    if (p < 0 || q < 0) {
        throw std::invalid_argument("GARCH orders must be non-negative");
    }
    if (p > 5 || q > 5) {
        throw std::invalid_argument("GARCH orders should not exceed 5 for numerical stability");
    }
}

Result<GARCHParameters> GARCHModel::fit(const TimeSeries<double>& returns, const std::string& distribution) {
    try {
        if (returns.size() < 100) {
            return Result<GARCHParameters>::error(ErrorCode::InvalidInput, 
                "Insufficient data for GARCH estimation (minimum 100 observations)");
        }
        
        distribution_ = distribution;
        returns_ = returns.values();
        
        // Remove mean (demean the series)
        double mean_return = std::accumulate(returns_.begin(), returns_.end(), 0.0) / returns_.size();
        for (auto& ret : returns_) {
            ret -= mean_return;
        }
        
        // Maximum likelihood estimation
        auto mle_result = estimate_mle(returns_);
        if (mle_result.is_error()) {
            return mle_result;
        }
        
        parameters_ = mle_result.value();
        
        // Filter volatility series
        filter_volatility(returns_, parameters_);
        
        // Calculate standardized residuals
        residuals_.resize(returns_.size());
        for (size_t i = 0; i < returns_.size(); ++i) {
            residuals_[i] = returns_[i] / volatility_[i];
        }
        
        // Calculate information criteria
        int n_params = 1 + p_order_ + q_order_; // omega + alpha + beta
        if (model_type_ == GARCHType::EGARCH || model_type_ == GARCHType::GJR_GARCH) {
            n_params += p_order_; // gamma parameters
        }
        
        parameters_.aic = -2.0 * parameters_.log_likelihood + 2.0 * n_params;
        parameters_.bic = -2.0 * parameters_.log_likelihood + n_params * std::log(returns_.size());
        
        is_fitted_ = true;
        
        return Result<GARCHParameters>::success(parameters_);
        
    } catch (const std::exception& e) {
        return Result<GARCHParameters>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<std::vector<double>> GARCHModel::forecast_volatility(int steps) const {
    if (!is_fitted_) {
        return Result<std::vector<double>>::error(ErrorCode::InvalidState, 
            "Model must be fitted before forecasting");
    }
    
    try {
        std::vector<double> forecasts(steps);
        
        // Get last volatility and shock
        double last_vol = volatility_.back();
        double last_shock = std::pow(returns_.back(), 2);
        
        // Simple persistence forecast for GARCH(1,1)
        if (p_order_ == 1 && q_order_ == 1) {
            double alpha = parameters_.alpha[0];
            double beta = parameters_.beta[0];
            double omega = parameters_.omega;
            double persistence = alpha + beta;
            
            // Long-run variance
            double long_run_var = omega / (1.0 - persistence);
            
            // Multi-step ahead forecasts
            for (int h = 0; h < steps; ++h) {
                if (h == 0) {
                    forecasts[h] = std::sqrt(omega + alpha * last_shock + beta * last_vol * last_vol);
                } else {
                    double variance_forecast = long_run_var + std::pow(persistence, h) * 
                                             (last_vol * last_vol - long_run_var);
                    forecasts[h] = std::sqrt(variance_forecast);
                }
            }
        } else {
            // For higher-order models, use recursive forecasting
            forecasts[0] = last_vol; // Placeholder - would need more complex recursion
        }
        
        return Result<std::vector<double>>::success(forecasts);
        
    } catch (const std::exception& e) {
        return Result<std::vector<double>>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<std::vector<double>> GARCHModel::get_residuals() const {
    if (!is_fitted_) {
        return Result<std::vector<double>>::error(ErrorCode::InvalidState, 
            "Model must be fitted first");
    }
    return Result<std::vector<double>>::success(residuals_);
}

Result<std::vector<double>> GARCHModel::get_conditional_volatility() const {
    if (!is_fitted_) {
        return Result<std::vector<double>>::error(ErrorCode::InvalidState, 
            "Model must be fitted first");
    }
    return Result<std::vector<double>>::success(volatility_);
}

Result<GARCHParameters> GARCHModel::estimate_mle(const std::vector<double>& returns) {
    try {
        // Initialize parameters
        GARCHParameters params;
        params.alpha.resize(p_order_);
        params.beta.resize(q_order_);
        
        // Initial parameter estimates
        double sample_var = 0.0;
        for (double ret : returns) {
            sample_var += ret * ret;
        }
        sample_var /= returns.size();
        
        // GARCH(1,1) initialization
        params.omega = 0.01 * sample_var;
        if (p_order_ > 0) params.alpha[0] = 0.05;
        if (q_order_ > 0) params.beta[0] = 0.90;
        
        // For asymmetric models
        if (model_type_ == GARCHType::EGARCH || model_type_ == GARCHType::GJR_GARCH) {
            params.gamma.resize(p_order_, 0.0);
        }
        
        // Simplified MLE (in practice, would use numerical optimization)
        // This is a placeholder implementation
        double log_likelihood = calculate_log_likelihood({params.omega, 
                                                         params.alpha[0], 
                                                         params.beta[0]}, returns);
        
        params.log_likelihood = log_likelihood;
        
        return Result<GARCHParameters>::success(params);
        
    } catch (const std::exception& e) {
        return Result<GARCHParameters>::error(ErrorCode::CalculationError, e.what());
    }
}

double GARCHModel::calculate_log_likelihood(const std::vector<double>& params,
                                           const std::vector<double>& returns) const {
    try {
        double omega = params[0];
        double alpha = params.size() > 1 ? params[1] : 0.0;
        double beta = params.size() > 2 ? params[2] : 0.0;
        
        // Ensure parameters are positive
        if (omega <= 0 || alpha < 0 || beta < 0 || alpha + beta >= 1.0) {
            return -1e10; // Very low likelihood for invalid parameters
        }
        
        double log_likelihood = 0.0;
        double h_t = omega / (1.0 - alpha - beta); // Unconditional variance
        
        for (size_t t = 1; t < returns.size(); ++t) {
            // Update conditional variance
            h_t = omega + alpha * returns[t-1] * returns[t-1] + beta * h_t;
            
            if (h_t <= 0) {
                return -1e10;
            }
            
            // Log-likelihood contribution (normal distribution)
            log_likelihood += -0.5 * (std::log(2.0 * M_PI) + std::log(h_t) + 
                                     returns[t] * returns[t] / h_t);
        }
        
        return log_likelihood;
        
    } catch (const std::exception&) {
        return -1e10;
    }
}

void GARCHModel::filter_volatility(const std::vector<double>& returns, 
                                  const GARCHParameters& params) {
    volatility_.resize(returns.size());
    
    // Initialize with unconditional variance
    double h_0 = params.omega;
    if (!params.alpha.empty() && !params.beta.empty()) {
        double persistence = params.alpha[0] + params.beta[0];
        if (persistence < 1.0) {
            h_0 = params.omega / (1.0 - persistence);
        }
    }
    
    volatility_[0] = std::sqrt(h_0);
    
    // Filter volatility
    for (size_t t = 1; t < returns.size(); ++t) {
        double h_t = params.omega;
        
        // ARCH terms
        for (int i = 0; i < p_order_ && t > i; ++i) {
            h_t += params.alpha[i] * returns[t-1-i] * returns[t-1-i];
        }
        
        // GARCH terms
        for (int j = 0; j < q_order_ && t > j; ++j) {
            h_t += params.beta[j] * volatility_[t-1-j] * volatility_[t-1-j];
        }
        
        // Asymmetric effects for EGARCH/GJR-GARCH
        if (model_type_ == GARCHType::GJR_GARCH) {
            for (int i = 0; i < p_order_ && t > i; ++i) {
                if (returns[t-1-i] < 0) {
                    h_t += params.gamma[i] * returns[t-1-i] * returns[t-1-i];
                }
            }
        }
        
        volatility_[t] = std::sqrt(std::max(h_t, 1e-8)); // Ensure positive volatility
    }
}

// ============================================================================
// VaRCalculator Implementation
// ============================================================================

VaRCalculator::VaRCalculator() : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {
    garch_model_ = std::make_unique<GARCHModel>();
}

Result<VaRResult> VaRCalculator::calculate_var(const TimeSeries<double>& returns,
                                              double confidence_level,
                                              VaRMethod method,
                                              size_t window_size) {
    try {
        if (returns.size() < 30) {
            return Result<VaRResult>::error(ErrorCode::InvalidInput, 
                "Insufficient data for VaR calculation");
        }
        
        VaRResult result;
        result.confidence_level = confidence_level;
        result.method = method;
        
        std::vector<double> return_values = returns.values();
        
        // Use rolling window if specified
        if (window_size > 0 && window_size < return_values.size()) {
            return_values = std::vector<double>(return_values.end() - window_size, return_values.end());
        }
        
        Result<double> var_result = Result<double>::error(ErrorCode::UnknownError, "Uninitialized");
        
        switch (method) {
            case VaRMethod::HistoricalSimulation:
                var_result = historical_simulation_var(return_values, confidence_level);
                break;
            case VaRMethod::Parametric:
                var_result = parametric_var(return_values, confidence_level);
                break;
            case VaRMethod::MonteCarlo:
                var_result = monte_carlo_var(return_values, confidence_level);
                break;
            case VaRMethod::FilteredHistorical:
                var_result = filtered_historical_var(return_values, confidence_level);
                break;
            case VaRMethod::ExtremeValueTheory:
                var_result = evt_var(return_values, confidence_level);
                break;
            case VaRMethod::CornishFisher:
                var_result = cornish_fisher_var(return_values, confidence_level);
                break;
            default:
                return Result<VaRResult>::error(ErrorCode::InvalidInput, "Unknown VaR method");
        }
        
        if (var_result.is_error()) {
            return Result<VaRResult>::error(var_result.error().code, var_result.error().message);
        }
        
        result.var_estimate = var_result.value();
        
        // Calculate Expected Shortfall
        auto es_result = calculate_expected_shortfall(returns, confidence_level, method);
        if (es_result.is_ok()) {
            result.expected_shortfall = es_result.value();
        }
        
        // Calculate empirical coverage
        int violations = 0;
        for (double ret : return_values) {
            if (ret < -result.var_estimate) {
                violations++;
            }
        }
        result.coverage_probability = 1.0 - static_cast<double>(violations) / return_values.size();
        
        // Maximum loss
        result.maximum_loss = -*std::min_element(return_values.begin(), return_values.end());
        
        return Result<VaRResult>::success(result);
        
    } catch (const std::exception& e) {
        return Result<VaRResult>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<double> VaRCalculator::historical_simulation_var(const std::vector<double>& returns,
                                                       double confidence_level) const {
    try {
        std::vector<double> sorted_returns = returns;
        std::sort(sorted_returns.begin(), sorted_returns.end());
        
        size_t index = static_cast<size_t>(confidence_level * sorted_returns.size());
        if (index >= sorted_returns.size()) {
            index = sorted_returns.size() - 1;
        }
        
        double var_estimate = -sorted_returns[index];
        return Result<double>::success(var_estimate);
        
    } catch (const std::exception& e) {
        return Result<double>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<double> VaRCalculator::parametric_var(const std::vector<double>& returns,
                                            double confidence_level) const {
    try {
        // Calculate sample mean and standard deviation
        double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        
        double variance = 0.0;
        for (double ret : returns) {
            variance += (ret - mean) * (ret - mean);
        }
        variance /= (returns.size() - 1);
        double std_dev = std::sqrt(variance);
        
        // Normal quantile (approximation)
        double z_score = 0.0;
        if (confidence_level == 0.01) {
            z_score = -2.33;
        } else if (confidence_level == 0.05) {
            z_score = -1.645;
        } else {
            // Simple approximation for other confidence levels
            z_score = -std::sqrt(-2.0 * std::log(confidence_level));
        }
        
        double var_estimate = -(mean + z_score * std_dev);
        return Result<double>::success(var_estimate);
        
    } catch (const std::exception& e) {
        return Result<double>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<double> VaRCalculator::monte_carlo_var(const std::vector<double>& returns,
                                             double confidence_level,
                                             size_t n_simulations) const {
    try {
        // Estimate parameters
        double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        
        double variance = 0.0;
        for (double ret : returns) {
            variance += (ret - mean) * (ret - mean);
        }
        variance /= (returns.size() - 1);
        double std_dev = std::sqrt(variance);
        
        // Generate random samples
        std::normal_distribution<double> dist(mean, std_dev);
        std::vector<double> simulated_returns(n_simulations);
        
        for (size_t i = 0; i < n_simulations; ++i) {
            simulated_returns[i] = dist(rng_);
        }
        
        // Calculate VaR from simulated data
        return historical_simulation_var(simulated_returns, confidence_level);
        
    } catch (const std::exception& e) {
        return Result<double>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<double> VaRCalculator::calculate_expected_shortfall(const TimeSeries<double>& returns,
                                                          double confidence_level,
                                                          VaRMethod method) {
    try {
        std::vector<double> return_values = returns.values();
        std::sort(return_values.begin(), return_values.end());
        
        size_t cutoff_index = static_cast<size_t>(confidence_level * return_values.size());
        if (cutoff_index >= return_values.size()) {
            cutoff_index = return_values.size() - 1;
        }
        
        // Calculate average of tail losses
        double tail_sum = 0.0;
        for (size_t i = 0; i <= cutoff_index; ++i) {
            tail_sum += return_values[i];
        }
        
        double expected_shortfall = -tail_sum / (cutoff_index + 1);
        return Result<double>::success(expected_shortfall);
        
    } catch (const std::exception& e) {
        return Result<double>::error(ErrorCode::CalculationError, e.what());
    }
}

// ============================================================================
// VaRBacktester Implementation
// ============================================================================

VaRBacktester::VaRBacktester() {}

Result<BacktestResult> VaRBacktester::kupiec_test(const TimeSeries<double>& returns,
                                                  const TimeSeries<double>& var_forecasts,
                                                  double confidence_level) const {
    try {
        if (returns.size() != var_forecasts.size()) {
            return Result<BacktestResult>::error(ErrorCode::InvalidInput, 
                "Returns and VaR forecasts must have same length");
        }
        
        BacktestResult result;
        result.test_type = BacktestType::Kupiec;
        
        // Identify violations
        auto violations = identify_violations(returns, var_forecasts);
        size_t n_violations = std::count(violations.begin(), violations.end(), true);
        size_t n_total = violations.size();
        
        result.violations = n_violations;
        result.total_observations = n_total;
        result.violation_rate = static_cast<double>(n_violations) / n_total;
        result.expected_violations = confidence_level * n_total;
        
        // Kupiec POF test statistic
        if (n_violations == 0) {
            result.test_statistic = 0.0;
        } else {
            double p_empirical = static_cast<double>(n_violations) / n_total;
            result.test_statistic = 2.0 * (n_violations * std::log(p_empirical / confidence_level) +
                                          (n_total - n_violations) * std::log((1.0 - p_empirical) / (1.0 - confidence_level)));
        }
        
        // Critical value (chi-square with 1 df at 5% level)
        result.critical_value = 3.841;
        result.p_value = 1.0 - chi_square_p_value(result.test_statistic, 1);
        result.reject_null = result.test_statistic > result.critical_value;
        
        if (result.reject_null) {
            result.interpretation = "Reject null hypothesis - VaR model is inadequate";
        } else {
            result.interpretation = "Fail to reject null hypothesis - VaR model appears adequate";
        }
        
        return Result<BacktestResult>::success(result);
        
    } catch (const std::exception& e) {
        return Result<BacktestResult>::error(ErrorCode::CalculationError, e.what());
    }
}

std::vector<bool> VaRBacktester::identify_violations(const TimeSeries<double>& returns,
                                                    const TimeSeries<double>& var_forecasts) const {
    std::vector<bool> violations;
    auto return_values = returns.values();
    auto var_values = var_forecasts.values();
    
    for (size_t i = 0; i < return_values.size(); ++i) {
        violations.push_back(return_values[i] < -var_values[i]);
    }
    
    return violations;
}

double VaRBacktester::chi_square_p_value(double test_stat, int df) const {
    // Simplified p-value calculation (would use proper statistical library in practice)
    if (df == 1) {
        if (test_stat < 0.01) return 0.92;
        if (test_stat < 1.0) return 0.32;
        if (test_stat < 3.841) return 0.05;
        return 0.01;
    }
    return 0.05; // Placeholder
}

Result<std::string> VaRBacktester::traffic_light_test(const TimeSeries<double>& returns,
                                                     const TimeSeries<double>& var_forecasts,
                                                     double confidence_level) const {
    try {
        auto violations = identify_violations(returns, var_forecasts);
        size_t n_violations = std::count(violations.begin(), violations.end(), true);
        
        // Basel traffic light approach
        std::string result;
        if (n_violations <= 4) {
            result = "GREEN - Model is performing well";
        } else if (n_violations <= 9) {
            result = "YELLOW - Model requires attention";
        } else {
            result = "RED - Model is inadequate and requires immediate revision";
        }
        
        return Result<std::string>::success(result);
        
    } catch (const std::exception& e) {
        return Result<std::string>::error(ErrorCode::CalculationError, e.what());
    }
}

// ============================================================================
// ExtremeValueTheory Implementation
// ============================================================================

ExtremeValueTheory::ExtremeValueTheory() : is_fitted_(false) {}

Result<EVTParameters> ExtremeValueTheory::fit_pot_model(const TimeSeries<double>& data,
                                                       double threshold_quantile) {
    try {
        std::vector<double> values = data.values();
        std::sort(values.begin(), values.end());
        
        // Determine threshold
        size_t threshold_index = static_cast<size_t>(threshold_quantile * values.size());
        double threshold = values[threshold_index];
        
        // Extract exceedances
        exceedances_.clear();
        for (double val : values) {
            if (val > threshold) {
                exceedances_.push_back(val - threshold);
            }
        }
        
        if (exceedances_.size() < 10) {
            return Result<EVTParameters>::error(ErrorCode::InvalidInput, 
                "Too few exceedances for reliable EVT estimation");
        }
        
        // Fit GPD using method of moments (simplified)
        double mean_excess = std::accumulate(exceedances_.begin(), exceedances_.end(), 0.0) / exceedances_.size();
        
        double variance = 0.0;
        for (double exc : exceedances_) {
            variance += (exc - mean_excess) * (exc - mean_excess);
        }
        variance /= (exceedances_.size() - 1);
        
        // Method of moments estimators for GPD
        EVTParameters params;
        params.threshold = threshold;
        params.threshold_quantile = threshold_quantile;
        params.n_exceedances = exceedances_.size();
        params.xi = 0.5 * (mean_excess * mean_excess / variance - 1.0);
        params.sigma = 0.5 * mean_excess * (mean_excess * mean_excess / variance + 1.0);
        
        // Simple goodness of fit
        params.anderson_darling = anderson_darling_test(exceedances_);
        params.kolmogorov_smirnov = kolmogorov_smirnov_test(exceedances_);
        
        parameters_ = params;
        is_fitted_ = true;
        
        return Result<EVTParameters>::success(params);
        
    } catch (const std::exception& e) {
        return Result<EVTParameters>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<double> ExtremeValueTheory::calculate_extreme_quantile(double confidence_level) const {
    if (!is_fitted_) {
        return Result<double>::error(ErrorCode::InvalidState, "EVT model must be fitted first");
    }
    
    try {
        double n = exceedances_.size();
        double N = 1.0 / parameters_.threshold_quantile; // Total sample size approximation
        
        // Extreme quantile using GPD
        double q = 1.0 - confidence_level;
        double extreme_quantile;
        
        if (std::abs(parameters_.xi) < 1e-6) {
            // Exponential case
            extreme_quantile = parameters_.threshold + parameters_.sigma * std::log(N * q / n);
        } else {
            // General GPD case
            extreme_quantile = parameters_.threshold + 
                              (parameters_.sigma / parameters_.xi) * 
                              (std::pow(N * q / n, -parameters_.xi) - 1.0);
        }
        
        return Result<double>::success(extreme_quantile);
        
    } catch (const std::exception& e) {
        return Result<double>::error(ErrorCode::CalculationError, e.what());
    }
}

double ExtremeValueTheory::anderson_darling_test(const std::vector<double>& data) const {
    // Simplified Anderson-Darling test implementation
    // In practice, would use proper statistical libraries
    return 0.5; // Placeholder
}

double ExtremeValueTheory::kolmogorov_smirnov_test(const std::vector<double>& data) const {
    // Simplified KS test implementation
    return 0.1; // Placeholder
}

// Additional missing implementations for VaRCalculator

Result<double> VaRCalculator::filtered_historical_var(const std::vector<double>& returns,
                                                     double confidence_level) const {
    try {
        // Fit GARCH model first
        auto returns_copy = returns;
        for (auto& ret : returns_copy) {
            ret -= std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        }
        
        // Simple filtered approach - would use fitted GARCH volatility in practice
        return historical_simulation_var(returns, confidence_level);
        
    } catch (const std::exception& e) {
        return Result<double>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<double> VaRCalculator::evt_var(const std::vector<double>& returns,
                                     double confidence_level) const {
    try {
        // Create temporary TimeSeries for EVT
        std::vector<DateTime> dummy_dates;
        for (size_t i = 0; i < returns.size(); ++i) {
            dummy_dates.push_back(DateTime(2020, 1, 1));
        }
        TimeSeries<double> temp_ts(dummy_dates, returns);
        
        ExtremeValueTheory evt;
        auto evt_result = evt.fit_pot_model(temp_ts, 0.95);
        if (evt_result.is_error()) {
            return Result<double>::error(evt_result.error().code, evt_result.error().message);
        }
        
        auto quantile_result = evt.calculate_extreme_quantile(confidence_level);
        if (quantile_result.is_error()) {
            return quantile_result;
        }
        
        return Result<double>::success(-quantile_result.value()); // Convert to VaR (positive)
        
    } catch (const std::exception& e) {
        return Result<double>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<double> VaRCalculator::cornish_fisher_var(const std::vector<double>& returns,
                                                double confidence_level) const {
    try {
        // Calculate sample moments
        double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        
        double variance = 0.0;
        double skewness = 0.0;
        double kurtosis = 0.0;
        
        for (double ret : returns) {
            double dev = ret - mean;
            variance += dev * dev;
            skewness += dev * dev * dev;
            kurtosis += dev * dev * dev * dev;
        }
        
        variance /= (returns.size() - 1);
        double std_dev = std::sqrt(variance);
        
        if (variance > 0) {
            skewness = (skewness / returns.size()) / std::pow(std_dev, 3);
            kurtosis = (kurtosis / returns.size()) / std::pow(std_dev, 4) - 3.0; // Excess kurtosis
        }
        
        // Normal quantile (approximation)
        double z_score = 0.0;
        if (confidence_level == 0.01) {
            z_score = -2.33;
        } else if (confidence_level == 0.05) {
            z_score = -1.645;
        } else {
            z_score = -std::sqrt(-2.0 * std::log(confidence_level));
        }
        
        // Cornish-Fisher expansion
        double cf_adjustment = (z_score * z_score - 1.0) * skewness / 6.0 +
                              (z_score * z_score * z_score - 3.0 * z_score) * kurtosis / 24.0 -
                              (2.0 * z_score * z_score * z_score - 5.0 * z_score) * skewness * skewness / 36.0;
        
        double adjusted_quantile = z_score + cf_adjustment;
        double var_estimate = -(mean + adjusted_quantile * std_dev);
        
        return Result<double>::success(var_estimate);
        
    } catch (const std::exception& e) {
        return Result<double>::error(ErrorCode::CalculationError, e.what());
    }
}

// Additional missing implementations for ExtremeValueTheory

Result<EVTParameters> ExtremeValueTheory::fit_block_maxima(const TimeSeries<double>& data,
                                                          size_t block_size) {
    try {
        auto values = data.values();
        block_maxima_.clear();
        
        // Extract block maxima
        for (size_t i = 0; i < values.size(); i += block_size) {
            size_t end_idx = std::min(i + block_size, values.size());
            auto max_it = std::max_element(values.begin() + i, values.begin() + end_idx);
            if (max_it != values.begin() + end_idx) {
                block_maxima_.push_back(*max_it);
            }
        }
        
        if (block_maxima_.size() < 10) {
            return Result<EVTParameters>::error(ErrorCode::InvalidInput,
                "Too few block maxima for reliable GEV estimation");
        }
        
        // Simple method of moments for GEV (placeholder)
        double mean_maxima = std::accumulate(block_maxima_.begin(), block_maxima_.end(), 0.0) / block_maxima_.size();
        double variance = 0.0;
        for (double max_val : block_maxima_) {
            variance += (max_val - mean_maxima) * (max_val - mean_maxima);
        }
        variance /= (block_maxima_.size() - 1);
        
        EVTParameters params;
        params.mu = mean_maxima; // Location parameter
        params.sigma = std::sqrt(variance * 6.0 / (M_PI * M_PI)); // Scale parameter (approx)
        params.xi = 0.0; // Shape parameter (Gumbel distribution assumption)
        
        parameters_ = params;
        is_fitted_ = true;
        
        return Result<EVTParameters>::success(params);
        
    } catch (const std::exception& e) {
        return Result<EVTParameters>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<double> ExtremeValueTheory::calculate_evt_expected_shortfall(double confidence_level) const {
    if (!is_fitted_) {
        return Result<double>::error(ErrorCode::InvalidState, "EVT model must be fitted first");
    }
    
    try {
        // Calculate extreme quantile first
        auto quantile_result = calculate_extreme_quantile(confidence_level);
        if (quantile_result.is_error()) {
            return quantile_result;
        }
        
        double var_level = quantile_result.value();
        
        // Simple approximation for Expected Shortfall
        // In practice, would integrate the tail distribution
        double es_multiplier = 1.0 / (1.0 - confidence_level);
        if (std::abs(parameters_.xi) < 1e-6) {
            // Exponential case
            es_multiplier = 1.0 + parameters_.sigma / var_level;
        } else {
            // GPD case
            es_multiplier = (var_level + parameters_.sigma - parameters_.xi * parameters_.threshold) / 
                           (1.0 - parameters_.xi);
        }
        
        double expected_shortfall = var_level * es_multiplier;
        return Result<double>::success(expected_shortfall);
        
    } catch (const std::exception& e) {
        return Result<double>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<std::unordered_map<std::string, double>> ExtremeValueTheory::test_goodness_of_fit() const {
    if (!is_fitted_) {
        return Result<std::unordered_map<std::string, double>>::error(ErrorCode::InvalidState,
            "EVT model must be fitted first");
    }
    
    try {
        std::unordered_map<std::string, double> tests;
        
        tests["anderson_darling"] = parameters_.anderson_darling;
        tests["kolmogorov_smirnov"] = parameters_.kolmogorov_smirnov;
        tests["log_likelihood"] = parameters_.log_likelihood;
        
        // Additional simple goodness of fit measures
        if (!exceedances_.empty()) {
            double mean_excess = std::accumulate(exceedances_.begin(), exceedances_.end(), 0.0) / exceedances_.size();
            tests["mean_excess"] = mean_excess;
            tests["n_exceedances"] = static_cast<double>(exceedances_.size());
        }
        
        return Result<std::unordered_map<std::string, double>>::success(tests);
        
    } catch (const std::exception& e) {
        return Result<std::unordered_map<std::string, double>>::error(ErrorCode::CalculationError, e.what());
    }
}

} // namespace pyfolio::risk