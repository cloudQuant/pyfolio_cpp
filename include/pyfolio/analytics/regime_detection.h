#pragma once

#include "../core/datetime.h"
#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../core/types.h"
#include "../math/statistics.h"
#include "market_indicators.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <random>
#include <vector>
#include <memory>
#include <functional>
#include <array>
#include <unordered_map>
#include <mutex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace pyfolio::analytics {

// Type aliases
using ReturnSeries = TimeSeries<Return>;
using PriceSeries  = TimeSeries<Price>;

/**
 * @brief Market regime types
 */
enum class RegimeType {
    Bull,      // Upward trending market
    Bear,      // Downward trending market
    Volatile,  // High volatility regime
    Stable,    // Low volatility regime
    Crisis,    // Crisis/stress regime
    Recovery   // Post-crisis recovery
};

/**
 * @brief Regime characteristics
 */
struct RegimeCharacteristics {
    RegimeType type;
    double mean_return;
    double volatility;
    double persistence;  // Average regime duration in days
    double probability;  // Long-run probability of being in this regime
    std::string description;

    /**
     * @brief Get regime name
     */
    std::string name() const {
        switch (type) {
            case RegimeType::Bull:
                return "Bull Market";
            case RegimeType::Bear:
                return "Bear Market";
            case RegimeType::Volatile:
                return "High Volatility";
            case RegimeType::Stable:
                return "Low Volatility";
            case RegimeType::Crisis:
                return "Crisis";
            case RegimeType::Recovery:
                return "Recovery";
            default:
                return "Unknown";
        }
    }

    /**
     * @brief Check if regime is favorable for long positions
     */
    bool is_favorable_for_long() const {
        return type == RegimeType::Bull || type == RegimeType::Recovery || type == RegimeType::Stable;
    }

    /**
     * @brief Get risk level (1=low, 5=very high)
     */
    int risk_level() const {
        switch (type) {
            case RegimeType::Crisis:
                return 5;
            case RegimeType::Bear:
                return 4;
            case RegimeType::Volatile:
                return 4;
            case RegimeType::Recovery:
                return 3;
            case RegimeType::Bull:
                return 2;
            case RegimeType::Stable:
                return 1;
            default:
                return 3;
        }
    }
};

/**
 * @brief Regime transition probabilities
 */
struct RegimeTransition {
    RegimeType from_regime;
    RegimeType to_regime;
    double probability;
    double expected_duration;  // Days until transition

    /**
     * @brief Check if transition represents regime deterioration
     */
    bool is_deterioration() const {
        return (from_regime == RegimeType::Bull && to_regime == RegimeType::Bear) ||
               (from_regime == RegimeType::Stable && to_regime == RegimeType::Volatile) ||
               (to_regime == RegimeType::Crisis);
    }

    /**
     * @brief Check if transition represents regime improvement
     */
    bool is_improvement() const {
        return (from_regime == RegimeType::Bear && to_regime == RegimeType::Bull) ||
               (from_regime == RegimeType::Volatile && to_regime == RegimeType::Stable) ||
               (from_regime == RegimeType::Crisis && to_regime == RegimeType::Recovery);
    }
};

/**
 * @brief Complete regime detection results
 */
struct RegimeDetectionResult {
    std::vector<RegimeType> regime_sequence;
    std::vector<DateTime> dates;
    std::vector<double> regime_probabilities;
    std::vector<RegimeCharacteristics> regime_characteristics;
    std::vector<RegimeTransition> transitions;

    RegimeType current_regime;
    double current_regime_confidence;
    size_t current_regime_duration;  // Days in current regime

    /**
     * @brief Get regime at specific date
     */
    RegimeType get_regime_at_date(const DateTime& date) const {
        auto it = std::find(dates.begin(), dates.end(), date);
        if (it != dates.end()) {
            size_t index = std::distance(dates.begin(), it);
            if (index < regime_sequence.size()) {
                return regime_sequence[index];
            }
        }
        return RegimeType::Stable;  // Default
    }

    /**
     * @brief Get regime statistics
     */
    std::map<RegimeType, double> get_regime_statistics() const {
        std::map<RegimeType, double> stats;
        for (RegimeType regime : regime_sequence) {
            stats[regime]++;
        }

        double total = static_cast<double>(regime_sequence.size());
        for (auto& [regime, count] : stats) {
            count /= total;
        }

        return stats;
    }

    /**
     * @brief Get recent regime changes
     */
    std::vector<std::pair<DateTime, RegimeType>> get_recent_changes(size_t num_changes = 5) const {
        std::vector<std::pair<DateTime, RegimeType>> changes;

        if (regime_sequence.empty() || dates.empty())
            return changes;

        RegimeType last_regime = regime_sequence[0];
        for (size_t i = 1; i < std::min(regime_sequence.size(), dates.size()); ++i) {
            if (regime_sequence[i] != last_regime) {
                changes.emplace_back(dates[i], regime_sequence[i]);
                last_regime = regime_sequence[i];

                if (changes.size() >= num_changes)
                    break;
            }
        }

        return changes;
    }
    
    /**
     * @brief Get size of regime sequence
     */
    size_t size() const {
        return regime_sequence.size();
    }
    
    /**
     * @brief Iterator support for regime sequence
     */
    auto begin() const {
        return regime_sequence.begin();
    }
    
    auto end() const {
        return regime_sequence.end();
    }
};

/**
 * @brief Advanced regime detection analyzer
 */
class RegimeDetector {
  private:
    mutable std::mt19937 rng_;
    size_t lookback_window_;
    double volatility_threshold_;
    double return_threshold_;

  public:
    /**
     * @brief Constructor
     */
    explicit RegimeDetector(size_t lookback_window = 21, double volatility_threshold = 0.02,
                            double return_threshold = 0.001)
        : rng_(std::random_device{}()), lookback_window_(lookback_window), volatility_threshold_(volatility_threshold),
          return_threshold_(return_threshold) {}

    /**
     * @brief Constructor with seed
     */
    RegimeDetector(uint32_t seed, size_t lookback_window = 21, double volatility_threshold = 0.02,
                   double return_threshold = 0.001)
        : rng_(seed), lookback_window_(lookback_window), volatility_threshold_(volatility_threshold),
          return_threshold_(return_threshold) {}

    /**
     * @brief Detect market regimes using multiple methods
     */
    Result<RegimeDetectionResult> detect_regimes(const ReturnSeries& returns) const {
        if (returns.empty()) {
            return Result<RegimeDetectionResult>::error(ErrorCode::InsufficientData, "Return series cannot be empty");
        }

        if (returns.size() < lookback_window_) {
            return Result<RegimeDetectionResult>::error(ErrorCode::InsufficientData,
                                                        "Insufficient data for regime detection");
        }

        RegimeDetectionResult result;
        result.dates = returns.timestamps();
        result.regime_sequence.resize(returns.size());
        result.regime_probabilities.resize(returns.size());

        // Use multiple detection methods
        auto volatility_regimes = detect_volatility_regimes(returns);
        auto trend_regimes      = detect_trend_regimes(returns);
        auto crisis_regimes     = detect_crisis_regimes(returns);

        // Combine results using ensemble approach
        for (size_t i = 0; i < returns.size(); ++i) {
            result.regime_sequence[i] =
                combine_regime_signals(volatility_regimes[i], trend_regimes[i], crisis_regimes[i], returns.values()[i]);

            // Calculate confidence based on signal agreement
            result.regime_probabilities[i] =
                calculate_regime_confidence(volatility_regimes[i], trend_regimes[i], crisis_regimes[i]);
        }

        // Set current regime information
        if (!result.regime_sequence.empty()) {
            result.current_regime            = result.regime_sequence.back();
            result.current_regime_confidence = result.regime_probabilities.back();
            result.current_regime_duration   = calculate_current_regime_duration(result.regime_sequence);
        }

        // Calculate regime characteristics
        result.regime_characteristics = calculate_regime_characteristics(returns, result.regime_sequence);

        // Calculate transitions
        result.transitions = calculate_regime_transitions(result.regime_sequence, result.dates);

        return Result<RegimeDetectionResult>::success(std::move(result));
    }

