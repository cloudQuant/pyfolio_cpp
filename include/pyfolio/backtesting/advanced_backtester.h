#pragma once

#include "../core/error_handling.h"
#include "../core/types.h"
#include "../core/time_series.h"
#include "../analytics/performance_metrics.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

namespace pyfolio::backtesting {

/**
 * @brief Commission structure types
 */
enum class CommissionType {
    Fixed,           ///< Fixed commission per trade
    Percentage,      ///< Percentage of trade value
    PerShare,        ///< Commission per share traded
    Tiered          ///< Tiered commission based on volume
};

/**
 * @brief Commission structure configuration
 */
struct CommissionStructure {
    CommissionType type = CommissionType::Percentage;
    double rate = 0.001;                    ///< Primary rate (percentage, per-share, or fixed)
    double minimum = 1.0;                   ///< Minimum commission per trade
    double maximum = std::numeric_limits<double>::max(); ///< Maximum commission per trade
    
    // Tiered structure (for CommissionType::Tiered)
    std::vector<std::pair<double, double>> tiers; ///< Volume thresholds and rates
    
    /**
     * @brief Calculate commission for a trade
     */
    double calculate_commission(double trade_value, Shares quantity) const {
        double commission = 0.0;
        
        switch (type) {
            case CommissionType::Fixed:
                commission = rate;
                break;
            case CommissionType::Percentage:
                commission = trade_value * rate;
                break;
            case CommissionType::PerShare:
                commission = static_cast<double>(std::abs(quantity)) * rate;
                break;
            case CommissionType::Tiered:
                commission = calculate_tiered_commission(trade_value);
                break;
        }
        
        return std::clamp(commission, minimum, maximum);
    }
    
private:
    double calculate_tiered_commission(double trade_value) const {
        if (tiers.empty()) return trade_value * rate;
        
        for (const auto& [threshold, tier_rate] : tiers) {
            if (trade_value <= threshold) {
                return trade_value * tier_rate;
            }
        }
        
        // Use highest tier rate if above all thresholds
        return trade_value * tiers.back().second;
    }
};

/**
 * @brief Market impact model types
 */
enum class MarketImpactModel {
    None,           ///< No market impact
    Linear,         ///< Linear impact proportional to size
    SquareRoot,     ///< Square-root impact (more realistic)
    Almgren,        ///< Almgren-Chriss temporary impact model
    Custom          ///< User-defined impact function
};

/**
 * @brief Market impact configuration
 */
struct MarketImpactConfig {
    MarketImpactModel model = MarketImpactModel::SquareRoot;
    double impact_coefficient = 0.1;       ///< Base impact coefficient
    double permanent_impact_ratio = 0.3;   ///< Ratio of permanent to temporary impact
    double participation_rate = 0.1;       ///< Fraction of daily volume
    double volatility_scaling = 1.0;       ///< Volatility scaling factor
    
    // Custom impact function
    std::function<double(double, double, double)> custom_impact_fn;
    
    /**
     * @brief Calculate market impact for a trade
     * @param trade_size Signed trade size (positive for buy, negative for sell)
     * @param daily_volume Average daily volume
     * @param volatility Asset volatility
     * @return Market impact as fraction of price
     */
    double calculate_impact(double trade_size, double daily_volume, double volatility) const {
        if (model == MarketImpactModel::None) return 0.0;
        
        double participation = std::abs(trade_size) / daily_volume;
        double sign = trade_size > 0 ? 1.0 : -1.0;
        
        double impact = 0.0;
        
        switch (model) {
            case MarketImpactModel::Linear:
                impact = impact_coefficient * participation * volatility;
                break;
            case MarketImpactModel::SquareRoot:
                impact = impact_coefficient * std::sqrt(participation) * volatility;
                break;
            case MarketImpactModel::Almgren:
                // Almgren-Chriss temporary impact
                impact = impact_coefficient * std::pow(participation, 0.6) * volatility;
                break;
            case MarketImpactModel::Custom:
                if (custom_impact_fn) {
                    impact = custom_impact_fn(trade_size, daily_volume, volatility);
                }
                break;
            default:
                impact = 0.0;
        }
        
        return sign * impact * volatility_scaling;
    }
};

/**
 * @brief Slippage model configuration
 */
struct SlippageConfig {
    double bid_ask_spread = 0.001;          ///< Average bid-ask spread
    double price_impact_decay = 0.5;        ///< How quickly price impact decays
    double volatility_multiplier = 1.0;     ///< Multiplier for volatility-based slippage
    bool enable_random_slippage = true;     ///< Add random component to slippage
    double random_slippage_std = 0.0005;    ///< Standard deviation of random slippage
    
