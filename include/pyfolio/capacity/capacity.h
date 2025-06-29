#pragma once

#include "../core/datetime.h"
#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../core/types.h"
#include "../positions/holdings.h"
#include "../transactions/transaction.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <vector>

namespace pyfolio::capacity {

/**
 * @brief Market impact model types
 */
enum class ImpactModel { Linear, SquareRoot, ThreeHalves, Almgren };

/**
 * @brief Liquidity constraint types
 */
enum class LiquidityConstraint {
    VolumePercent,    // Percentage of daily volume
    ADVMultiple,      // Multiple of average daily volume
    AbsoluteShares,   // Absolute share limit
    MarketCapPercent  // Percentage of market cap
};

/**
 * @brief Market microstructure data for capacity analysis
 */
struct MarketMicrostructure {
    Symbol symbol;
    double average_daily_volume;
    double average_dollar_volume;
    double market_cap;
    double typical_spread_bps;
    double price_impact_coefficient;
    double volatility;

    /**
     * @brief Calculate bid-ask spread impact
     */
    double calculate_spread_cost(double shares, double price) const {
        double dollar_amount = shares * price;
        return dollar_amount * (typical_spread_bps / 10000.0) * 0.5;  // Half spread
    }

    /**
     * @brief Estimate price impact using square root model
     */
    double estimate_price_impact(double shares, double price) const {
        if (average_daily_volume <= 0)
            return 0.0;

        double participation_rate = shares / average_daily_volume;
        double volatility_factor  = volatility * price;

        // Square root impact model: impact = coefficient * volatility * sqrt(participation_rate)
        return price_impact_coefficient * volatility_factor * std::sqrt(participation_rate);
    }
};

/**
 * @brief Trading capacity constraints
 */
struct CapacityConstraints {
    double max_adv_participation  = 0.10;   // Max 10% of average daily volume
    double max_single_day_volume  = 0.05;   // Max 5% of single day volume
    double max_market_cap_percent = 0.01;   // Max 1% of market cap
    double max_spread_cost_bps    = 50.0;   // Max 50 bps spread cost
    double max_impact_bps         = 100.0;  // Max 100 bps price impact
    size_t max_trading_days       = 30;     // Max 30 days to complete trade

    /**
     * @brief Check if constraints are violated
     */
    bool is_violated(double adv_participation, double spread_cost_bps, double impact_bps, size_t trading_days) const {
        return adv_participation > max_adv_participation || spread_cost_bps > max_spread_cost_bps ||
               impact_bps > max_impact_bps || trading_days > max_trading_days;
    }
};

/**
 * @brief Capacity analysis results for a single security
 */
struct SecurityCapacityResult {
    Symbol symbol;
    double max_position_shares;
    double max_position_dollars;
    double max_daily_trade_shares;
    double max_daily_trade_dollars;
    double estimated_impact_cost;
    double estimated_spread_cost;
    double total_trading_cost;
    size_t estimated_trading_days;
    LiquidityConstraint binding_constraint;

    /**
     * @brief Get total cost as basis points
     */
    double total_cost_bps() const {
        if (max_position_dollars <= 0)
            return 0.0;
        return (total_trading_cost / max_position_dollars) * 10000.0;
    }

    /**
     * @brief Check if position is capacity constrained
     */
    bool is_capacity_constrained() const { return binding_constraint != LiquidityConstraint::AbsoluteShares; }
};

/**
 * @brief Portfolio-level capacity analysis
 */
struct PortfolioCapacityResult {
    std::map<Symbol, SecurityCapacityResult> security_results;
    double total_portfolio_capacity;
    double current_portfolio_size;
    double capacity_utilization;
    double total_estimated_costs;
    double average_trading_days;
    std::vector<Symbol> capacity_constrained_securities;

    /**
     * @brief Get capacity result for specific security
     */
    SecurityCapacityResult get_security_result(const Symbol& symbol) const {
        auto it = security_results.find(symbol);
        if (it != security_results.end()) {
            return it->second;
        }
        return SecurityCapacityResult{};  // Empty result
    }

    /**
     * @brief Calculate capacity headroom
     */
    double capacity_headroom() const {
        if (total_portfolio_capacity <= 0)
            return 0.0;
        return (total_portfolio_capacity - current_portfolio_size) / total_portfolio_capacity;
    }