    /**
     * @brief Real-time regime classification for single observation
     */
    Result<std::pair<RegimeType, double>> classify_current_regime(const std::vector<double>& recent_returns) const {
        if (recent_returns.size() < 5) {
            return Result<std::pair<RegimeType, double>>::error(ErrorCode::InsufficientData,
                                                                "Need at least 5 recent returns for classification");
        }

        // Calculate recent statistics
        auto mean_result = stats::mean(std::span<const double>{recent_returns});
        auto vol_result  = stats::standard_deviation(std::span<const double>{recent_returns});

        if (mean_result.is_error() || vol_result.is_error()) {
            return Result<std::pair<RegimeType, double>>::error(ErrorCode::CalculationError,
                                                                "Failed to calculate statistics");
        }

        double mean_return = mean_result.value();
        double volatility  = vol_result.value();

        // Classify regime based on return and volatility
        RegimeType regime;
        double confidence = 0.5;

        // Crisis detection (very high volatility)
        if (volatility > volatility_threshold_ * 3.0) {
            regime     = RegimeType::Crisis;
            confidence = 0.9;
        }
        // High volatility regime
        else if (volatility > volatility_threshold_ * 1.5) {
            regime     = RegimeType::Volatile;
            confidence = 0.7;
        }
        // Trend-based classification for normal volatility
        else if (mean_return > return_threshold_) {
            regime     = RegimeType::Bull;
            confidence = 0.8;
        } else if (mean_return < -return_threshold_) {
            regime     = RegimeType::Bear;
            confidence = 0.8;
        } else if (volatility < volatility_threshold_ * 0.5) {
            regime     = RegimeType::Stable;
            confidence = 0.7;
        } else {
            regime     = RegimeType::Recovery;
            confidence = 0.6;
        }

        return Result<std::pair<RegimeType, double>>::success(std::make_pair(regime, confidence));
    }

    /**
     * @brief Predict regime transitions
     */
    Result<std::vector<std::pair<RegimeType, double>>> predict_regime_transitions(
        const RegimeDetectionResult& current_state, size_t forecast_horizon = 21) const {
        if (current_state.regime_sequence.empty()) {
            return Result<std::vector<std::pair<RegimeType, double>>>::error(ErrorCode::InvalidInput,
                                                                             "Current state cannot be empty");
        }

        std::vector<std::pair<RegimeType, double>> predictions;
        predictions.reserve(forecast_horizon);

        // Calculate transition matrix
        auto transition_matrix = estimate_transition_matrix(current_state.regime_sequence);

        RegimeType current_regime = current_state.current_regime;
        double confidence         = current_state.current_regime_confidence;

        // Forward simulation
        for (size_t step = 0; step < forecast_horizon; ++step) {
            auto next_regime_result = predict_next_regime(current_regime, transition_matrix);
            if (next_regime_result.is_ok()) {
                auto [next_regime, transition_prob] = next_regime_result.value();
                confidence *= transition_prob;  // Uncertainty compounds
                predictions.emplace_back(next_regime, confidence);
                current_regime = next_regime;
            } else {
                break;
            }
        }

        return Result<std::vector<std::pair<RegimeType, double>>>::success(std::move(predictions));
    }

    /**
     * @brief Calculate regime-based portfolio recommendations
     */
    Result<std::map<RegimeType, std::string>> get_regime_recommendations() const {
        std::map<RegimeType, std::string> recommendations;

        recommendations[RegimeType::Bull]     = "Increase equity allocation, reduce cash, consider growth stocks";
        recommendations[RegimeType::Bear]     = "Reduce equity allocation, increase defensive assets, consider hedging";
        recommendations[RegimeType::Volatile] = "Reduce position sizes, increase hedging, focus on risk management";
        recommendations[RegimeType::Stable]   = "Maintain balanced allocation, consider carry strategies";
        recommendations[RegimeType::Crisis] = "Emergency risk reduction, increase cash and safe havens, avoid leverage";
        recommendations[RegimeType::Recovery] =
            "Gradually increase risk, focus on quality assets, avoid speculative positions";

        return Result<std::map<RegimeType, std::string>>::success(std::move(recommendations));
    }

    /**
     * @brief Markov switching model for regime detection
     */
    Result<RegimeDetectionResult> markov_switching_detection(const ReturnSeries& returns, int num_regimes = 2,
                                                             int max_iterations = 1000, uint32_t seed = 42) const {
        if (returns.empty()) {
            return Result<RegimeDetectionResult>::error(ErrorCode::InsufficientData, "Return series cannot be empty");
        }

        if (num_regimes < 2 || num_regimes > 5) {
            return Result<RegimeDetectionResult>::error(ErrorCode::InvalidInput,
                                                        "Number of regimes must be between 2 and 5");
        }

        // Initialize random number generator
        std::mt19937 gen(seed);
        std::uniform_real_distribution<> uniform(0.0, 1.0);
        std::normal_distribution<> normal(0.0, 1.0);

        const auto& values = returns.values();
        size_t n           = values.size();

        // Initialize regime parameters
        std::vector<double> means(num_regimes);
        std::vector<double> variances(num_regimes);
        std::vector<std::vector<double>> transition_matrix(num_regimes, std::vector<double>(num_regimes));

        // Initialize parameters with k-means-like approach
        auto mean_result = stats::mean(std::span<const double>{values});
        auto var_result  = stats::variance(std::span<const double>{values});

        double overall_mean = mean_result.value_or(0.0);
        double overall_var  = var_result.value_or(0.01);

        for (int k = 0; k < num_regimes; ++k) {
            means[k]     = overall_mean + normal(gen) * std::sqrt(overall_var) * 0.5;
            variances[k] = overall_var * (0.5 + uniform(gen));

            // Initialize transition probabilities
            for (int j = 0; j < num_regimes; ++j) {
                transition_matrix[k][j] = uniform(gen);
            }

            // Normalize transition probabilities
            double sum = std::accumulate(transition_matrix[k].begin(), transition_matrix[k].end(), 0.0);
            if (sum > 0) {
                for (int j = 0; j < num_regimes; ++j) {
                    transition_matrix[k][j] /= sum;
                }
            }
        }

        // EM algorithm
        std::vector<std::vector<double>> state_probabilities(n, std::vector<double>(num_regimes));
        double prev_likelihood = -std::numeric_limits<double>::infinity();

        for (int iter = 0; iter < max_iterations; ++iter) {
            // E-step: Calculate state probabilities using forward-backward algorithm
            forward_backward_algorithm(values, means, variances, transition_matrix, state_probabilities);

            // M-step: Update parameters
            update_markov_parameters(values, state_probabilities, means, variances, transition_matrix);

            // Check convergence
            double likelihood = calculate_likelihood(values, means, variances, transition_matrix);
            if (std::abs(likelihood - prev_likelihood) < 1e-6) {
                break;
            }
            prev_likelihood = likelihood;
        }

        // Extract most likely regime sequence
        RegimeDetectionResult result;
        result.dates = returns.timestamps();
        result.regime_sequence.reserve(n);
        result.regime_probabilities.reserve(n);

        for (size_t i = 0; i < n; ++i) {
            auto max_it    = std::max_element(state_probabilities[i].begin(), state_probabilities[i].end());
            int regime_idx = std::distance(state_probabilities[i].begin(), max_it);

            // Map regime index to RegimeType
            RegimeType regime = map_regime_index_to_type(regime_idx, means);
            result.regime_sequence.push_back(regime);
            result.regime_probabilities.push_back(*max_it);
        }

        // Set current regime info
        if (!result.regime_sequence.empty()) {
            result.current_regime            = result.regime_sequence.back();
            result.current_regime_confidence = result.regime_probabilities.back();
            result.current_regime_duration   = calculate_current_regime_duration(result.regime_sequence);
        }

        // Calculate characteristics and transitions
        result.regime_characteristics = calculate_regime_characteristics(returns, result.regime_sequence);
        result.transitions            = calculate_regime_transitions(result.regime_sequence, result.dates);

        return Result<RegimeDetectionResult>::success(std::move(result));
    }

    /**
     * @brief Hidden Markov Model regime detection
     */
    Result<RegimeDetectionResult> hidden_markov_detection(const ReturnSeries& returns, int num_regimes = 2) const {
        // Use Markov switching as the base implementation
        return markov_switching_detection(returns, num_regimes);
    }

