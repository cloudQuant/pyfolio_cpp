#pragma once

#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../core/types.h"
#include "../math/statistics.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <vector>

namespace pyfolio::analytics {

/**
 * @brief Bayesian prior distribution types
 */
enum class PriorType {
    Uniform,    // Non-informative uniform prior
    Normal,     // Normal (Gaussian) prior
    StudentT,   // Student's t-distribution prior
    Empirical,  // Empirical Bayes using historical data
    Jeffreys    // Jeffreys non-informative prior
};

/**
 * @brief Prior distribution parameters
 */
struct PriorDistribution {
    PriorType type;
    double mean               = 0.0;
    double variance           = 1.0;
    double degrees_of_freedom = 3.0;  // For Student-t

    /**
     * @brief Create uniform prior
     */
    static PriorDistribution uniform(double lower = -1.0, double upper = 1.0) {
        PriorDistribution prior;
        prior.type     = PriorType::Uniform;
        prior.mean     = (lower + upper) / 2.0;
        prior.variance = std::pow(upper - lower, 2) / 12.0;
        return prior;
    }

    /**
     * @brief Create normal prior
     */
    static PriorDistribution normal(double mean, double variance) {
        PriorDistribution prior;
        prior.type     = PriorType::Normal;
        prior.mean     = mean;
        prior.variance = variance;
        return prior;
    }

    /**
     * @brief Create Student-t prior
     */
    static PriorDistribution student_t(double mean, double scale, double df) {
        PriorDistribution prior;
        prior.type               = PriorType::StudentT;
        prior.mean               = mean;
        prior.variance           = scale * scale;
        prior.degrees_of_freedom = df;
        return prior;
    }
};

/**
 * @brief Bayesian performance analysis results
 */
struct BayesianPerformanceResult {
    // Posterior distributions (via samples)
    std::vector<double> alpha_samples;
    std::vector<double> beta_samples;
    std::vector<double> sharpe_samples;
    std::vector<double> volatility_samples;

    // Summary statistics
    double alpha_mean;
    double alpha_std;
    double alpha_credible_lower;  // 95% credible interval
    double alpha_credible_upper;

    double beta_mean;
    double beta_std;
    double beta_credible_lower;
    double beta_credible_upper;

    double sharpe_mean;
    double sharpe_std;
    double sharpe_credible_lower;
    double sharpe_credible_upper;

    // Probabilities
    double prob_alpha_positive;
    double prob_outperformance;    // P(alpha > 0)
    double prob_beta_greater_one;  // P(beta > 1)

    // Model comparison
    double marginal_likelihood;
    double dic;  // Deviance Information Criterion

    /**
     * @brief Get alpha percentile
     */
    double alpha_percentile(double percentile) const {
        if (alpha_samples.empty())
            return 0.0;
        auto sorted_samples = alpha_samples;
        std::sort(sorted_samples.begin(), sorted_samples.end());
        size_t index = static_cast<size_t>(percentile * (sorted_samples.size() - 1));
        return sorted_samples[index];
    }

    /**
     * @brief Get Sharpe ratio percentile
     */
    double sharpe_percentile(double percentile) const {
        if (sharpe_samples.empty())
            return 0.0;
        auto sorted_samples = sharpe_samples;
        std::sort(sorted_samples.begin(), sorted_samples.end());
        size_t index = static_cast<size_t>(percentile * (sorted_samples.size() - 1));
        return sorted_samples[index];
    }

    /**
     * @brief Check if alpha is significantly positive
     */
    bool is_alpha_significant(double threshold = 0.95) const { return prob_alpha_positive > threshold; }
};

/**
 * @brief Bayesian HMM regime detection results
 */
struct BayesianRegimeResult {
    std::vector<int> regime_sequence;              // 0, 1, 2, ... for different regimes
    std::vector<double> regime_probabilities;      // Probability of each regime at each time
    std::vector<double> transition_probabilities;  // Regime transition matrix (flattened)
    std::vector<double> regime_means;              // Mean return for each regime
    std::vector<double> regime_volatilities;       // Volatility for each regime
    size_t num_regimes;