    /**
     * @brief Check if portfolio is near capacity limits
     */
    bool is_near_capacity_limit(double threshold = 0.80) const { return capacity_utilization > threshold; }
};

/**
 * @brief Advanced capacity analyzer
 */
class CapacityAnalyzer {
  private:
    CapacityConstraints constraints_;
    std::map<Symbol, MarketMicrostructure> market_data_;
    ImpactModel impact_model_;

  public:
    /**
     * @brief Constructor with default constraints
     */
    CapacityAnalyzer() : impact_model_(ImpactModel::SquareRoot) {}

    /**
     * @brief Constructor with custom constraints
     */
    explicit CapacityAnalyzer(const CapacityConstraints& constraints, ImpactModel model = ImpactModel::SquareRoot)
        : constraints_(constraints), impact_model_(model) {}

    /**
     * @brief Set market microstructure data
     */
    void set_market_data(const std::map<Symbol, MarketMicrostructure>& market_data) { market_data_ = market_data; }

    /**
     * @brief Update constraints
     */
    void set_constraints(const CapacityConstraints& constraints) { constraints_ = constraints; }

    /**
     * @brief Analyze capacity for a single security
     */
    Result<SecurityCapacityResult> analyze_security_capacity(const Symbol& symbol, double target_position_dollars,
                                                             double current_price) const {
        auto market_it = market_data_.find(symbol);
        if (market_it == market_data_.end()) {
            return Result<SecurityCapacityResult>::error(ErrorCode::InvalidSymbol,
                                                         "No market data available for symbol: " + symbol);
        }

        const auto& market_data = market_it->second;
        SecurityCapacityResult result;
        result.symbol = symbol;

        if (current_price <= 0) {
            return Result<SecurityCapacityResult>::error(ErrorCode::InvalidInput, "Current price must be positive");
        }

        // Calculate maximum position based on different constraints
        double max_shares_adv        = market_data.average_daily_volume * constraints_.max_adv_participation;
        double max_shares_market_cap = (market_data.market_cap * constraints_.max_market_cap_percent) / current_price;

        // Take the most restrictive constraint
        result.max_position_shares  = std::min(max_shares_adv, max_shares_market_cap);
        result.max_position_dollars = result.max_position_shares * current_price;

        // Determine binding constraint
        if (max_shares_adv < max_shares_market_cap) {
            result.binding_constraint = LiquidityConstraint::ADVMultiple;
        } else {
            result.binding_constraint = LiquidityConstraint::MarketCapPercent;
        }

        // Calculate daily trading limits
        result.max_daily_trade_shares  = market_data.average_daily_volume * constraints_.max_single_day_volume;
        result.max_daily_trade_dollars = result.max_daily_trade_shares * current_price;

        // Estimate trading timeline
        double target_shares = std::min(target_position_dollars / current_price, result.max_position_shares);
        if (result.max_daily_trade_shares > 0) {
            result.estimated_trading_days =
                static_cast<size_t>(std::ceil(target_shares / result.max_daily_trade_shares));
        }

        // Calculate trading costs
        result.estimated_spread_cost = market_data.calculate_spread_cost(target_shares, current_price);
        result.estimated_impact_cost = market_data.estimate_price_impact(target_shares, current_price) * target_shares;
        result.total_trading_cost    = result.estimated_spread_cost + result.estimated_impact_cost;

        // Check constraint violations
        double adv_participation = target_shares / market_data.average_daily_volume;
        double spread_cost_bps   = (result.estimated_spread_cost / (target_shares * current_price)) * 10000.0;
        double impact_bps        = (result.estimated_impact_cost / (target_shares * current_price)) * 10000.0;

        if (constraints_.is_violated(adv_participation, spread_cost_bps, impact_bps, result.estimated_trading_days)) {
            // Adjust position size to meet constraints
            result =
                optimize_position_size(symbol, target_position_dollars, current_price, market_data).value_or(result);
        }

        return Result<SecurityCapacityResult>::success(result);
    }

