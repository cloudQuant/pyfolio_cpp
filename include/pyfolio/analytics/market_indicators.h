#pragma once

#include "../core/datetime.h"
#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../core/types.h"
#include "../math/statistics.h"
#include <cmath>
#include <map>
#include <vector>

namespace pyfolio::analytics {

/**
 * @brief Market indicator types
 */
enum class IndicatorType {
    VIX,              // Volatility Index
    TermSpread,       // Term structure spread
    CreditSpread,     // Credit risk spread
    YieldCurveSlope,  // Yield curve slope
    PutCallRatio,     // Put/Call options ratio
    HighYieldSpread,  // High yield bond spread
    DollarIndex,      // US Dollar strength index
    CommodityIndex    // Commodity price index
};

/**
 * @brief Market indicator value with metadata
 */
struct IndicatorValue {
    double value;
    DateTime timestamp;
    IndicatorType type;
    double percentile;   // Historical percentile (0-100)
    std::string regime;  // Associated market regime

    /**
     * @brief Check if indicator suggests stress
     */
    bool indicates_stress() const {
        switch (type) {
            case IndicatorType::VIX:
                return value > 30.0;  // VIX > 30 suggests high stress
            case IndicatorType::CreditSpread:
            case IndicatorType::HighYieldSpread:
                return percentile > 75.0;  // High spreads suggest stress
            case IndicatorType::TermSpread:
                return value < 0.5;  // Flat/inverted curve suggests stress
            case IndicatorType::PutCallRatio:
                return value > 1.2;  // High put/call ratio suggests fear
            default:
                return percentile > 80.0;  // General stress threshold
        }
    }

    /**
     * @brief Get signal strength (0-1)
     */
    double signal_strength() const { return std::min(1.0, percentile / 100.0); }
};

/**
 * @brief Collection of market indicators
 */
struct MarketIndicators {
    std::vector<double> vix_levels;
    std::vector<double> term_spreads;        // 10Y-2Y spread
    std::vector<double> credit_spreads;      // Investment grade credit spread
    std::vector<double> yield_curve_slopes;  // 30Y-3M spread
    std::vector<double> put_call_ratios;
    std::vector<double> high_yield_spreads;
    std::vector<double> dollar_index;
    std::vector<double> commodity_index;

    /**
     * @brief Get number of observations
     */
    size_t size() const { return vix_levels.size(); }

    /**
     * @brief Check if empty
     */
    bool empty() const { return vix_levels.empty(); }

    /**
     * @brief Resize all indicators
     */
    void resize(size_t new_size) {
        vix_levels.resize(new_size);
        term_spreads.resize(new_size);
        credit_spreads.resize(new_size);
        yield_curve_slopes.resize(new_size);
        put_call_ratios.resize(new_size);
        high_yield_spreads.resize(new_size);
        dollar_index.resize(new_size);
        commodity_index.resize(new_size);
    }

    /**
     * @brief Get indicator by type and index
     */
    Result<double> get_indicator(IndicatorType type, size_t index) const {
        if (index >= size()) {
            return Result<double>::error(ErrorCode::InvalidInput, "Index out of range");
        }

        switch (type) {
            case IndicatorType::VIX:
                return Result<double>::success(vix_levels[index]);
            case IndicatorType::TermSpread:
                return Result<double>::success(term_spreads[index]);
            case IndicatorType::CreditSpread:
                return Result<double>::success(credit_spreads[index]);
            case IndicatorType::YieldCurveSlope:
                return Result<double>::success(yield_curve_slopes[index]);
            case IndicatorType::PutCallRatio:
                return Result<double>::success(put_call_ratios[index]);
            case IndicatorType::HighYieldSpread:
                return Result<double>::success(high_yield_spreads[index]);
            case IndicatorType::DollarIndex:
                return Result<double>::success(dollar_index[index]);
            case IndicatorType::CommodityIndex:
                return Result<double>::success(commodity_index[index]);
            default:
                return Result<double>::error(ErrorCode::InvalidInput, "Unknown indicator type");
        }
    }
};

/**
 * @brief Time series of market indicators
 */
using MarketIndicatorSeries = TimeSeries<MarketIndicators>;

/**
 * @brief Market indicators analyzer
 */
class MarketIndicatorsAnalyzer {
  private:
    std::map<IndicatorType, std::vector<double>> historical_data_;
    std::map<IndicatorType, std::pair<double, double>> percentile_bounds_;