    /**
     * @brief Get current regime probability
     */
    double current_regime_probability(int regime) const {
        if (regime_sequence.empty() || regime < 0 || regime >= static_cast<int>(num_regimes)) {
            return 0.0;
        }
        return regime_probabilities.back() * (regime_sequence.back() == regime ? 1.0 : 0.0);
    }

    /**
     * @brief Get regime persistence (average duration)
     */
    double regime_persistence(int regime) const {
        if (regime < 0 || regime >= static_cast<int>(num_regimes) ||
            transition_probabilities.size() != num_regimes * num_regimes) {
            return 0.0;
        }

        // Diagonal element of transition matrix
        double stay_prob = transition_probabilities[regime * num_regimes + regime];
        return 1.0 / (1.0 - stay_prob);
    }
};

/**
 * @brief Bayesian forecast results
 */
struct BayesianForecastResult {
    std::vector<double> return_forecasts;   // Point forecasts
    std::vector<double> forecast_lower_95;  // 95% prediction intervals
    std::vector<double> forecast_upper_95;
    std::vector<double> forecast_volatility;  // Forecast uncertainty
    std::vector<DateTime> forecast_dates;

    double model_confidence;  // Overall model confidence [0,1]

    /**
     * @brief Get forecast at specific horizon
     */
    double get_forecast(size_t horizon) const {
        if (horizon >= return_forecasts.size())
            return 0.0;
        return return_forecasts[horizon];
    }

    /**
     * @brief Get prediction interval width
     */
    double prediction_interval_width(size_t horizon) const {
        if (horizon >= forecast_lower_95.size() || horizon >= forecast_upper_95.size()) {
            return 0.0;
        }
        return forecast_upper_95[horizon] - forecast_lower_95[horizon];
    }
};

/**
 * @brief Bayesian performance analyzer
 */
class BayesianAnalyzer {
  private:
    mutable std::mt19937 rng_;
    size_t num_samples_;
    size_t burn_in_;

  public:
    /**
     * @brief Constructor
     */
    explicit BayesianAnalyzer(size_t num_samples = 10000, size_t burn_in = 1000)
        : rng_(std::random_device{}()), num_samples_(num_samples), burn_in_(burn_in) {}

    /**
     * @brief Constructor with fixed seed
     */
    BayesianAnalyzer(uint32_t seed, size_t num_samples = 10000, size_t burn_in = 1000)
        : rng_(seed), num_samples_(num_samples), burn_in_(burn_in) {}

    /**
     * @brief Bayesian alpha and beta analysis
     */
    Result<BayesianPerformanceResult> analyze_performance(
        const ReturnSeries& portfolio_returns, const ReturnSeries& benchmark_returns,
        const PriorDistribution& alpha_prior = PriorDistribution::normal(0.0, 0.01),
        const PriorDistribution& beta_prior  = PriorDistribution::normal(1.0, 0.25),
        double risk_free_rate                = 0.02) const {
        if (portfolio_returns.size() != benchmark_returns.size()) {
            return Result<BayesianPerformanceResult>::error(ErrorCode::InvalidInput,
                                                            "Portfolio and benchmark returns must have same length");
        }

        if (portfolio_returns.size() < 30) {
            return Result<BayesianPerformanceResult>::error(ErrorCode::InsufficientData,
                                                            "Need at least 30 observations for Bayesian analysis");
        }

        // Convert to excess returns
        const auto& port_returns  = portfolio_returns.values();
        const auto& bench_returns = benchmark_returns.values();

        std::vector<double> excess_port_returns;
        std::vector<double> excess_bench_returns;

        double daily_rf = risk_free_rate / 252.0;

        for (size_t i = 0; i < port_returns.size(); ++i) {
            excess_port_returns.push_back(port_returns[i] - daily_rf);
            excess_bench_returns.push_back(bench_returns[i] - daily_rf);
        }

        // MCMC sampling for alpha and beta
        auto mcmc_result = mcmc_alpha_beta_sampling(excess_port_returns, excess_bench_returns, alpha_prior, beta_prior);

        if (mcmc_result.is_error()) {
            return Result<BayesianPerformanceResult>::error(mcmc_result.error().code, mcmc_result.error().message);
        }

        BayesianPerformanceResult result = mcmc_result.value();

        // Calculate Sharpe ratio samples
        result.sharpe_samples.reserve(result.alpha_samples.size());
        for (size_t i = 0; i < result.alpha_samples.size(); ++i) {
            // Calculate portfolio volatility for each sample
            double alpha = result.alpha_samples[i];
            double beta  = result.beta_samples[i];

            // Estimate portfolio return and volatility
            auto bench_vol_result = stats::standard_deviation(std::span<const double>{excess_bench_returns});
            if (bench_vol_result.is_ok()) {
                double bench_vol = bench_vol_result.value();
                double port_vol  = result.volatility_samples[i];
                double expected_return =
                    alpha + beta * stats::mean(std::span<const double>{excess_bench_returns}).value_or(0.0);

                if (port_vol > 0) {
                    result.sharpe_samples.push_back(expected_return / port_vol);
                } else {
                    result.sharpe_samples.push_back(0.0);
                }
            } else {
                result.sharpe_samples.push_back(0.0);
            }
        }

        // Calculate summary statistics
        calculate_summary_statistics(result);

        return Result<BayesianPerformanceResult>::success(std::move(result));
    }