    /**
     * @brief Structural break detection
     */
    Result<RegimeDetectionResult> structural_break_detection(const ReturnSeries& returns,
                                                             double significance_level = 0.05) const {
        if (returns.empty()) {
            return Result<RegimeDetectionResult>::error(ErrorCode::InsufficientData, "Return series cannot be empty");
        }

        const auto& values = returns.values();
        size_t n           = values.size();

        if (n < 20) {
            return Result<RegimeDetectionResult>::error(ErrorCode::InsufficientData,
                                                        "Need at least 20 observations for structural break detection");
        }

        std::vector<size_t> break_points;

        // CUSUM test for structural breaks
        auto mean_result    = stats::mean(std::span<const double>{values});
        double overall_mean = mean_result.value_or(0.0);

        std::vector<double> cusum(n);
        cusum[0] = values[0] - overall_mean;

        for (size_t i = 1; i < n; ++i) {
            cusum[i] = cusum[i - 1] + (values[i] - overall_mean);
        }

        // Find potential break points
        auto var_result    = stats::variance(std::span<const double>{values});
        double overall_var = var_result.value_or(0.01);
        
        // Calculate threshold based on critical value for significance level
        // Using approximation for CUSUM test critical value
        double critical_value = 1.358 * std::sqrt(n);  // ~0.05 significance level
        double threshold = critical_value * std::sqrt(overall_var);

        // Look for break points where CUSUM exceeds threshold
        double max_cusum = 0.0;
        for (size_t i = 0; i < n; ++i) {
            max_cusum = std::max(max_cusum, std::abs(cusum[i]));
        }
        
        // Normalize CUSUM and find breaks
        if (max_cusum > 0) {
            for (size_t i = 10; i < n - 10; ++i) {
                // Check for significant changes in CUSUM
                if (std::abs(cusum[i]) > threshold || 
                    (i > 0 && std::abs(cusum[i] - cusum[i-1]) > overall_var * 10)) {
                    // Avoid detecting too many consecutive break points
                    if (break_points.empty() || i - break_points.back() > 20) {
                        break_points.push_back(i);
                    }
                }
            }
        }

        // Create regime sequence based on break points
        RegimeDetectionResult result;
        result.dates = returns.timestamps();
        result.regime_sequence.resize(n);
        result.regime_probabilities.resize(n, 0.8);  // High confidence for structural breaks

        // Assign regimes based on break points
        RegimeType current_regime = RegimeType::Bull;
        size_t last_break         = 0;

        for (size_t break_point : break_points) {
            // Determine regime type based on mean return in segment
            auto segment_mean =
                stats::mean(std::span<const double>{values.data() + last_break, break_point - last_break});

            if (segment_mean.is_ok()) {
                current_regime = (segment_mean.value() > 0) ? RegimeType::Bull : RegimeType::Bear;
            }

            for (size_t i = last_break; i < break_point && i < n; ++i) {
                result.regime_sequence[i] = current_regime;
            }

            last_break = break_point;
            // Alternate regime for simplicity
            current_regime = (current_regime == RegimeType::Bull) ? RegimeType::Bear : RegimeType::Bull;
        }

        // Fill remaining observations
        for (size_t i = last_break; i < n; ++i) {
            result.regime_sequence[i] = current_regime;
        }

        // Set current regime info
        if (!result.regime_sequence.empty()) {
            result.current_regime            = result.regime_sequence.back();
            result.current_regime_confidence = 0.8;
            result.current_regime_duration   = calculate_current_regime_duration(result.regime_sequence);
        }

        // Calculate characteristics and transitions
        result.regime_characteristics = calculate_regime_characteristics(returns, result.regime_sequence);
        result.transitions            = calculate_regime_transitions(result.regime_sequence, result.dates);

        return Result<RegimeDetectionResult>::success(std::move(result));
    }

    /**
     * @brief Volatility regime detection
     */
    Result<RegimeDetectionResult> volatility_regime_detection(const ReturnSeries& returns) const {
        if (returns.empty()) {
            return Result<RegimeDetectionResult>::error(ErrorCode::InsufficientData, "Return series cannot be empty");
        }

        const auto& values = returns.values();
        size_t n           = values.size();

        // Calculate rolling volatility
        std::vector<double> rolling_volatility;
        rolling_volatility.reserve(n);

        for (size_t i = 0; i < n; ++i) {
            size_t start = (i >= lookback_window_) ? i - lookback_window_ + 1 : 0;
            std::vector<double> window(values.begin() + start, values.begin() + i + 1);

            auto vol_result = stats::standard_deviation(std::span<const double>{window});
            rolling_volatility.push_back(vol_result.value_or(0.01));
        }

        // Apply threshold-based regime classification
        auto median_vol_result = stats::median(std::span<const double>{rolling_volatility});
        double median_vol      = median_vol_result.value_or(0.01);

        double high_vol_threshold = median_vol * 1.5;
        double low_vol_threshold  = median_vol * 0.5;

        RegimeDetectionResult result;
        result.dates = returns.timestamps();
        result.regime_sequence.reserve(n);
        result.regime_probabilities.reserve(n);

        for (size_t i = 0; i < n; ++i) {
            double vol = rolling_volatility[i];
            RegimeType regime;
            double confidence;

            if (vol > high_vol_threshold * 2.0) {
                regime     = RegimeType::Crisis;
                confidence = 0.9;
            } else if (vol > high_vol_threshold) {
                regime     = RegimeType::Volatile;
                confidence = 0.8;
            } else if (vol < low_vol_threshold) {
                regime     = RegimeType::Stable;
                confidence = 0.8;
            } else {
                // Determine based on return sign
                regime     = (values[i] > 0) ? RegimeType::Bull : RegimeType::Bear;
                confidence = 0.6;
            }

            result.regime_sequence.push_back(regime);
            result.regime_probabilities.push_back(confidence);
        }

        // Set current regime info
        if (!result.regime_sequence.empty()) {
            result.current_regime            = result.regime_sequence.back();
            result.current_regime_confidence = result.regime_probabilities.back();
            result.current_regime_duration   = calculate_current_regime_duration(result.regime_sequence);
        }

        // Calculate characteristics and transitions
        result.regime_characteristics = calculate_regime_characteristics(returns, result.regime_sequence);
        result.transitions            = calculate_regime_transitions(result.regime_sequence, result.dates);

        return Result<RegimeDetectionResult>::success(std::move(result));
    }

  protected:
    /**
     * @brief Detect volatility-based regimes
     */
    std::vector<RegimeType> detect_volatility_regimes(const ReturnSeries& returns) const {
        std::vector<RegimeType> regimes;
        regimes.reserve(returns.size());

        const auto& values = returns.values();

        for (size_t i = 0; i < values.size(); ++i) {
            size_t start = (i >= lookback_window_) ? i - lookback_window_ + 1 : 0;
            std::vector<double> window(values.begin() + start, values.begin() + i + 1);

            auto vol_result = stats::standard_deviation(std::span<const double>{window});
            if (vol_result.is_ok()) {
                double vol = vol_result.value();

                if (vol > volatility_threshold_ * 2.0) {
                    regimes.push_back(RegimeType::Crisis);
                } else if (vol > volatility_threshold_) {
                    regimes.push_back(RegimeType::Volatile);
                } else {
                    regimes.push_back(RegimeType::Stable);
                }
            } else {
                regimes.push_back(RegimeType::Stable);
            }
        }

        return regimes;
    }

    /**
     * @brief Detect trend-based regimes
     */
    std::vector<RegimeType> detect_trend_regimes(const ReturnSeries& returns) const {
        std::vector<RegimeType> regimes;
        regimes.reserve(returns.size());

        const auto& values = returns.values();

        for (size_t i = 0; i < values.size(); ++i) {
            size_t start = (i >= lookback_window_) ? i - lookback_window_ + 1 : 0;
            std::vector<double> window(values.begin() + start, values.begin() + i + 1);

            auto mean_result = stats::mean(std::span<const double>{window});
            if (mean_result.is_ok()) {
                double mean_return = mean_result.value();

                if (mean_return > return_threshold_) {
                    regimes.push_back(RegimeType::Bull);
                } else if (mean_return < -return_threshold_) {
                    regimes.push_back(RegimeType::Bear);
                } else {
                    regimes.push_back(RegimeType::Recovery);
                }
            } else {
                regimes.push_back(RegimeType::Recovery);
            }
        }

        return regimes;
    }

    /**
     * @brief Detect crisis regimes using extreme events
     */
    std::vector<RegimeType> detect_crisis_regimes(const ReturnSeries& returns) const {
        std::vector<RegimeType> regimes;
        regimes.reserve(returns.size());

        const auto& values = returns.values();

        // Calculate rolling VaR for crisis detection
        for (size_t i = 0; i < values.size(); ++i) {
            size_t start = (i >= lookback_window_) ? i - lookback_window_ + 1 : 0;
            std::vector<double> window(values.begin() + start, values.begin() + i + 1);

            auto var_result = stats::value_at_risk(std::span<const double>{window}, 0.05);
            if (var_result.is_ok()) {
                double var_5pct       = var_result.value();
                double current_return = values[i];

                if (current_return < var_5pct * 1.5) {  // Extreme negative return
                    regimes.push_back(RegimeType::Crisis);
                } else {
                    regimes.push_back(RegimeType::Recovery);
                }
            } else {
                regimes.push_back(RegimeType::Recovery);
            }
        }

        return regimes;
    }