  public:
    /**
     * @brief Constructor
     */
    MarketIndicatorsAnalyzer() = default;

    /**
     * @brief Set historical data for percentile calculations
     */
    void set_historical_data(IndicatorType type, const std::vector<double>& data) {
        historical_data_[type] = data;
        calculate_percentile_bounds(type);
    }

    /**
     * @brief Calculate VIX regime indicator
     */
    Result<TimeSeries<double>> calculate_vix_regime(const TimeSeries<double>& vix_data) const {
        if (vix_data.empty()) {
            return Result<TimeSeries<double>>::error(ErrorCode::InsufficientData, "VIX data cannot be empty");
        }

        std::vector<DateTime> timestamps = vix_data.timestamps();
        std::vector<double> regime_scores;
        regime_scores.reserve(vix_data.size());

        const auto& values = vix_data.values();

        for (double vix : values) {
            double score = 0.0;

            // VIX regime scoring:
            // < 15: Low volatility (score 0.2)
            // 15-25: Normal volatility (score 0.5)
            // 25-35: Elevated volatility (score 0.8)
            // > 35: Crisis volatility (score 1.0)

            if (vix < 15.0) {
                score = 0.2;
            } else if (vix < 25.0) {
                score = 0.5;
            } else if (vix < 35.0) {
                score = 0.8;
            } else {
                score = 1.0;
            }

            regime_scores.push_back(score);
        }

        return Result<TimeSeries<double>>::success(
            TimeSeries<double>{std::move(timestamps), std::move(regime_scores), "vix_regime"});
    }

    /**
     * @brief Calculate yield curve slope regime indicator
     */
    Result<TimeSeries<double>> calculate_yield_curve_slope(const TimeSeries<double>& slope_data) const {
        if (slope_data.empty()) {
            return Result<TimeSeries<double>>::error(ErrorCode::InsufficientData, "Yield curve data cannot be empty");
        }

        std::vector<DateTime> timestamps = slope_data.timestamps();
        std::vector<double> regime_scores;
        regime_scores.reserve(slope_data.size());

        const auto& values = slope_data.values();

        for (double slope : values) {
            double score = 0.0;

            // Yield curve slope regime scoring:
            // < 0: Inverted (crisis score 1.0)
            // 0-0.5: Flat (elevated score 0.8)
            // 0.5-1.5: Normal (neutral score 0.5)
            // 1.5-2.5: Steep (favorable score 0.3)
            // > 2.5: Very steep (favorable score 0.2)

            if (slope < 0.0) {
                score = 1.0;  // Inverted curve - crisis signal
            } else if (slope < 0.5) {
                score = 0.8;  // Flat curve - stress signal
            } else if (slope < 1.5) {
                score = 0.5;  // Normal curve
            } else if (slope < 2.5) {
                score = 0.3;  // Steep curve - favorable
            } else {
                score = 0.2;  // Very steep - very favorable
            }

            regime_scores.push_back(score);
        }

        return Result<TimeSeries<double>>::success(
            TimeSeries<double>{std::move(timestamps), std::move(regime_scores), "yield_curve_regime"});
    }

    /**
     * @brief Calculate credit spreads regime indicator
     */
    Result<TimeSeries<double>> calculate_credit_spreads(const TimeSeries<double>& spread_data) const {
        if (spread_data.empty()) {
            return Result<TimeSeries<double>>::error(ErrorCode::InsufficientData, "Credit spread data cannot be empty");
        }

        // Calculate rolling percentiles for dynamic thresholds
        const auto& values               = spread_data.values();
        std::vector<DateTime> timestamps = spread_data.timestamps();
        std::vector<double> regime_scores;
        regime_scores.reserve(values.size());

        // Use rolling window for percentile calculation
        const size_t window = std::min(static_cast<size_t>(252), values.size());  // 1 year window

        for (size_t i = 0; i < values.size(); ++i) {
            size_t start = (i >= window) ? i - window + 1 : 0;
            std::vector<double> window_data(values.begin() + start, values.begin() + i + 1);

            std::sort(window_data.begin(), window_data.end());

            double current_spread = values[i];
            double percentile     = calculate_percentile(window_data, current_spread);

            // Convert percentile to regime score
            double score = 0.0;
            if (percentile > 90.0) {
                score = 1.0;  // Very high spreads - crisis
            } else if (percentile > 75.0) {
                score = 0.8;  // High spreads - stress
            } else if (percentile > 50.0) {
                score = 0.5;  // Normal spreads
            } else if (percentile > 25.0) {
                score = 0.3;  // Low spreads - favorable
            } else {
                score = 0.2;  // Very low spreads - very favorable
            }

            regime_scores.push_back(score);
        }

        return Result<TimeSeries<double>>::success(
            TimeSeries<double>{std::move(timestamps), std::move(regime_scores), "credit_regime"});
    }