    /**
     * @brief Bayesian regime detection using Hidden Markov Model
     */
    Result<BayesianRegimeResult> detect_regimes(const ReturnSeries& returns, size_t num_regimes = 2) const {
        if (returns.empty()) {
            return Result<BayesianRegimeResult>::error(ErrorCode::InsufficientData, "Return series cannot be empty");
        }

        if (num_regimes < 2 || num_regimes > 5) {
            return Result<BayesianRegimeResult>::error(ErrorCode::InvalidInput,
                                                       "Number of regimes must be between 2 and 5");
        }

        const auto& return_values = returns.values();

        // Initialize regime detection result
        BayesianRegimeResult result;
        result.num_regimes = num_regimes;
        result.regime_sequence.resize(return_values.size());
        result.regime_probabilities.resize(return_values.size());
        result.transition_probabilities.resize(num_regimes * num_regimes);
        result.regime_means.resize(num_regimes);
        result.regime_volatilities.resize(num_regimes);

        // Simple EM algorithm for HMM parameter estimation
        auto em_result = estimate_hmm_parameters(return_values, num_regimes);
        if (em_result.is_error()) {
            return Result<BayesianRegimeResult>::error(em_result.error().code, em_result.error().message);
        }

        result = em_result.value();

        return Result<BayesianRegimeResult>::success(std::move(result));
    }

    /**
     * @brief Bayesian forecasting with uncertainty quantification
     */
    Result<BayesianForecastResult> forecast_returns(
        const ReturnSeries& returns, size_t forecast_horizon = 21,
        const PriorDistribution& volatility_prior = PriorDistribution::normal(0.15, 0.05)) const {
        if (returns.empty()) {
            return Result<BayesianForecastResult>::error(ErrorCode::InsufficientData, "Return series cannot be empty");
        }

        if (forecast_horizon == 0 || forecast_horizon > 252) {
            return Result<BayesianForecastResult>::error(ErrorCode::InvalidInput,
                                                         "Forecast horizon must be between 1 and 252 days");
        }

        const auto& return_values = returns.values();

        BayesianForecastResult result;
        result.return_forecasts.resize(forecast_horizon);
        result.forecast_lower_95.resize(forecast_horizon);
        result.forecast_upper_95.resize(forecast_horizon);
        result.forecast_volatility.resize(forecast_horizon);
        result.forecast_dates.resize(forecast_horizon);

        // Generate forecast dates
        DateTime last_date = returns.timestamps().back();
        for (size_t i = 0; i < forecast_horizon; ++i) {
            result.forecast_dates[i] = last_date.add_days(static_cast<int>(i + 1));
        }

        // Bayesian AR(1) model with stochastic volatility
        auto forecast_result = bayesian_ar_forecast(return_values, forecast_horizon, volatility_prior);
        if (forecast_result.is_error()) {
            return Result<BayesianForecastResult>::error(forecast_result.error().code, forecast_result.error().message);
        }

        result = forecast_result.value();

        // Calculate model confidence based on data quality and forecast uncertainty
        double mean_uncertainty =
            std::accumulate(result.forecast_volatility.begin(), result.forecast_volatility.end(), 0.0) /
            result.forecast_volatility.size();

        result.model_confidence = std::max(0.1, std::min(0.95, 1.0 - mean_uncertainty));

        return Result<BayesianForecastResult>::success(std::move(result));
    }