    /**
     * @brief Calculate slippage for a trade
     */
    double calculate_slippage(double trade_size, double volatility, std::mt19937& rng) const {
        double base_slippage = bid_ask_spread * 0.5; // Half spread
        
        // Add volatility-based component
        double vol_slippage = volatility * volatility_multiplier * std::abs(trade_size);
        
        // Add random component
        double random_component = 0.0;
        if (enable_random_slippage) {
            std::normal_distribution<double> dist(0.0, random_slippage_std);
            random_component = dist(rng);
        }
        
        return base_slippage + vol_slippage + random_component;
    }
};

/**
 * @brief Liquidity constraint configuration
 */
struct LiquidityConstraints {
    double max_participation_rate = 0.2;    ///< Maximum fraction of daily volume
    double min_trade_size = 1.0;            ///< Minimum trade size (shares)
    double max_trade_size = 1e6;            ///< Maximum trade size (shares)
    bool enforce_market_hours = true;       ///< Only trade during market hours
    double urgency_penalty = 0.001;         ///< Additional cost for urgent trades
    
    /**
     * @brief Check if a trade is feasible given liquidity constraints
     */
    bool is_trade_feasible(double trade_size, double daily_volume) const {
        double abs_size = std::abs(trade_size);
        
        if (abs_size < min_trade_size || abs_size > max_trade_size) {
            return false;
        }
        
        double participation = abs_size / daily_volume;
        return participation <= max_participation_rate;
    }
    
    /**
     * @brief Split large trades into smaller chunks
     */
    std::vector<double> split_trade(double trade_size, double daily_volume) const {
        std::vector<double> chunks;
        
        double max_chunk_size = daily_volume * max_participation_rate;
        double remaining = trade_size;
        double sign = trade_size > 0 ? 1.0 : -1.0;
        
        while (std::abs(remaining) > min_trade_size) {
            double chunk_size = std::min(std::abs(remaining), max_chunk_size) * sign;
            chunks.push_back(chunk_size);
            remaining -= chunk_size;
        }
        
        return chunks;
    }
};

/**
 * @brief Enhanced trade execution details
 */
struct ExecutedTrade {
    DateTime timestamp;
    std::string symbol;
    Shares quantity;
    Price execution_price;      ///< Actual execution price after impact/slippage
    Price market_price;         ///< Market price at time of trade
    double commission;          ///< Commission paid
    double market_impact;       ///< Market impact cost
    double slippage;           ///< Slippage cost
    double total_cost;         ///< Total transaction cost
    TransactionSide side;
    std::string execution_algo; ///< Execution algorithm used
    
    /**
     * @brief Calculate implementation shortfall
     */
    double implementation_shortfall() const {
        return (execution_price - market_price) * static_cast<double>(quantity);
    }
};

/**
 * @brief Backtesting configuration
 */
struct BacktestConfig {
    DateTime start_date;
    DateTime end_date;
    double initial_capital = 1000000.0;
    
    // Transaction cost configuration
    CommissionStructure commission;
    MarketImpactConfig market_impact;
    SlippageConfig slippage;
    LiquidityConstraints liquidity;
    
    // Execution settings
    bool enable_partial_fills = true;
    bool enable_trade_splitting = true;
    double cash_buffer = 0.05;              ///< Keep 5% cash buffer
    
    // Risk management
    double max_position_size = 0.1;         ///< Maximum position size (10% of portfolio)
    double max_daily_turnover = 1.0;        ///< Maximum daily turnover
    bool enable_stop_loss = false;
    double stop_loss_threshold = -0.05;     ///< 5% stop loss
    
    // Benchmark
    std::string benchmark_symbol = "SPY";
    
    // Random seed for reproducible results
    uint32_t random_seed = 42;
};

/**
 * @brief Portfolio state during backtesting
 */
struct PortfolioState {
    double cash = 0.0;
    std::unordered_map<std::string, Position> positions;
    std::unordered_map<std::string, double> pending_orders; ///< Symbol -> quantity
    double total_value = 0.0;
    double total_commission = 0.0;
    double total_market_impact = 0.0;
    double total_slippage = 0.0;
    
    /**
     * @brief Update portfolio value based on current prices
     */
    void update_value(const std::unordered_map<std::string, Price>& prices) {
        total_value = cash;
        for (const auto& [symbol, position] : positions) {
            auto price_it = prices.find(symbol);
            if (price_it != prices.end()) {
                total_value += static_cast<double>(position.shares) * price_it->second;
            }
        }
    }
    