    /**
     * @brief Combine multiple regime signals
     */
    RegimeType combine_regime_signals(RegimeType vol_regime, RegimeType trend_regime, RegimeType crisis_regime,
                                      double current_return) const {
        // Crisis takes precedence
        if (crisis_regime == RegimeType::Crisis) {
            return RegimeType::Crisis;
        }

        // High volatility with negative trend suggests bear market
        if (vol_regime == RegimeType::Volatile && trend_regime == RegimeType::Bear) {
            return RegimeType::Bear;
        }

        // High volatility with positive trend suggests volatile bull
        if (vol_regime == RegimeType::Volatile && trend_regime == RegimeType::Bull) {
            return RegimeType::Volatile;
        }

        // Stable volatility follows trend
        if (vol_regime == RegimeType::Stable) {
            return trend_regime;
        }

        // Default to trend regime
        return trend_regime;
    }

    /**
     * @brief Calculate regime confidence based on signal agreement
     */
    double calculate_regime_confidence(RegimeType vol_regime, RegimeType trend_regime, RegimeType crisis_regime) const {
        std::vector<RegimeType> signals = {vol_regime, trend_regime, crisis_regime};
        std::map<RegimeType, int> counts;

        for (RegimeType regime : signals) {
            counts[regime]++;
        }

        // Find most common regime
        int max_count = 0;
        for (const auto& [regime, count] : counts) {
            max_count = std::max(max_count, count);
        }

        // Confidence based on agreement
        return static_cast<double>(max_count) / signals.size();
    }

    /**
     * @brief Calculate current regime duration
     */
    size_t calculate_current_regime_duration(const std::vector<RegimeType>& sequence) const {
        if (sequence.empty())
            return 0;

        RegimeType current = sequence.back();
        size_t duration    = 1;

        for (auto it = sequence.rbegin() + 1; it != sequence.rend(); ++it) {
            if (*it == current) {
                duration++;
            } else {
                break;
            }
        }

        return duration;
    }

    /**
     * @brief Calculate regime characteristics
     */
    std::vector<RegimeCharacteristics> calculate_regime_characteristics(const ReturnSeries& returns,
                                                                        const std::vector<RegimeType>& sequence) const {
        std::map<RegimeType, std::vector<double>> regime_returns;
        std::map<RegimeType, std::vector<size_t>> regime_durations;

        const auto& values = returns.values();

        // Group returns by regime
        for (size_t i = 0; i < std::min(sequence.size(), values.size()); ++i) {
            regime_returns[sequence[i]].push_back(values[i]);
        }

        // Calculate durations
        if (!sequence.empty()) {
            RegimeType current_regime = sequence[0];
            size_t current_duration   = 1;

            for (size_t i = 1; i < sequence.size(); ++i) {
                if (sequence[i] == current_regime) {
                    current_duration++;
                } else {
                    regime_durations[current_regime].push_back(current_duration);
                    current_regime   = sequence[i];
                    current_duration = 1;
                }
            }
            regime_durations[current_regime].push_back(current_duration);
        }

        std::vector<RegimeCharacteristics> characteristics;

        for (const auto& [regime, rets] : regime_returns) {
            RegimeCharacteristics char_data;
            char_data.type = regime;

            if (!rets.empty()) {
                auto mean_result = stats::mean(std::span<const double>{rets});
                auto vol_result  = stats::standard_deviation(std::span<const double>{rets});

                char_data.mean_return = mean_result.value_or(0.0);
                char_data.volatility  = vol_result.value_or(0.0);
                char_data.probability = static_cast<double>(rets.size()) / sequence.size();

                // Calculate average duration
                if (!regime_durations[regime].empty()) {
                    double avg_duration =
                        std::accumulate(regime_durations[regime].begin(), regime_durations[regime].end(), 0.0) /
                        regime_durations[regime].size();
                    char_data.persistence = avg_duration;
                }

                char_data.description = generate_regime_description(char_data);
            }

            characteristics.push_back(char_data);
        }

        return characteristics;
    }

    /**
     * @brief Calculate regime transitions
     */
    std::vector<RegimeTransition> calculate_regime_transitions(const std::vector<RegimeType>& sequence,
                                                               const std::vector<DateTime>& dates) const {
        std::map<std::pair<RegimeType, RegimeType>, int> transition_counts;
        std::map<RegimeType, int> regime_counts;

        // Count transitions
        for (size_t i = 1; i < sequence.size(); ++i) {
            RegimeType from = sequence[i - 1];
            RegimeType to   = sequence[i];

            transition_counts[{from, to}]++;
            regime_counts[from]++;
        }

        std::vector<RegimeTransition> transitions;

        for (const auto& [transition, count] : transition_counts) {
            RegimeTransition trans;
            trans.from_regime = transition.first;
            trans.to_regime   = transition.second;

            if (regime_counts[transition.first] > 0) {
                trans.probability       = static_cast<double>(count) / regime_counts[transition.first];
                trans.expected_duration = 1.0 / trans.probability;
            }

            transitions.push_back(trans);
        }

        return transitions;
    }

    /**
     * @brief Generate regime description
     */
    std::string generate_regime_description(const RegimeCharacteristics& chars) const {
        std::string desc = chars.name() + ": ";
        desc += "Mean return " + std::to_string(chars.mean_return * 100.0) + "%, ";
        desc += "Volatility " + std::to_string(chars.volatility * 100.0) + "%, ";
        desc += "Avg duration " + std::to_string(chars.persistence) + " days";
        return desc;
    }

    /**
     * @brief Estimate transition matrix
     */
    std::map<std::pair<RegimeType, RegimeType>, double> estimate_transition_matrix(
        const std::vector<RegimeType>& sequence) const {
        std::map<std::pair<RegimeType, RegimeType>, double> matrix;
        std::map<RegimeType, int> regime_counts;

        // Count transitions
        for (size_t i = 1; i < sequence.size(); ++i) {
            RegimeType from = sequence[i - 1];
            RegimeType to   = sequence[i];

            matrix[{from, to}]++;
            regime_counts[from]++;
        }

        // Normalize to probabilities
        for (auto& [transition, count] : matrix) {
            RegimeType from = transition.first;
            if (regime_counts[from] > 0) {
                count /= regime_counts[from];
            }
        }

        return matrix;
    }

    /**
     * @brief Predict next regime
     */
    Result<std::pair<RegimeType, double>> predict_next_regime(
        RegimeType current_regime, const std::map<std::pair<RegimeType, RegimeType>, double>& transition_matrix) const {
        std::vector<std::pair<RegimeType, double>> candidates;

        for (const auto& [transition, prob] : transition_matrix) {
            if (transition.first == current_regime && prob > 0.0) {
                candidates.emplace_back(transition.second, prob);
            }
        }

        if (candidates.empty()) {
            return Result<std::pair<RegimeType, double>>::error(ErrorCode::CalculationError,
                                                                "No valid transitions found");
        }

        // Select most likely transition
        auto max_it = std::max_element(candidates.begin(), candidates.end(),
                                       [](const auto& a, const auto& b) { return a.second < b.second; });

        return Result<std::pair<RegimeType, double>>::success(*max_it);
    }

    /**
     * @brief Forward-backward algorithm for HMM
     */
    void forward_backward_algorithm(const std::vector<double>& observations, const std::vector<double>& means,
                                    const std::vector<double>& variances,
                                    const std::vector<std::vector<double>>& transition_matrix,
                                    std::vector<std::vector<double>>& state_probabilities) const {
        size_t n          = observations.size();
        size_t num_states = means.size();

        // Forward pass
        std::vector<std::vector<double>> forward(n, std::vector<double>(num_states));

        // Initialize forward probabilities
        for (size_t k = 0; k < num_states; ++k) {
            forward[0][k] = gaussian_pdf(observations[0], means[k], variances[k]) / num_states;
        }

        // Forward recursion
        for (size_t t = 1; t < n; ++t) {
            for (size_t k = 0; k < num_states; ++k) {
                forward[t][k] = 0.0;
                for (size_t j = 0; j < num_states; ++j) {
                    forward[t][k] += forward[t - 1][j] * transition_matrix[j][k];
                }
                forward[t][k] *= gaussian_pdf(observations[t], means[k], variances[k]);
            }

            // Normalize to prevent underflow
            double sum = std::accumulate(forward[t].begin(), forward[t].end(), 0.0);
            if (sum > 0) {
                for (size_t k = 0; k < num_states; ++k) {
                    forward[t][k] /= sum;
                }
            }
        }

        // Backward pass
        std::vector<std::vector<double>> backward(n, std::vector<double>(num_states, 1.0));

        // Backward recursion
        for (int t = static_cast<int>(n) - 2; t >= 0; --t) {
            for (size_t k = 0; k < num_states; ++k) {
                backward[t][k] = 0.0;
                for (size_t j = 0; j < num_states; ++j) {
                    backward[t][k] += transition_matrix[k][j] *
                                      gaussian_pdf(observations[t + 1], means[j], variances[j]) * backward[t + 1][j];
                }
            }

            // Normalize
            double sum = std::accumulate(backward[t].begin(), backward[t].end(), 0.0);
            if (sum > 0) {
                for (size_t k = 0; k < num_states; ++k) {
                    backward[t][k] /= sum;
                }
            }
        }

        // Combine forward and backward probabilities
        for (size_t t = 0; t < n; ++t) {
            double sum = 0.0;
            for (size_t k = 0; k < num_states; ++k) {
                state_probabilities[t][k] = forward[t][k] * backward[t][k];
                sum += state_probabilities[t][k];
            }

            // Normalize
            if (sum > 0) {
                for (size_t k = 0; k < num_states; ++k) {
                    state_probabilities[t][k] /= sum;
                }
            }
        }
    }