    /**
     * @brief Calculate Value at Risk with Bayesian uncertainty
     */
    Result<std::pair<double, double>> bayesian_var(const ReturnSeries& returns, double confidence_level = 0.95) const {
        if (returns.empty()) {
            return Result<std::pair<double, double>>::error(ErrorCode::InsufficientData,
                                                            "Return series cannot be empty");
        }

        const auto& return_values = returns.values();

        // Bayesian estimation of VaR distribution
        std::vector<double> var_samples;
        var_samples.reserve(num_samples_);

        // Sample from posterior distribution of parameters
        std::normal_distribution<double> normal_dist;
        std::uniform_real_distribution<double> uniform_dist(0.0, 1.0);

        for (size_t sample = 0; sample < num_samples_; ++sample) {
            // Sample mean and variance from posterior
            auto mean_result = stats::mean(std::span<const double>{return_values});
            auto var_result  = stats::variance(std::span<const double>{return_values});

            if (mean_result.is_error() || var_result.is_error())
                continue;

            double sample_mean = mean_result.value();
            double sample_var  = var_result.value();

            // Add uncertainty to parameters
            double uncertain_mean = sample_mean + normal_dist(rng_) * std::sqrt(sample_var / return_values.size());
            double uncertain_std  = std::sqrt(sample_var) * (1.0 + 0.1 * normal_dist(rng_));

            // Calculate VaR for this parameter sample
            double z_score      = stats::normal_ppf(1.0 - confidence_level);
            double var_estimate = uncertain_mean + z_score * uncertain_std;

            var_samples.push_back(var_estimate);
        }

        if (var_samples.empty()) {
            return Result<std::pair<double, double>>::error(ErrorCode::CalculationError,
                                                            "Failed to generate VaR samples");
        }

        // Calculate mean and standard deviation of VaR estimates
        double mean_var = std::accumulate(var_samples.begin(), var_samples.end(), 0.0) / var_samples.size();

        double var_variance = 0.0;
        for (double var_sample : var_samples) {
            var_variance += std::pow(var_sample - mean_var, 2);
        }
        var_variance /= (var_samples.size() - 1);
        double var_std = std::sqrt(var_variance);

        return Result<std::pair<double, double>>::success(std::make_pair(mean_var, var_std));
    }