    /**
     * @brief Analyze portfolio capacity
     */
    Result<PortfolioCapacityResult> analyze_portfolio_capacity(const std::map<Symbol, double>& target_weights,
                                                               double total_portfolio_value,
                                                               const std::map<Symbol, double>& current_prices) const {
        if (target_weights.empty()) {
            return Result<PortfolioCapacityResult>::error(ErrorCode::InvalidInput, "Target weights cannot be empty");
        }

        PortfolioCapacityResult portfolio_result;
        portfolio_result.current_portfolio_size   = total_portfolio_value;
        portfolio_result.total_portfolio_capacity = 0.0;
        portfolio_result.total_estimated_costs    = 0.0;

        std::vector<double> trading_days_vector;

        // Analyze each security
        for (const auto& [symbol, weight] : target_weights) {
            auto price_it = current_prices.find(symbol);
            if (price_it == current_prices.end()) {
                continue;  // Skip securities without price data
            }

            double target_dollars = total_portfolio_value * weight;
            double current_price  = price_it->second;

            auto security_result = analyze_security_capacity(symbol, target_dollars, current_price);
            if (security_result.is_ok()) {
                const auto& result                        = security_result.value();
                portfolio_result.security_results[symbol] = result;

                // Aggregate portfolio metrics
                portfolio_result.total_portfolio_capacity += result.max_position_dollars;
                portfolio_result.total_estimated_costs += result.total_trading_cost;

                if (result.estimated_trading_days > 0) {
                    trading_days_vector.push_back(static_cast<double>(result.estimated_trading_days));
                }

                // Track capacity constrained securities
                if (result.is_capacity_constrained()) {
                    portfolio_result.capacity_constrained_securities.push_back(symbol);
                }
            }
        }

        // Calculate portfolio-level metrics
        if (portfolio_result.total_portfolio_capacity > 0) {
            portfolio_result.capacity_utilization =
                portfolio_result.current_portfolio_size / portfolio_result.total_portfolio_capacity;
        }

        if (!trading_days_vector.empty()) {
            portfolio_result.average_trading_days =
                std::accumulate(trading_days_vector.begin(), trading_days_vector.end(), 0.0) /
                trading_days_vector.size();
        }

        return Result<PortfolioCapacityResult>::success(std::move(portfolio_result));
    }

    /**
     * @brief Calculate capacity decay over time
     */
    Result<TimeSeries<double>> calculate_capacity_decay(const Symbol& symbol, double initial_capacity,
                                                        const TimeSeries<double>& volume_series) const {
        if (volume_series.empty()) {
            return Result<TimeSeries<double>>::error(ErrorCode::InsufficientData, "Volume series cannot be empty");
        }

        std::vector<double> capacity_values;
        capacity_values.reserve(volume_series.size());

        auto market_it = market_data_.find(symbol);
        if (market_it == market_data_.end()) {
            return Result<TimeSeries<double>>::error(ErrorCode::InvalidSymbol, "No market data for symbol: " + symbol);
        }

        const auto& market_data = market_it->second;
        double current_capacity = initial_capacity;

        for (size_t i = 0; i < volume_series.size(); ++i) {
            double daily_volume = volume_series[i];

            // Update capacity based on volume changes
            if (market_data.average_daily_volume > 0) {
                double volume_ratio = daily_volume / market_data.average_daily_volume;
                current_capacity *= volume_ratio;  // Simplistic decay model
            }

            capacity_values.push_back(current_capacity);
        }

        return Result<TimeSeries<double>>::success(
            TimeSeries<double>{volume_series.timestamps(), capacity_values, symbol + "_capacity"});
    }

    /**
     * @brief Simulate trading impact on capacity
     */
    Result<std::vector<double>> simulate_trading_impact(const Symbol& symbol,
                                                        const transactions::TransactionSeries& transactions,
                                                        double initial_capacity) const {
        if (transactions.empty()) {
            return Result<std::vector<double>>::error(ErrorCode::InsufficientData,
                                                      "Transaction series cannot be empty");
        }

        auto market_it = market_data_.find(symbol);
        if (market_it == market_data_.end()) {
            return Result<std::vector<double>>::error(ErrorCode::InvalidSymbol, "No market data for symbol: " + symbol);
        }

        const auto& market_data = market_it->second;
        std::vector<double> capacity_impact;
        capacity_impact.reserve(transactions.size());

        double current_capacity = initial_capacity;

        for (const auto& txn : transactions) {
            if (txn.symbol() != symbol)
                continue;

            double trade_size         = std::abs(txn.shares());
            double participation_rate = trade_size / market_data.average_daily_volume;

            // Calculate capacity reduction due to trading
            double impact_factor = 1.0 - (participation_rate * 0.1);  // 10% reduction per 1% participation
            current_capacity *= std::max(0.1, impact_factor);         // Minimum 10% of original capacity

            capacity_impact.push_back(current_capacity);
        }

        return Result<std::vector<double>>::success(std::move(capacity_impact));
    }