    /**
     * @brief Get portfolio weights
     */
    std::unordered_map<std::string, double> get_weights() const {
        std::unordered_map<std::string, double> weights;
        if (total_value <= 0) return weights;
        
        for (const auto& [symbol, position] : positions) {
            double position_value = static_cast<double>(position.shares) * position.price;
            weights[symbol] = position_value / total_value;
        }
        
        return weights;
    }
};

/**
 * @brief Backtesting results and analytics
 */
struct BacktestResults {
    DateTime start_date;
    DateTime end_date;
    double initial_capital;
    double final_value;
    
    // Performance metrics
    analytics::PerformanceMetrics performance;
    
    // Transaction cost analysis
    double total_commission;
    double total_market_impact;
    double total_slippage;
    double total_transaction_costs;
    double transaction_cost_ratio; ///< Transaction costs / total return
    
    // Trade statistics
    size_t total_trades;
    double average_trade_size;
    double turnover_rate;
    
    // Risk metrics
    double max_drawdown;
    double sharpe_ratio;
    double sortino_ratio;
    double calmar_ratio;
    
    // Time series data
    TimeSeries<double> portfolio_values;
    TimeSeries<double> returns;
    TimeSeries<double> drawdowns;
    std::vector<ExecutedTrade> trade_history;
    
    // Benchmark comparison
    std::string benchmark_symbol;
    analytics::PerformanceMetrics benchmark_performance;
    double information_ratio;
    double alpha;
    double beta;
    double tracking_error;
    
    /**
     * @brief Calculate implementation shortfall
     */
    double calculate_implementation_shortfall() const {
        double total_shortfall = 0.0;
        for (const auto& trade : trade_history) {
            total_shortfall += trade.implementation_shortfall();
        }
        return total_shortfall;
    }
    
    /**
     * @brief Generate performance report
     */
    std::string generate_report() const {
        std::ostringstream report;
        
        report << "=== Backtest Results Summary ===" << std::endl;
        report << "Period: " << start_date.to_string() << " to " << end_date.to_string() << std::endl;
        report << "Initial Capital: $" << std::fixed << std::setprecision(2) << initial_capital << std::endl;
        report << "Final Value: $" << final_value << std::endl;
        report << "Total Return: " << ((final_value / initial_capital - 1) * 100) << "%" << std::endl;
        
        report << std::endl << "=== Performance Metrics ===" << std::endl;
        report << "Sharpe Ratio: " << std::setprecision(4) << sharpe_ratio << std::endl;
        report << "Max Drawdown: " << (max_drawdown * 100) << "%" << std::endl;
        report << "Volatility: " << (performance.annual_volatility * 100) << "%" << std::endl;
        
        report << std::endl << "=== Transaction Costs ===" << std::endl;
        report << "Total Commission: $" << std::setprecision(2) << total_commission << std::endl;
        report << "Total Market Impact: $" << total_market_impact << std::endl;
        report << "Total Slippage: $" << total_slippage << std::endl;
        report << "Total Transaction Costs: $" << total_transaction_costs << std::endl;
        report << "Transaction Cost Ratio: " << (transaction_cost_ratio * 100) << "%" << std::endl;
        
        report << std::endl << "=== Trade Statistics ===" << std::endl;
        report << "Total Trades: " << total_trades << std::endl;
        report << "Average Trade Size: $" << average_trade_size << std::endl;
        report << "Turnover Rate: " << (turnover_rate * 100) << "%" << std::endl;
        
        if (!benchmark_symbol.empty()) {
            report << std::endl << "=== Benchmark Comparison ===" << std::endl;
            report << "Benchmark: " << benchmark_symbol << std::endl;
            report << "Alpha: " << (alpha * 100) << "%" << std::endl;
            report << "Beta: " << beta << std::endl;
            report << "Information Ratio: " << information_ratio << std::endl;
            report << "Tracking Error: " << (tracking_error * 100) << "%" << std::endl;
        }
        
        return report.str();
    }
};

/**
 * @brief Strategy interface for backtesting
 */
class TradingStrategy {
public:
    virtual ~TradingStrategy() = default;
    
    /**
     * @brief Generate trading signals
     * @param timestamp Current timestamp
     * @param prices Current market prices
     * @param portfolio Current portfolio state
     * @return Map of symbol -> target weight (0-1)
     */
    virtual std::unordered_map<std::string, double> generate_signals(
        const DateTime& timestamp,
        const std::unordered_map<std::string, Price>& prices,
        const PortfolioState& portfolio) = 0;
    
    /**
     * @brief Initialize strategy (called once at start)
     */
    virtual void initialize(const BacktestConfig& config) {}
    
    /**
     * @brief Finalize strategy (called once at end)
     */
    virtual void finalize() {}
    
    /**
     * @brief Get strategy name
     */
    virtual std::string get_name() const = 0;
    