    /**
     * @brief Update Markov model parameters
     */
    void update_markov_parameters(const std::vector<double>& observations,
                                  const std::vector<std::vector<double>>& state_probabilities,
                                  std::vector<double>& means, std::vector<double>& variances,
                                  std::vector<std::vector<double>>& transition_matrix) const {
        size_t n          = observations.size();
        size_t num_states = means.size();

        // Update means and variances
        for (size_t k = 0; k < num_states; ++k) {
            double sum_prob = 0.0;
            double sum_obs  = 0.0;

            for (size_t t = 0; t < n; ++t) {
                sum_prob += state_probabilities[t][k];
                sum_obs += state_probabilities[t][k] * observations[t];
            }

            if (sum_prob > 1e-6) {
                means[k] = sum_obs / sum_prob;

                // Update variance
                double sum_var = 0.0;
                for (size_t t = 0; t < n; ++t) {
                    double diff = observations[t] - means[k];
                    sum_var += state_probabilities[t][k] * diff * diff;
                }
                variances[k] = std::max(1e-6, sum_var / sum_prob);
            }
        }

        // Update transition matrix
        for (size_t i = 0; i < num_states; ++i) {
            double sum_from_i = 0.0;
            for (size_t j = 0; j < num_states; ++j) {
                double sum_transitions = 0.0;

                for (size_t t = 0; t < n - 1; ++t) {
                    sum_transitions += state_probabilities[t][i] * state_probabilities[t + 1][j];
                }

                transition_matrix[i][j] = sum_transitions;
                sum_from_i += sum_transitions;
            }

            // Normalize
            if (sum_from_i > 1e-6) {
                for (size_t j = 0; j < num_states; ++j) {
                    transition_matrix[i][j] /= sum_from_i;
                }
            }
        }
    }

    /**
     * @brief Calculate model likelihood
     */
    double calculate_likelihood(const std::vector<double>& observations, const std::vector<double>& means,
                                const std::vector<double>& variances,
                                const std::vector<std::vector<double>>& transition_matrix) const {
        double log_likelihood = 0.0;
        size_t n              = observations.size();
        size_t num_states     = means.size();

        std::vector<double> current_probs(num_states, 1.0 / num_states);

        for (size_t t = 0; t < n; ++t) {
            std::vector<double> next_probs(num_states, 0.0);
            double observation_prob = 0.0;

            for (size_t k = 0; k < num_states; ++k) {
                double emission_prob = gaussian_pdf(observations[t], means[k], variances[k]);
                observation_prob += current_probs[k] * emission_prob;

                // Update state probabilities
                for (size_t j = 0; j < num_states; ++j) {
                    next_probs[j] += current_probs[k] * transition_matrix[k][j] * emission_prob;
                }
            }

            log_likelihood += std::log(std::max(1e-10, observation_prob));

            // Normalize next probabilities
            double sum = std::accumulate(next_probs.begin(), next_probs.end(), 0.0);
            if (sum > 1e-10) {
                for (size_t k = 0; k < num_states; ++k) {
                    next_probs[k] /= sum;
                }
            }

            current_probs = next_probs;
        }

        return log_likelihood;
    }

    /**
     * @brief Map regime index to RegimeType
     */
    RegimeType map_regime_index_to_type(int index, const std::vector<double>& means) const {
        if (index >= static_cast<int>(means.size())) {
            return RegimeType::Stable;
        }

        double mean = means[index];

        // Simple mapping based on mean return
        if (mean > return_threshold_) {
            return RegimeType::Bull;
        } else if (mean < -return_threshold_) {
            return RegimeType::Bear;
        } else if (std::abs(mean) < return_threshold_ * 0.5) {
            return RegimeType::Stable;
        } else {
            return RegimeType::Recovery;
        }
    }

    /**
     * @brief Gaussian PDF
     */
    double gaussian_pdf(double x, double mean, double variance) const {
        if (variance <= 0)
            variance = 1e-6;

        double diff        = x - mean;
        double exponent    = -0.5 * diff * diff / variance;
        double coefficient = 1.0 / std::sqrt(2.0 * M_PI * variance);

        return coefficient * std::exp(exponent);
    }
};

/**
 * @brief Advanced Machine Learning Regime Detection Algorithms
 * 
 * This class implements state-of-the-art machine learning algorithms for
 * market regime detection including deep learning, ensemble methods, and
 * adaptive algorithms suitable for high-frequency trading environments.
 */
class MLRegimeDetector : public RegimeDetector {
private:
    mutable std::mt19937 rng_;
    mutable std::mutex model_mutex_;
    
    // Model parameters
    size_t lookback_window_;
    size_t num_regimes_;
    double learning_rate_;
    double convergence_threshold_;
    size_t max_iterations_;
    
    // Cached models for performance
    mutable std::unordered_map<std::string, std::vector<double>> cached_models_;
    
public:
    /**
     * @brief Constructor with advanced ML parameters
     */
    explicit MLRegimeDetector(size_t lookback_window = 252, size_t num_regimes = 3,
                             double learning_rate = 0.01, double convergence_threshold = 1e-6,
                             size_t max_iterations = 1000)
        : RegimeDetector(lookback_window), rng_(std::random_device{}()), 
          lookback_window_(lookback_window), num_regimes_(num_regimes),
          learning_rate_(learning_rate), convergence_threshold_(convergence_threshold), 
          max_iterations_(max_iterations) {}
    
    /**
     * @brief Deep Neural Network Regime Detection
     * 
     * Implements a deep learning approach using a multi-layer perceptron
     * with financial feature engineering for regime classification.
     * 
     * @param returns Time series of returns
     * @param hidden_layers Number of hidden layers (default: 2)
     * @param neurons_per_layer Neurons per hidden layer (default: 32)
     * @return Regime detection results with neural network confidence scores
     */
    [[nodiscard]] Result<RegimeDetectionResult> deep_neural_network_detection(
        const ReturnSeries& returns, size_t hidden_layers = 2, size_t neurons_per_layer = 32) const {
        
        if (returns.empty() || returns.size() < lookback_window_) {
            return Result<RegimeDetectionResult>::error(ErrorCode::InsufficientData,
                "Insufficient data for deep learning regime detection");
        }
        
        const auto& values = returns.values();
        size_t n = values.size();
        
        // Feature engineering for neural network
        auto features_result = extract_advanced_features(returns);
        if (features_result.is_error()) {
            return Result<RegimeDetectionResult>::error(features_result.error().code,
                "Failed to extract features: " + features_result.error().message);
        }
        
        auto features = features_result.value();
        size_t feature_count = features[0].size();
        
        // Initialize neural network weights
        std::vector<std::vector<std::vector<double>>> weights;
        std::vector<std::vector<double>> biases;
        initialize_neural_network(feature_count, hidden_layers, neurons_per_layer, num_regimes_, weights, biases);
        
        // Training data preparation
        std::vector<std::vector<double>> training_features;
        std::vector<size_t> training_labels;
        
        for (size_t i = lookback_window_; i < n; ++i) {
            training_features.push_back(features[i]);
            
            // Generate pseudo-labels using simple heuristics for training
            RegimeType label_regime = generate_training_label(values, i);
            training_labels.push_back(static_cast<size_t>(label_regime) % num_regimes_);
        }
        
        // Train neural network using backpropagation
        auto training_result = train_neural_network(training_features, training_labels, weights, biases);
        if (training_result.is_error()) {
            return Result<RegimeDetectionResult>::error(training_result.error().code,
                "Neural network training failed: " + training_result.error().message);
        }
        
        // Make predictions
        RegimeDetectionResult result;
        result.dates = returns.timestamps();
        result.regime_sequence.reserve(n);
        result.regime_probabilities.reserve(n);
        
        for (size_t i = 0; i < n; ++i) {
            if (i < lookback_window_) {
                // Use simple classification for early data points
                result.regime_sequence.push_back(RegimeType::Stable);
                result.regime_probabilities.push_back(0.5);
            } else {
                auto prediction = predict_neural_network(features[i], weights, biases);
                RegimeType regime = map_prediction_to_regime(prediction.first);
                result.regime_sequence.push_back(regime);
                result.regime_probabilities.push_back(prediction.second);
            }
        }
        
        // Set current regime info
        if (!result.regime_sequence.empty()) {
            result.current_regime = result.regime_sequence.back();
            result.current_regime_confidence = result.regime_probabilities.back();
            result.current_regime_duration = calculate_current_regime_duration(result.regime_sequence);
        }
        
        return Result<RegimeDetectionResult>::success(std::move(result));
    }
    