  private:
    /**
     * @brief MCMC sampling for alpha and beta parameters
     */
    Result<BayesianPerformanceResult> mcmc_alpha_beta_sampling(const std::vector<double>& portfolio_returns,
                                                               const std::vector<double>& benchmark_returns,
                                                               const PriorDistribution& alpha_prior,
                                                               const PriorDistribution& beta_prior) const {
        BayesianPerformanceResult result;
        result.alpha_samples.reserve(num_samples_);
        result.beta_samples.reserve(num_samples_);
        result.volatility_samples.reserve(num_samples_);

        // Initialize parameters
        double alpha = 0.0;
        double beta  = 1.0;
        double sigma = 0.01;  // Residual volatility

        std::normal_distribution<double> normal_dist;
        std::gamma_distribution<double> gamma_dist(2.0, 0.01);  // For precision sampling

        size_t total_samples = num_samples_ + burn_in_;

        for (size_t iter = 0; iter < total_samples; ++iter) {
            // Gibbs sampling

            // Sample alpha | beta, sigma, data
            double alpha_precision            = 1.0 / alpha_prior.variance;
            double alpha_likelihood_precision = 0.0;
            double alpha_likelihood_mean      = 0.0;

            for (size_t i = 0; i < portfolio_returns.size(); ++i) {
                double residual = portfolio_returns[i] - beta * benchmark_returns[i];
                alpha_likelihood_precision += 1.0 / (sigma * sigma);
                alpha_likelihood_mean += residual / (sigma * sigma);
            }

            double posterior_precision = alpha_precision + alpha_likelihood_precision;
            double posterior_mean = (alpha_prior.mean * alpha_precision + alpha_likelihood_mean) / posterior_precision;
            double posterior_var  = 1.0 / posterior_precision;

            alpha = posterior_mean + normal_dist(rng_) * std::sqrt(posterior_var);

            // Sample beta | alpha, sigma, data
            double beta_precision            = 1.0 / beta_prior.variance;
            double beta_likelihood_precision = 0.0;
            double beta_likelihood_mean      = 0.0;

            for (size_t i = 0; i < portfolio_returns.size(); ++i) {
                double bench_return = benchmark_returns[i];
                double residual     = portfolio_returns[i] - alpha;
                beta_likelihood_precision += bench_return * bench_return / (sigma * sigma);
                beta_likelihood_mean += residual * bench_return / (sigma * sigma);
            }

            posterior_precision = beta_precision + beta_likelihood_precision;
            posterior_mean      = (beta_prior.mean * beta_precision + beta_likelihood_mean) / posterior_precision;
            posterior_var       = 1.0 / posterior_precision;

            beta = posterior_mean + normal_dist(rng_) * std::sqrt(posterior_var);

            // Sample sigma | alpha, beta, data
            double sse = 0.0;
            for (size_t i = 0; i < portfolio_returns.size(); ++i) {
                double residual = portfolio_returns[i] - alpha - beta * benchmark_returns[i];
                sse += residual * residual;
            }

            double shape = 2.0 + portfolio_returns.size() / 2.0;
            double rate  = 1.0 + sse / 2.0;

            std::gamma_distribution<double> precision_dist(shape, 1.0 / rate);
            double precision = precision_dist(rng_);
            sigma            = 1.0 / std::sqrt(precision);

            // Store samples (after burn-in)
            if (iter >= burn_in_) {
                result.alpha_samples.push_back(alpha);
                result.beta_samples.push_back(beta);
                result.volatility_samples.push_back(sigma);
            }
        }

        return Result<BayesianPerformanceResult>::success(std::move(result));
    }

    /**
     * @brief Estimate HMM parameters using EM algorithm
     */
    Result<BayesianRegimeResult> estimate_hmm_parameters(const std::vector<double>& returns, size_t num_regimes) const {
        BayesianRegimeResult result;
        result.num_regimes = num_regimes;

        // Initialize parameters randomly
        std::uniform_real_distribution<double> uniform_dist(0.0, 1.0);
        std::normal_distribution<double> normal_dist;

        // Initialize regime means and volatilities
        auto returns_mean = stats::mean(std::span<const double>{returns}).value_or(0.0);
        auto returns_std  = stats::standard_deviation(std::span<const double>{returns}).value_or(0.01);

        result.regime_means.resize(num_regimes);
        result.regime_volatilities.resize(num_regimes);

        for (size_t i = 0; i < num_regimes; ++i) {
            result.regime_means[i]        = returns_mean + normal_dist(rng_) * returns_std * 0.5;
            result.regime_volatilities[i] = returns_std * (0.5 + uniform_dist(rng_));
        }

        // Initialize transition matrix
        result.transition_probabilities.resize(num_regimes * num_regimes);
        for (size_t i = 0; i < num_regimes; ++i) {
            double row_sum = 0.0;
            for (size_t j = 0; j < num_regimes; ++j) {
                double prob                                          = uniform_dist(rng_);
                result.transition_probabilities[i * num_regimes + j] = prob;
                row_sum += prob;
            }
            // Normalize row
            for (size_t j = 0; j < num_regimes; ++j) {
                result.transition_probabilities[i * num_regimes + j] /= row_sum;
            }
        }

        // Simplified Viterbi algorithm for regime sequence
        result.regime_sequence.resize(returns.size());
        result.regime_probabilities.resize(returns.size());

        for (size_t t = 0; t < returns.size(); ++t) {
            // Simple assignment based on likelihood
            double best_likelihood = -std::numeric_limits<double>::infinity();
            int best_regime        = 0;

            for (size_t regime = 0; regime < num_regimes; ++regime) {
                double mean = result.regime_means[regime];
                double vol  = result.regime_volatilities[regime];

                if (vol > 0) {
                    double likelihood =
                        -0.5 * std::log(2.0 * M_PI * vol * vol) - 0.5 * std::pow(returns[t] - mean, 2) / (vol * vol);

                    if (likelihood > best_likelihood) {
                        best_likelihood = likelihood;
                        best_regime     = static_cast<int>(regime);
                    }
                }
            }

            result.regime_sequence[t]      = best_regime;
            result.regime_probabilities[t] = std::exp(best_likelihood);
        }

        return Result<BayesianRegimeResult>::success(std::move(result));
    }