    /**
     * @brief Analyze multiple indicators for regime classification
     */
    Result<TimeSeries<IndicatorValue>> analyze_regime_indicators(const MarketIndicatorSeries& indicators) const {
        if (indicators.empty()) {
            return Result<TimeSeries<IndicatorValue>>::error(ErrorCode::InsufficientData,
                                                             "Market indicators cannot be empty");
        }

        std::vector<DateTime> timestamps = indicators.timestamps();
        std::vector<IndicatorValue> indicator_values;
        indicator_values.reserve(indicators.size());

        const auto& data_series = indicators.values();

        for (size_t i = 0; i < data_series.size(); ++i) {
            const auto& market_data = data_series[i];

            // Calculate composite stress indicator
            double stress_score = 0.0;
            int indicator_count = 0;

            // VIX contribution
            if (i < market_data.vix_levels.size() && market_data.vix_levels[i] > 0) {
                double vix_score = std::min(1.0, market_data.vix_levels[i] / 50.0);
                stress_score += vix_score;
                indicator_count++;
            }

            // Credit spread contribution
            if (i < market_data.credit_spreads.size() && market_data.credit_spreads[i] > 0) {
                double credit_score = std::min(1.0, market_data.credit_spreads[i] / 5.0);
                stress_score += credit_score;
                indicator_count++;
            }

            // Term spread contribution (inverted - low spread = high stress)
            if (i < market_data.term_spreads.size()) {
                double term_score = market_data.term_spreads[i] < 0.5 ? 0.8 : 0.2;
                stress_score += term_score;
                indicator_count++;
            }

            if (indicator_count > 0) {
                stress_score /= indicator_count;
            }

            IndicatorValue indicator_val;
            indicator_val.value      = stress_score;
            indicator_val.timestamp  = timestamps[i];
            indicator_val.type       = IndicatorType::VIX;  // Composite indicator
            indicator_val.percentile = stress_score * 100.0;

            // Determine regime based on stress score
            if (stress_score > 0.8) {
                indicator_val.regime = "Crisis";
            } else if (stress_score > 0.6) {
                indicator_val.regime = "Stress";
            } else if (stress_score > 0.4) {
                indicator_val.regime = "Elevated";
            } else if (stress_score > 0.2) {
                indicator_val.regime = "Normal";
            } else {
                indicator_val.regime = "Calm";
            }

            indicator_values.push_back(indicator_val);
        }

        return Result<TimeSeries<IndicatorValue>>::success(
            TimeSeries<IndicatorValue>{std::move(timestamps), std::move(indicator_values), "regime_indicators"});
    }

    /**
     * @brief Calculate indicator correlation matrix
     */
    Result<std::map<std::pair<IndicatorType, IndicatorType>, double>> calculate_indicator_correlations(
        const MarketIndicatorSeries& indicators) const {
        if (indicators.empty()) {
            return Result<std::map<std::pair<IndicatorType, IndicatorType>, double>>::error(
                ErrorCode::InsufficientData, "Market indicators cannot be empty");
        }

        std::map<std::pair<IndicatorType, IndicatorType>, double> correlations;

        const auto& data_series = indicators.values();
        if (data_series.empty()) {
            return Result<std::map<std::pair<IndicatorType, IndicatorType>, double>>::error(
                ErrorCode::InsufficientData, "No indicator data available");
        }

        const auto& first_data = data_series[0];
        size_t n               = data_series.size();

        // Extract VIX and credit spread data for correlation
        std::vector<double> vix_data, credit_data;
        vix_data.reserve(n);
        credit_data.reserve(n);

        for (const auto& market_data : data_series) {
            if (!market_data.vix_levels.empty()) {
                vix_data.push_back(market_data.vix_levels[0]);
            }
            if (!market_data.credit_spreads.empty()) {
                credit_data.push_back(market_data.credit_spreads[0]);
            }
        }

        // Calculate VIX-Credit correlation
        if (vix_data.size() > 1 && credit_data.size() > 1) {
            auto corr_result =
                stats::correlation(std::span<const double>{vix_data}, std::span<const double>{credit_data});

            if (corr_result.is_ok()) {
                correlations[{IndicatorType::VIX, IndicatorType::CreditSpread}] = corr_result.value();
                correlations[{IndicatorType::CreditSpread, IndicatorType::VIX}] = corr_result.value();
            }
        }

        return Result<std::map<std::pair<IndicatorType, IndicatorType>, double>>::success(std::move(correlations));
    }