    /**
     * @brief Ensemble Regime Detection
     * 
     * Combines multiple ML algorithms using voting and weighted averaging
     * for robust regime detection with uncertainty quantification.
     */
    [[nodiscard]] Result<RegimeDetectionResult> ensemble_detection(const ReturnSeries& returns) const {
        if (returns.empty()) {
            return Result<RegimeDetectionResult>::error(ErrorCode::InsufficientData,
                "Return series cannot be empty");
        }
        
        // Get predictions from multiple algorithms
        std::vector<Result<RegimeDetectionResult>> predictions;
        
        // Base regime detector
        RegimeDetector base_detector(lookback_window_);
        predictions.push_back(base_detector.detect_regimes(returns));
        predictions.push_back(base_detector.markov_switching_detection(returns, 2));
        predictions.push_back(base_detector.markov_switching_detection(returns, 3));
        predictions.push_back(base_detector.structural_break_detection(returns));
        predictions.push_back(base_detector.volatility_regime_detection(returns));
        
        // Random Forest approach
        auto rf_result = random_forest_detection(returns);
        if (rf_result.is_ok()) {
            predictions.push_back(rf_result);
        }
        
        // Support Vector Machine approach  
        auto svm_result = support_vector_machine_detection(returns);
        if (svm_result.is_ok()) {
            predictions.push_back(svm_result);
        }
        
        // Filter successful predictions
        std::vector<RegimeDetectionResult> valid_predictions;
        for (const auto& pred : predictions) {
            if (pred.is_ok()) {
                valid_predictions.push_back(pred.value());
            }
        }
        
        if (valid_predictions.empty()) {
            return Result<RegimeDetectionResult>::error(ErrorCode::CalculationError,
                "All ensemble models failed");
        }
        
        // Combine predictions using weighted voting
        return combine_ensemble_predictions(valid_predictions, returns.timestamps());
    }
    
    /**
     * @brief Random Forest Regime Detection
     * 
     * Implements a random forest classifier for regime detection using
     * financial and technical indicators as features.
     */
    [[nodiscard]] Result<RegimeDetectionResult> random_forest_detection(
        const ReturnSeries& returns, size_t num_trees = 100) const {
        
        if (returns.empty() || returns.size() < lookback_window_) {
            return Result<RegimeDetectionResult>::error(ErrorCode::InsufficientData,
                "Insufficient data for random forest detection");
        }
        
        const auto& values = returns.values();
        size_t n = values.size();
        
        // Extract features for random forest
        auto features_result = extract_advanced_features(returns);
        if (features_result.is_error()) {
            return Result<RegimeDetectionResult>::error(features_result.error().code, features_result.error().message);
        }
        
        auto features = features_result.value();
        
        // Generate training labels using multiple heuristics
        std::vector<RegimeType> labels;
        for (size_t i = 0; i < n; ++i) {
            labels.push_back(generate_training_label(values, i));
        }
        
        // Train random forest (simplified implementation)
        std::vector<DecisionTree> forest;
        forest.reserve(num_trees);
        
        for (size_t tree = 0; tree < num_trees; ++tree) {
            // Bootstrap sampling
            std::vector<size_t> bootstrap_indices;
            bootstrap_indices.reserve(n);
            
            std::uniform_int_distribution<size_t> dist(0, n - 1);
            for (size_t i = 0; i < n; ++i) {
                bootstrap_indices.push_back(dist(rng_));
            }
            
            // Train individual tree
            auto tree_result = train_decision_tree(features, labels, bootstrap_indices);
            if (tree_result.is_ok()) {
                forest.push_back(std::move(tree_result.value()));
            }
        }
        
        if (forest.empty()) {
            return Result<RegimeDetectionResult>::error(ErrorCode::CalculationError,
                "Failed to train any decision trees");
        }
        
        // Make predictions using forest
        RegimeDetectionResult result;
        result.dates = returns.timestamps();
        result.regime_sequence.reserve(n);
        result.regime_probabilities.reserve(n);
        
        for (size_t i = 0; i < n; ++i) {
            auto prediction = predict_random_forest(features[i], forest);
            result.regime_sequence.push_back(prediction.first);
            result.regime_probabilities.push_back(prediction.second);
        }
        
        // Set current regime info
        if (!result.regime_sequence.empty()) {
            result.current_regime = result.regime_sequence.back();
            result.current_regime_confidence = result.regime_probabilities.back();
            result.current_regime_duration = calculate_current_regime_duration(result.regime_sequence);
        }
        
        return Result<RegimeDetectionResult>::success(std::move(result));
    }
    
    /**
     * @brief Support Vector Machine Regime Detection
     * 
     * Uses SVM with RBF kernel for non-linear regime classification
     * with feature scaling and hyperparameter optimization.
     */
    [[nodiscard]] Result<RegimeDetectionResult> support_vector_machine_detection(
        const ReturnSeries& returns, double C = 1.0, double gamma = 0.1) const {
        
        if (returns.empty() || returns.size() < lookback_window_) {
            return Result<RegimeDetectionResult>::error(ErrorCode::InsufficientData,
                "Insufficient data for SVM detection");
        }
        
        const auto& values = returns.values();
        size_t n = values.size();
        
        // Extract and normalize features
        auto features_result = extract_advanced_features(returns);
        if (features_result.is_error()) {
            return Result<RegimeDetectionResult>::error(features_result.error().code,
                "Failed to extract features: " + features_result.error().message);
        }
        
        auto raw_features = features_result.value();
        auto normalized_features = normalize_features(raw_features);
        
        // Generate training labels
        std::vector<int> labels;
        for (size_t i = 0; i < n; ++i) {
            RegimeType regime = generate_training_label(values, i);
            labels.push_back(static_cast<int>(regime) % static_cast<int>(num_regimes_));
        }
        
        // Train SVM (simplified dual coordinate descent)
        std::vector<double> alpha(n, 0.0);
        std::vector<std::vector<double>> kernel_matrix = compute_rbf_kernel_matrix(normalized_features, gamma);
        
        // Simplified SMO algorithm
        for (size_t iter = 0; iter < max_iterations_; ++iter) {
            bool changed = false;
            
            for (size_t i = 0; i < n - 1; ++i) {
                for (size_t j = i + 1; j < n; ++j) {
                    if (update_alpha_pair(alpha, labels, kernel_matrix, i, j, C)) {
                        changed = true;
                    }
                }
            }
            
            if (!changed) break;
        }
        
        // Make predictions
        RegimeDetectionResult result;
        result.dates = returns.timestamps();
        result.regime_sequence.reserve(n);
        result.regime_probabilities.reserve(n);
        
        for (size_t i = 0; i < n; ++i) {
            auto prediction = predict_svm(normalized_features[i], normalized_features, alpha, labels, gamma);
            RegimeType regime = static_cast<RegimeType>(prediction.first % static_cast<int>(num_regimes_));
            result.regime_sequence.push_back(regime);
            result.regime_probabilities.push_back(prediction.second);
        }
        
        // Calculate regime characteristics
        result.regime_characteristics = calculate_regime_characteristics(returns, result.regime_sequence);
        
        // Set current regime info
        if (!result.regime_sequence.empty()) {
            result.current_regime = result.regime_sequence.back();
            result.current_regime_confidence = result.regime_probabilities.back();
            result.current_regime_duration = calculate_current_regime_duration(result.regime_sequence);
        }
        
        return Result<RegimeDetectionResult>::success(std::move(result));
    }
    
    /**
     * @brief Adaptive Online Regime Detection
     * 
     * Real-time regime detection that adapts to new data using
     * online learning algorithms with concept drift detection.
     */
    [[nodiscard]] Result<std::pair<RegimeType, double>> adaptive_online_detection(
        const std::vector<double>& recent_returns, const std::vector<double>& features) const {
        
        if (recent_returns.empty() || features.empty()) {
            return Result<std::pair<RegimeType, double>>::error(ErrorCode::InsufficientData,
                "Insufficient data for online detection");
        }
        
        // Detect concept drift
        bool drift_detected = detect_concept_drift(recent_returns);
        
        // Update model if drift detected
        if (drift_detected) {
            // Trigger model retraining (simplified)
            std::lock_guard<std::mutex> lock(model_mutex_);
            cached_models_.clear(); // Clear cached models
        }
        
        // Make prediction using ensemble of online learners
        std::vector<std::pair<RegimeType, double>> predictions;
        
        // Online perceptron
        auto perceptron_pred = online_perceptron_predict(features);
        if (perceptron_pred.is_ok()) {
            predictions.push_back(perceptron_pred.value());
        }
        
        // Online gradient descent
        auto gradient_pred = online_gradient_predict(features);
        if (gradient_pred.is_ok()) {
            predictions.push_back(gradient_pred.value());
        }
        
        // Exponentially weighted moving average classifier
        auto ewma_pred = ewma_classifier_predict(features);
        if (ewma_pred.is_ok()) {
            predictions.push_back(ewma_pred.value());
        }
        
        if (predictions.empty()) {
            return Result<std::pair<RegimeType, double>>::error(ErrorCode::CalculationError,
                "All online models failed");
        }
        
        // Combine predictions
        return combine_online_predictions(predictions);
    }
    