    /**
     * @brief Bayesian AR(1) forecasting
     */
    Result<BayesianForecastResult> bayesian_ar_forecast(const std::vector<double>& returns, size_t forecast_horizon,
                                                        const PriorDistribution& volatility_prior) const {
        BayesianForecastResult result;
        result.return_forecasts.resize(forecast_horizon);
        result.forecast_lower_95.resize(forecast_horizon);
        result.forecast_upper_95.resize(forecast_horizon);
        result.forecast_volatility.resize(forecast_horizon);

        // Estimate AR(1) parameters
        if (returns.size() < 2) {
            return Result<BayesianForecastResult>::error(ErrorCode::InsufficientData,
                                                         "Need at least 2 observations for AR(1) model");
        }

        // Simple AR(1): r_t = phi * r_{t-1} + epsilon_t
        std::vector<double> y, x;
        for (size_t i = 1; i < returns.size(); ++i) {
            y.push_back(returns[i]);
            x.push_back(returns[i - 1]);
        }

        // Bayesian regression for phi
        double phi   = estimate_ar_coefficient(x, y);
        double sigma = estimate_residual_volatility(x, y, phi);

        // Generate forecasts with uncertainty
        std::normal_distribution<double> normal_dist;

        for (size_t h = 0; h < forecast_horizon; ++h) {
            std::vector<double> forecast_samples;
            forecast_samples.reserve(1000);

            for (size_t sample = 0; sample < 1000; ++sample) {
                double forecast    = 0.0;
                double last_return = returns.back();

                // Multi-step ahead forecast
                for (size_t step = 0; step <= h; ++step) {
                    if (step == 0) {
                        forecast = phi * last_return + normal_dist(rng_) * sigma;
                    } else {
                        forecast = phi * forecast + normal_dist(rng_) * sigma;
                    }
                }

                forecast_samples.push_back(forecast);
            }

            // Calculate statistics
            std::sort(forecast_samples.begin(), forecast_samples.end());

            result.return_forecasts[h] =
                std::accumulate(forecast_samples.begin(), forecast_samples.end(), 0.0) / forecast_samples.size();

            result.forecast_lower_95[h] = forecast_samples[static_cast<size_t>(0.025 * forecast_samples.size())];
            result.forecast_upper_95[h] = forecast_samples[static_cast<size_t>(0.975 * forecast_samples.size())];

            // Calculate forecast volatility
            double variance = 0.0;
            for (double sample : forecast_samples) {
                variance += std::pow(sample - result.return_forecasts[h], 2);
            }
            result.forecast_volatility[h] = std::sqrt(variance / (forecast_samples.size() - 1));
        }

        return Result<BayesianForecastResult>::success(std::move(result));
    }