    /**
     * @brief Get current market stress level
     */
    Result<std::pair<double, std::string>> get_current_stress_level(const MarketIndicators& current_indicators) const {
        if (current_indicators.empty()) {
            return Result<std::pair<double, std::string>>::error(ErrorCode::InsufficientData,
                                                                 "Current indicators cannot be empty");
        }

        double stress_score  = 0.0;
        int valid_indicators = 0;

        // VIX stress component
        if (!current_indicators.vix_levels.empty() && current_indicators.vix_levels[0] > 0) {
            double vix = current_indicators.vix_levels[0];
            stress_score += (vix > 30.0) ? 1.0 : (vix > 20.0) ? 0.6 : 0.3;
            valid_indicators++;
        }

        // Credit spread stress component
        if (!current_indicators.credit_spreads.empty() && current_indicators.credit_spreads[0] > 0) {
            double spread = current_indicators.credit_spreads[0];
            stress_score += (spread > 3.0) ? 1.0 : (spread > 2.0) ? 0.6 : 0.3;
            valid_indicators++;
        }

        // Term spread stress component
        if (!current_indicators.term_spreads.empty()) {
            double term_spread = current_indicators.term_spreads[0];
            stress_score += (term_spread < 0.0) ? 1.0 : (term_spread < 0.5) ? 0.8 : 0.2;
            valid_indicators++;
        }

        if (valid_indicators == 0) {
            return Result<std::pair<double, std::string>>::error(ErrorCode::InsufficientData,
                                                                 "No valid indicators available");
        }

        stress_score /= valid_indicators;

        std::string stress_level;
        if (stress_score > 0.8) {
            stress_level = "Extreme Stress";
        } else if (stress_score > 0.6) {
            stress_level = "High Stress";
        } else if (stress_score > 0.4) {
            stress_level = "Moderate Stress";
        } else if (stress_score > 0.2) {
            stress_level = "Low Stress";
        } else {
            stress_level = "Minimal Stress";
        }

        return Result<std::pair<double, std::string>>::success(std::make_pair(stress_score, stress_level));
    }

  private:
    /**
     * @brief Calculate percentile bounds for an indicator
     */
    void calculate_percentile_bounds(IndicatorType type) {
        auto it = historical_data_.find(type);
        if (it != historical_data_.end() && !it->second.empty()) {
            std::vector<double> sorted_data = it->second;
            std::sort(sorted_data.begin(), sorted_data.end());

            size_t p25_idx = static_cast<size_t>(sorted_data.size() * 0.25);
            size_t p75_idx = static_cast<size_t>(sorted_data.size() * 0.75);

            double p25 = sorted_data[p25_idx];
            double p75 = sorted_data[p75_idx];

            percentile_bounds_[type] = {p25, p75};
        }
    }

    /**
     * @brief Calculate percentile of value in dataset
     */
    double calculate_percentile(const std::vector<double>& sorted_data, double value) const {
        if (sorted_data.empty())
            return 50.0;

        auto it     = std::lower_bound(sorted_data.begin(), sorted_data.end(), value);
        size_t rank = std::distance(sorted_data.begin(), it);

        return 100.0 * static_cast<double>(rank) / sorted_data.size();
    }
};

/**
 * @brief Convert indicator type to string
 */
inline std::string indicator_type_to_string(IndicatorType type) {
    switch (type) {
        case IndicatorType::VIX:
            return "VIX";
        case IndicatorType::TermSpread:
            return "Term Spread";
        case IndicatorType::CreditSpread:
            return "Credit Spread";
        case IndicatorType::YieldCurveSlope:
            return "Yield Curve Slope";
        case IndicatorType::PutCallRatio:
            return "Put/Call Ratio";
        case IndicatorType::HighYieldSpread:
            return "High Yield Spread";
        case IndicatorType::DollarIndex:
            return "Dollar Index";
        case IndicatorType::CommodityIndex:
            return "Commodity Index";
        default:
            return "Unknown";
    }
}

}  // namespace pyfolio::analytics