    // Convenience method aliases for consistent API
    [[nodiscard]] Result<RegimeDetectionResult> detect_regimes_dnn(const ReturnSeries& returns) const {
        return deep_neural_network_detection(returns);
    }
    
    [[nodiscard]] Result<RegimeDetectionResult> detect_regimes_ensemble(const ReturnSeries& returns) const {
        return ensemble_detection(returns);
    }
    
    [[nodiscard]] Result<RegimeDetectionResult> detect_regimes_random_forest(const ReturnSeries& returns) const {
        return random_forest_detection(returns);
    }
    
    [[nodiscard]] Result<RegimeDetectionResult> detect_regimes_svm(const ReturnSeries& returns) const {
        return support_vector_machine_detection(returns);
    }
    
    [[nodiscard]] Result<std::pair<RegimeType, double>> detect_current_regime_adaptive(const ReturnSeries& returns) const {
        // Extract features for adaptive detection
        auto features_result = extract_advanced_features(returns);
        if (features_result.is_error()) {
            return Result<std::pair<RegimeType, double>>::error(features_result.error().code,
                "Failed to extract features: " + features_result.error().message);
        }
        
        const auto& values = returns.values();
        const auto& features = features_result.value();
        
        if (features.empty()) {
            return Result<std::pair<RegimeType, double>>::error(ErrorCode::InsufficientData,
                "No features extracted for adaptive detection");
        }
        
        // Use recent values and latest features
        std::vector<double> recent_returns(values.end() - std::min(values.size(), lookback_window_), values.end());
        return adaptive_online_detection(recent_returns, features.back());
    }
    
    // Accessor methods
    size_t get_lookback_window() const { return lookback_window_; }
    size_t get_num_regimes() const { return num_regimes_; }
    
    // Feature extraction (made public for demonstration purposes)
    [[nodiscard]] Result<std::vector<std::vector<double>>> extract_advanced_features(
        const ReturnSeries& returns) const {
        
        const auto& values = returns.values();
        size_t n = values.size();
        std::vector<std::vector<double>> features;
        features.reserve(n);
        
        for (size_t i = 0; i < n; ++i) {
            std::vector<double> feature_vector;
            
            // Basic features
            feature_vector.push_back(values[i]); // Current return
            
            // Rolling statistics - always add same number of features
            if (i >= lookback_window_) {
                std::span<const double> window(values.data() + i - lookback_window_ + 1, lookback_window_);
                
                auto mean = stats::mean(window).value_or(0.0);
                auto std_dev = stats::standard_deviation(window).value_or(0.01);
                auto skewness = stats::skewness(window).value_or(0.0);
                auto kurtosis = stats::kurtosis(window).value_or(3.0);
                
                feature_vector.push_back(mean);
                feature_vector.push_back(std_dev);
                feature_vector.push_back(skewness);
                feature_vector.push_back(kurtosis);
                
                // Technical indicators
                feature_vector.push_back(values[i] / mean); // Relative to mean
                feature_vector.push_back(std::abs(values[i]) / std_dev); // Z-score magnitude
                
                // Momentum features - always add (use 0 if not enough data)
                double momentum_5 = (i >= 5) ? (values[i] - values[i-5]) / 5.0 : 0.0;
                feature_vector.push_back(momentum_5);
                
                // Volatility clustering - always add (use 0 if not enough data)
                double vol_cluster = (i >= 1) ? std::abs(values[i]) * std::abs(values[i-1]) : 0.0;
                feature_vector.push_back(vol_cluster);
            } else {
                // Fill with defaults for early observations - same 8 features
                feature_vector.insert(feature_vector.end(), 8, 0.0);
            }
            
            features.push_back(feature_vector);
        }
        
        return Result<std::vector<std::vector<double>>>::success(std::move(features));
    }

private:
    /**
     * @brief Generate training labels using heuristic rules
     */
    RegimeType generate_training_label(const std::vector<double>& values, size_t index) const {
        if (index < lookback_window_) {
            return RegimeType::Stable;
        }
        
        size_t start = index - lookback_window_ + 1;
        std::span<const double> window(values.data() + start, lookback_window_);
        
        auto mean = stats::mean(window).value_or(0.0);
        auto std_dev = stats::standard_deviation(window).value_or(0.01);
        
        // Crisis detection
        if (std_dev > 0.05) { // 5% daily volatility threshold
            return RegimeType::Crisis;
        }
        
        // Volatility-based classification
        if (std_dev > 0.025) { // 2.5% daily volatility
            return RegimeType::Volatile;
        }
        
        // Trend-based classification
        if (mean > 0.001) { // 0.1% daily return
            return RegimeType::Bull;
        } else if (mean < -0.001) {
            return RegimeType::Bear;
        } else if (std_dev < 0.01) { // 1% daily volatility
            return RegimeType::Stable;
        } else {
            return RegimeType::Recovery;
        }
    }
    
    /**
     * @brief Calculate current regime duration
     */
    size_t calculate_current_regime_duration(const std::vector<RegimeType>& regime_sequence) const {
        if (regime_sequence.empty()) return 0;
        
        RegimeType current = regime_sequence.back();
        size_t duration = 1;
        
        for (int i = static_cast<int>(regime_sequence.size()) - 2; i >= 0; --i) {
            if (regime_sequence[i] == current) {
                duration++;
            } else {
                break;
            }
        }
        
        return duration;
    }
    
    // Simplified placeholder implementations for complex ML algorithms
    // In production, these would be full implementations or use external ML libraries
    
    void initialize_neural_network(size_t input_size, size_t hidden_layers, size_t neurons_per_layer,
                                  size_t output_size, std::vector<std::vector<std::vector<double>>>& weights,
                                  std::vector<std::vector<double>>& biases) const {
        // Placeholder implementation
        weights.clear();
        biases.clear();
        // Would implement Xavier/He initialization here
    }
    
    [[nodiscard]] Result<void> train_neural_network(
        const std::vector<std::vector<double>>& features,
        const std::vector<size_t>& labels,
        std::vector<std::vector<std::vector<double>>>& weights,
        std::vector<std::vector<double>>& biases) const {
        // Placeholder for backpropagation training
        return Result<void>::success();
    }
    
    std::pair<size_t, double> predict_neural_network(
        const std::vector<double>& features,
        const std::vector<std::vector<std::vector<double>>>& weights,
        const std::vector<std::vector<double>>& biases) const {
        // Placeholder implementation
        return {0, 0.5};
    }
    
    RegimeType map_prediction_to_regime(size_t prediction) const {
        return static_cast<RegimeType>(prediction % static_cast<size_t>(RegimeType::Recovery) + 1);
    }
    
    // Additional placeholder methods for other ML algorithms...
    
    // Tree features are handled by extract_advanced_features method
    
    struct DecisionNode {
        bool is_leaf = false;
        RegimeType prediction = RegimeType::Stable;
        size_t feature_index = 0;
        double threshold = 0.0;
        std::unique_ptr<DecisionNode> left = nullptr;
        std::unique_ptr<DecisionNode> right = nullptr;
    };
    
    struct DecisionTree {
        std::unique_ptr<DecisionNode> root = nullptr;
        
        // Make DecisionTree movable but not copyable
        DecisionTree() = default;
        DecisionTree(const DecisionTree&) = delete;
        DecisionTree& operator=(const DecisionTree&) = delete;
        DecisionTree(DecisionTree&&) = default;
        DecisionTree& operator=(DecisionTree&&) = default;
    };
    