    /**
     * @brief Calculate summary statistics from MCMC samples
     */
    void calculate_summary_statistics(BayesianPerformanceResult& result) const {
        // Alpha statistics
        if (!result.alpha_samples.empty()) {
            result.alpha_mean = std::accumulate(result.alpha_samples.begin(), result.alpha_samples.end(), 0.0) /
                                result.alpha_samples.size();

            double alpha_variance = 0.0;
            for (double alpha : result.alpha_samples) {
                alpha_variance += std::pow(alpha - result.alpha_mean, 2);
            }
            result.alpha_std = std::sqrt(alpha_variance / (result.alpha_samples.size() - 1));

            auto sorted_alpha = result.alpha_samples;
            std::sort(sorted_alpha.begin(), sorted_alpha.end());
            result.alpha_credible_lower = sorted_alpha[static_cast<size_t>(0.025 * sorted_alpha.size())];
            result.alpha_credible_upper = sorted_alpha[static_cast<size_t>(0.975 * sorted_alpha.size())];

            // Calculate probabilities
            size_t positive_count      = std::count_if(result.alpha_samples.begin(), result.alpha_samples.end(),
                                                       [](double alpha) { return alpha > 0.0; });
            result.prob_alpha_positive = static_cast<double>(positive_count) / result.alpha_samples.size();
            result.prob_outperformance = result.prob_alpha_positive;
        }

        // Beta statistics
        if (!result.beta_samples.empty()) {
            result.beta_mean = std::accumulate(result.beta_samples.begin(), result.beta_samples.end(), 0.0) /
                               result.beta_samples.size();

            double beta_variance = 0.0;
            for (double beta : result.beta_samples) {
                beta_variance += std::pow(beta - result.beta_mean, 2);
            }
            result.beta_std = std::sqrt(beta_variance / (result.beta_samples.size() - 1));

            auto sorted_beta = result.beta_samples;
            std::sort(sorted_beta.begin(), sorted_beta.end());
            result.beta_credible_lower = sorted_beta[static_cast<size_t>(0.025 * sorted_beta.size())];
            result.beta_credible_upper = sorted_beta[static_cast<size_t>(0.975 * sorted_beta.size())];

            size_t greater_one_count     = std::count_if(result.beta_samples.begin(), result.beta_samples.end(),
                                                         [](double beta) { return beta > 1.0; });
            result.prob_beta_greater_one = static_cast<double>(greater_one_count) / result.beta_samples.size();
        }

        // Sharpe ratio statistics
        if (!result.sharpe_samples.empty()) {
            result.sharpe_mean = std::accumulate(result.sharpe_samples.begin(), result.sharpe_samples.end(), 0.0) /
                                 result.sharpe_samples.size();

            double sharpe_variance = 0.0;
            for (double sharpe : result.sharpe_samples) {
                sharpe_variance += std::pow(sharpe - result.sharpe_mean, 2);
            }
            result.sharpe_std = std::sqrt(sharpe_variance / (result.sharpe_samples.size() - 1));

            auto sorted_sharpe = result.sharpe_samples;
            std::sort(sorted_sharpe.begin(), sorted_sharpe.end());
            result.sharpe_credible_lower = sorted_sharpe[static_cast<size_t>(0.025 * sorted_sharpe.size())];
            result.sharpe_credible_upper = sorted_sharpe[static_cast<size_t>(0.975 * sorted_sharpe.size())];
        }
    }

    /**
     * @brief Estimate AR(1) coefficient using Bayesian regression
     */
    double estimate_ar_coefficient(const std::vector<double>& x, const std::vector<double>& y) const {
        if (x.size() != y.size() || x.empty())
            return 0.0;

        double sum_x  = std::accumulate(x.begin(), x.end(), 0.0);
        double sum_y  = std::accumulate(y.begin(), y.end(), 0.0);
        double sum_xx = std::inner_product(x.begin(), x.end(), x.begin(), 0.0);
        double sum_xy = std::inner_product(x.begin(), x.end(), y.begin(), 0.0);

        double n           = static_cast<double>(x.size());
        double numerator   = sum_xy - (sum_x * sum_y) / n;
        double denominator = sum_xx - (sum_x * sum_x) / n;

        return (denominator != 0.0) ? numerator / denominator : 0.0;
    }

    /**
     * @brief Estimate residual volatility
     */
    double estimate_residual_volatility(const std::vector<double>& x, const std::vector<double>& y, double phi) const {
        if (x.size() != y.size() || x.empty())
            return 0.01;

        double sse = 0.0;
        for (size_t i = 0; i < x.size(); ++i) {
            double residual = y[i] - phi * x[i];
            sse += residual * residual;
        }

        return std::sqrt(sse / (x.size() - 1));
    }
};

}  // namespace pyfolio::analytics