    /**
     * @brief Get strategy parameters
     */
    virtual std::unordered_map<std::string, double> get_parameters() const {
        return {};
    }
};

/**
 * @brief Advanced backtesting engine
 */
class AdvancedBacktester {
private:
    BacktestConfig config_;
    std::unique_ptr<TradingStrategy> strategy_;
    std::mt19937 rng_;
    
    // Market data
    std::unordered_map<std::string, TimeSeries<Price>> price_data_;
    std::unordered_map<std::string, TimeSeries<double>> volume_data_;
    std::unordered_map<std::string, TimeSeries<double>> volatility_data_;
    
    // Benchmark data
    TimeSeries<Price> benchmark_prices_;
    
    // State tracking
    PortfolioState current_state_;
    std::vector<ExecutedTrade> trade_history_;
    TimeSeries<double> portfolio_values_;
    TimeSeries<double> portfolio_returns_;
    
    // Performance tracking
    double total_commission_ = 0.0;
    double total_market_impact_ = 0.0;
    double total_slippage_ = 0.0;
    
public:
    explicit AdvancedBacktester(const BacktestConfig& config)
        : config_(config), rng_(config.random_seed) {
        current_state_.cash = config.initial_capital;
        current_state_.total_value = config.initial_capital;
    }
    
    /**
     * @brief Set trading strategy
     */
    void set_strategy(std::unique_ptr<TradingStrategy> strategy) {
        strategy_ = std::move(strategy);
    }
    
    /**
     * @brief Load market data
     */
    Result<void> load_price_data(const std::string& symbol, 
                                const TimeSeries<Price>& prices) {
        if (prices.empty()) {
            return Result<void>::error(ErrorCode::InsufficientData,
                "Price data is empty for symbol: " + symbol);
        }
        
        price_data_[symbol] = prices;
        return Result<void>::success();
    }
    
    /**
     * @brief Load volume data
     */
    Result<void> load_volume_data(const std::string& symbol,
                                 const TimeSeries<double>& volumes) {
        volume_data_[symbol] = volumes;
        return Result<void>::success();
    }
    
    /**
     * @brief Load volatility data
     */
    Result<void> load_volatility_data(const std::string& symbol,
                                     const TimeSeries<double>& volatility) {
        volatility_data_[symbol] = volatility;
        return Result<void>::success();
    }
    
    /**
     * @brief Load benchmark data
     */
    Result<void> load_benchmark_data(const TimeSeries<Price>& benchmark_prices) {
        benchmark_prices_ = benchmark_prices;
        return Result<void>::success();
    }
    
    /**
     * @brief Run backtest
     */
    Result<BacktestResults> run_backtest();
    
    /**
     * @brief Get current portfolio state
     */
    const PortfolioState& get_portfolio_state() const {
        return current_state_;
    }
    
    /**
     * @brief Get trade history
     */
    const std::vector<ExecutedTrade>& get_trade_history() const {
        return trade_history_;
    }
    
private:
    /**
     * @brief Execute a single trading period
     */
    Result<void> execute_period(const DateTime& timestamp,
                               const std::unordered_map<std::string, Price>& prices);
    
    /**
     * @brief Execute trades for target portfolio
     */
    Result<void> execute_trades(const DateTime& timestamp,
                               const std::unordered_map<std::string, Price>& prices,
                               const std::unordered_map<std::string, double>& target_weights);
    
    /**
     * @brief Execute a single trade with transaction costs
     */
    Result<ExecutedTrade> execute_trade(const DateTime& timestamp,
                                       const std::string& symbol,
                                       Shares quantity,
                                       Price market_price,
                                       double daily_volume,
                                       double volatility);
    
    /**
     * @brief Calculate required trades to reach target portfolio
     */
    std::unordered_map<std::string, Shares> calculate_required_trades(
        const std::unordered_map<std::string, Price>& prices,
        const std::unordered_map<std::string, double>& target_weights) const;
    
    /**
     * @brief Update portfolio state after trades
     */
    void update_portfolio_state(const std::vector<ExecutedTrade>& executed_trades);
    
    /**
     * @brief Calculate performance metrics
     */
    analytics::PerformanceMetrics calculate_performance_metrics() const;
    
    /**
     * @brief Calculate benchmark metrics
     */
    analytics::PerformanceMetrics calculate_benchmark_metrics() const;
    
    /**
     * @brief Get market data for timestamp
     */
    std::unordered_map<std::string, Price> get_prices_at(const DateTime& timestamp) const;
    double get_volume_at(const std::string& symbol, const DateTime& timestamp) const;
    double get_volatility_at(const std::string& symbol, const DateTime& timestamp) const;
};

}  // namespace pyfolio::backtesting