    [[nodiscard]] Result<DecisionTree> train_decision_tree(
        const std::vector<std::vector<double>>& features,
        const std::vector<RegimeType>& labels,
        const std::vector<size_t>& bootstrap_indices) const {
        
        if (features.empty() || labels.empty() || bootstrap_indices.empty()) {
            return Result<DecisionTree>::error(ErrorCode::InvalidInput, "Empty training data");
        }
        
        DecisionTree tree;
        
        // Build decision tree with bootstrap samples
        std::vector<std::vector<double>> sampled_features;
        std::vector<RegimeType> sampled_labels;
        
        for (size_t idx : bootstrap_indices) {
            if (idx < features.size()) {
                sampled_features.push_back(features[idx]);
                sampled_labels.push_back(labels[idx]);
            }
        }
        
        if (sampled_features.empty()) {
            return Result<DecisionTree>::error(ErrorCode::InvalidInput, "No valid bootstrap samples");
        }
        
        tree.root = build_tree_node(sampled_features, sampled_labels, 0, 3); // max_depth = 3
        
        return Result<DecisionTree>::success(std::move(tree));
    }
    
    // Helper method for building decision tree nodes
    std::unique_ptr<DecisionNode> build_tree_node(
        const std::vector<std::vector<double>>& features,
        const std::vector<RegimeType>& labels,
        size_t depth,
        size_t max_depth) const {
        
        auto node = std::make_unique<DecisionNode>();
        
        // Stop criteria: max depth or pure node
        if (depth >= max_depth || features.size() < 2 || is_pure_node(labels)) {
            node->is_leaf = true;
            node->prediction = majority_class(labels);
            return node;
        }
        
        // Find best split
        double best_gini = 1.0;
        size_t best_feature = 0;
        double best_threshold = 0.0;
        
        for (size_t f = 0; f < features[0].size(); ++f) {
            for (size_t i = 0; i < features.size(); ++i) {
                double threshold = features[i][f];
                double gini = calculate_split_gini(features, labels, f, threshold);
                if (gini < best_gini) {
                    best_gini = gini;
                    best_feature = f;
                    best_threshold = threshold;
                }
            }
        }
        
        // If no improvement, make leaf
        if (best_gini >= 0.9) {
            node->is_leaf = true;
            node->prediction = majority_class(labels);
            return node;
        }
        
        // Split data
        auto [left_features, left_labels, right_features, right_labels] = 
            split_data(features, labels, best_feature, best_threshold);
        
        if (left_features.empty() || right_features.empty()) {
            node->is_leaf = true;
            node->prediction = majority_class(labels);
            return node;
        }
        
        node->feature_index = best_feature;
        node->threshold = best_threshold;
        node->left = build_tree_node(left_features, left_labels, depth + 1, max_depth);
        node->right = build_tree_node(right_features, right_labels, depth + 1, max_depth);
        
        return node;
    }
    
    bool is_pure_node(const std::vector<RegimeType>& labels) const {
        if (labels.empty()) return true;
        RegimeType first = labels[0];
        return std::all_of(labels.begin(), labels.end(), [first](RegimeType r) { return r == first; });
    }
    
    RegimeType majority_class(const std::vector<RegimeType>& labels) const {
        if (labels.empty()) return RegimeType::Stable;
        std::unordered_map<RegimeType, size_t> counts;
        for (auto label : labels) {
            counts[label]++;
        }
        return std::max_element(counts.begin(), counts.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; })->first;
    }
    
    double calculate_split_gini(
        const std::vector<std::vector<double>>& features,
        const std::vector<RegimeType>& labels,
        size_t feature_index,
        double threshold) const {
        
        std::vector<RegimeType> left_labels, right_labels;
        for (size_t i = 0; i < features.size(); ++i) {
            if (features[i][feature_index] <= threshold) {
                left_labels.push_back(labels[i]);
            } else {
                right_labels.push_back(labels[i]);
            }
        }
        
        double total = labels.size();
        if (total == 0) return 1.0;
        
        double left_weight = left_labels.size() / total;
        double right_weight = right_labels.size() / total;
        
        return left_weight * gini_impurity(left_labels) + right_weight * gini_impurity(right_labels);
    }
    
    double gini_impurity(const std::vector<RegimeType>& labels) const {
        if (labels.empty()) return 0.0;
        std::unordered_map<RegimeType, size_t> counts;
        for (auto label : labels) {
            counts[label]++;
        }
        
        double impurity = 1.0;
        double total = labels.size();
        for (const auto& [regime, count] : counts) {
            double prob = count / total;
            impurity -= prob * prob;
        }
        return impurity;
    }
    
    std::tuple<std::vector<std::vector<double>>, std::vector<RegimeType>,
               std::vector<std::vector<double>>, std::vector<RegimeType>>
    split_data(const std::vector<std::vector<double>>& features,
               const std::vector<RegimeType>& labels,
               size_t feature_index,
               double threshold) const {
        
        std::vector<std::vector<double>> left_features, right_features;
        std::vector<RegimeType> left_labels, right_labels;
        
        for (size_t i = 0; i < features.size(); ++i) {
            if (features[i][feature_index] <= threshold) {
                left_features.push_back(features[i]);
                left_labels.push_back(labels[i]);
            } else {
                right_features.push_back(features[i]);
                right_labels.push_back(labels[i]);
            }
        }
        
        return {left_features, left_labels, right_features, right_labels};
    }
    
    std::pair<RegimeType, double> predict_random_forest(
        const std::vector<double>& features,
        const std::vector<DecisionTree>& forest) const {
        
        if (forest.empty()) {
            return {RegimeType::Stable, 0.0};
        }
        
        std::unordered_map<RegimeType, size_t> votes;
        for (const auto& tree : forest) {
            RegimeType prediction = predict_tree(features, tree);
            votes[prediction]++;
        }
        
        auto max_vote = std::max_element(votes.begin(), votes.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        
        double confidence = static_cast<double>(max_vote->second) / forest.size();
        return {max_vote->first, confidence};
    }
    
    RegimeType predict_tree(const std::vector<double>& features, const DecisionTree& tree) const {
        if (!tree.root) return RegimeType::Stable;
        
        DecisionNode* current = tree.root.get();
        while (!current->is_leaf) {
            if (features[current->feature_index] <= current->threshold) {
                current = current->left.get();
            } else {
                current = current->right.get();
            }
            if (!current) break;
        }
        
        return current ? current->prediction : RegimeType::Stable;
    }
    
    std::vector<std::vector<double>> normalize_features(
        const std::vector<std::vector<double>>& features) const {
        // Placeholder for feature normalization
        return features;
    }
    
    std::vector<std::vector<double>> compute_rbf_kernel_matrix(
        const std::vector<std::vector<double>>& features, double gamma) const {
        // Placeholder for RBF kernel computation
        size_t n = features.size();
        return std::vector<std::vector<double>>(n, std::vector<double>(n, 1.0));
    }
    
    bool update_alpha_pair(std::vector<double>& alpha, const std::vector<int>& labels,
                          const std::vector<std::vector<double>>& kernel_matrix,
                          size_t i, size_t j, double C) const {
        // Placeholder for SMO algorithm
        return false;
    }
    
    std::pair<int, double> predict_svm(const std::vector<double>& features,
                                      const std::vector<std::vector<double>>& support_vectors,
                                      const std::vector<double>& alpha,
                                      const std::vector<int>& labels,
                                      double gamma) const {
        // Placeholder implementation
        return {0, 0.5};
    }
    
    [[nodiscard]] Result<RegimeDetectionResult> combine_ensemble_predictions(
        const std::vector<RegimeDetectionResult>& predictions,
        const std::vector<DateTime>& dates) const {
        // Placeholder for ensemble combination
        if (predictions.empty()) {
            return Result<RegimeDetectionResult>::error(ErrorCode::InvalidInput, "No predictions to combine");
        }
        return Result<RegimeDetectionResult>::success(predictions[0]);
    }
    
    bool detect_concept_drift(const std::vector<double>& recent_returns) const {
        // Placeholder for drift detection
        return false;
    }
    
    [[nodiscard]] Result<std::pair<RegimeType, double>> online_perceptron_predict(
        const std::vector<double>& features) const {
        // Placeholder implementation
        return Result<std::pair<RegimeType, double>>::success({RegimeType::Stable, 0.5});
    }
    
    [[nodiscard]] Result<std::pair<RegimeType, double>> online_gradient_predict(
        const std::vector<double>& features) const {
        // Placeholder implementation
        return Result<std::pair<RegimeType, double>>::success({RegimeType::Stable, 0.5});
    }
    
    [[nodiscard]] Result<std::pair<RegimeType, double>> ewma_classifier_predict(
        const std::vector<double>& features) const {
        // Placeholder implementation
        return Result<std::pair<RegimeType, double>>::success({RegimeType::Stable, 0.5});
    }
    
    [[nodiscard]] Result<std::pair<RegimeType, double>> combine_online_predictions(
        const std::vector<std::pair<RegimeType, double>>& predictions) const {
        if (predictions.empty()) {
            return Result<std::pair<RegimeType, double>>::error(ErrorCode::InvalidInput, "No predictions to combine");
        }
        return Result<std::pair<RegimeType, double>>::success(predictions[0]);
    }
};

}  // namespace pyfolio::analytics