  private:
    /**
     * @brief Optimize position size to meet constraints
     */
    std::optional<SecurityCapacityResult> optimize_position_size(const Symbol& symbol, double target_dollars,
                                                                 double current_price,
                                                                 const MarketMicrostructure& market_data) const {
        SecurityCapacityResult result;
        result.symbol = symbol;

        // Binary search for optimal position size
        double min_dollars     = 0.0;
        double max_dollars     = target_dollars;
        double optimal_dollars = 0.0;

        for (int iteration = 0; iteration < 20; ++iteration) {  // Max 20 iterations
            double test_dollars = (min_dollars + max_dollars) / 2.0;
            double test_shares  = test_dollars / current_price;

            // Check constraints
            double adv_participation = test_shares / market_data.average_daily_volume;
            double spread_cost       = market_data.calculate_spread_cost(test_shares, current_price);
            double impact_cost       = market_data.estimate_price_impact(test_shares, current_price) * test_shares;

            double spread_cost_bps = (spread_cost / test_dollars) * 10000.0;
            double impact_bps      = (impact_cost / test_dollars) * 10000.0;

            size_t trading_days = static_cast<size_t>(
                std::ceil(test_shares / (market_data.average_daily_volume * constraints_.max_single_day_volume)));

            if (constraints_.is_violated(adv_participation, spread_cost_bps, impact_bps, trading_days)) {
                max_dollars = test_dollars;
            } else {
                min_dollars     = test_dollars;
                optimal_dollars = test_dollars;
            }

            if (std::abs(max_dollars - min_dollars) < 1000.0) {  // $1000 precision
                break;
            }
        }

        if (optimal_dollars > 0) {
            result.max_position_dollars  = optimal_dollars;
            result.max_position_shares   = optimal_dollars / current_price;
            result.estimated_spread_cost = market_data.calculate_spread_cost(result.max_position_shares, current_price);
            result.estimated_impact_cost =
                market_data.estimate_price_impact(result.max_position_shares, current_price) *
                result.max_position_shares;
            result.total_trading_cost = result.estimated_spread_cost + result.estimated_impact_cost;
            result.binding_constraint = LiquidityConstraint::VolumePercent;

            return result;
        }

        return std::nullopt;
    }
};

/**
 * @brief Create market microstructure data from basic inputs
 */
inline MarketMicrostructure create_market_microstructure(const Symbol& symbol, double avg_daily_volume,
                                                         double market_cap, double current_price,
                                                         double typical_spread_bps = 10.0, double volatility = 0.20) {
    MarketMicrostructure data;
    data.symbol                = symbol;
    data.average_daily_volume  = avg_daily_volume;
    data.average_dollar_volume = avg_daily_volume * current_price;
    data.market_cap            = market_cap;
    data.typical_spread_bps    = typical_spread_bps;
    data.volatility            = volatility;

    // Estimate price impact coefficient based on market cap and volume
    // Larger, more liquid stocks have lower impact
    double liquidity_factor       = std::log(data.average_dollar_volume / 1000000.0);  // Log of ADV in millions
    data.price_impact_coefficient = 0.1 / std::max(1.0, liquidity_factor);

    return data;
}

/**
 * @brief Calculate portfolio turnover capacity
 */
inline Result<double> calculate_turnover_capacity(const PortfolioCapacityResult& capacity_result,
                                                  double target_turnover, double portfolio_value) {
    if (target_turnover <= 0.0) {
        return Result<double>::error(ErrorCode::InvalidInput, "Target turnover must be positive");
    }

    double total_trading_capacity = 0.0;
    for (const auto& [symbol, result] : capacity_result.security_results) {
        total_trading_capacity += result.max_daily_trade_dollars;
    }

    if (total_trading_capacity <= 0.0) {
        return Result<double>::success(0.0);
    }

    // Annual turnover capacity = daily capacity * trading days per year
    double annual_trading_capacity = total_trading_capacity * 252.0;
    double max_supportable_aum     = annual_trading_capacity / target_turnover;

    return Result<double>::success(max_supportable_aum);
}

}  // namespace pyfolio::capacity