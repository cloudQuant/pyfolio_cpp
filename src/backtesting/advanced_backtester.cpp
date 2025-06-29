#include "pyfolio/backtesting/advanced_backtester.h"
#include "pyfolio/analytics/statistics.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace pyfolio::backtesting {

Result<BacktestResults> AdvancedBacktester::run_backtest() {
    if (!strategy_) {
        return Result<BacktestResults>::error(ErrorCode::InvalidInput,
            "No trading strategy set");
    }
    
    if (price_data_.empty()) {
        return Result<BacktestResults>::error(ErrorCode::InsufficientData,
            "No price data loaded");
    }
    
    // Initialize strategy
    strategy_->initialize(config_);
    
    // Get all trading dates
    std::vector<DateTime> trading_dates;
    for (const auto& [symbol, prices] : price_data_) {
        const auto& dates = prices.timestamps();
        for (const auto& date : dates) {
            if (date >= config_.start_date && date <= config_.end_date) {
                trading_dates.push_back(date);
            }
        }
    }
    
    // Remove duplicates and sort
    std::sort(trading_dates.begin(), trading_dates.end());
    trading_dates.erase(std::unique(trading_dates.begin(), trading_dates.end()),
                       trading_dates.end());
    
    if (trading_dates.empty()) {
        return Result<BacktestResults>::error(ErrorCode::InsufficientData,
            "No trading dates in specified period");
    }
    
    // Initialize portfolio tracking
    std::vector<DateTime> portfolio_dates;
    std::vector<double> portfolio_values;
    std::vector<double> returns;
    
    // Run backtest for each trading date
    for (size_t i = 0; i < trading_dates.size(); ++i) {
        const auto& timestamp = trading_dates[i];
        
        // Get current market prices
        auto prices = get_prices_at(timestamp);
        if (prices.empty()) {
            continue; // Skip if no price data available
        }
        
        // Update portfolio value with current prices
        current_state_.update_value(prices);
        
        // Execute trading period
        auto result = execute_period(timestamp, prices);
        if (result.is_error()) {
            // Log error but continue backtesting
            continue;
        }
        
        // Record portfolio state
        portfolio_dates.push_back(timestamp);
        portfolio_values.push_back(current_state_.total_value);
        
        // Calculate return
        if (i > 0 && !portfolio_values.empty()) {
            double prev_value = portfolio_values[portfolio_values.size() - 2];
            double current_return = (current_state_.total_value - prev_value) / prev_value;
            returns.push_back(current_return);
        }
    }
    
    // Finalize strategy
    strategy_->finalize();
    
    // Create time series for results
    auto portfolio_ts_result = TimeSeries<double>::create(portfolio_dates, portfolio_values);
    if (portfolio_ts_result.is_error()) {
        return Result<BacktestResults>::error(portfolio_ts_result.error().code,
            "Failed to create portfolio time series: " + portfolio_ts_result.error().message);
    }
    
    portfolio_values_ = portfolio_ts_result.value();
    
    // Create returns time series (one less element than values)
    if (returns.size() == portfolio_dates.size() - 1) {
        std::vector<DateTime> return_dates(portfolio_dates.begin() + 1, portfolio_dates.end());
        auto returns_ts_result = TimeSeries<double>::create(return_dates, returns);
        if (returns_ts_result.is_ok()) {
            portfolio_returns_ = returns_ts_result.value();
        }
    }
    
    // Calculate final results
    BacktestResults results;
    results.start_date = config_.start_date;
    results.end_date = config_.end_date;
    results.initial_capital = config_.initial_capital;
    results.final_value = current_state_.total_value;
    
    // Performance metrics
    results.performance = calculate_performance_metrics();
    
    // Transaction cost analysis
    results.total_commission = total_commission_;
    results.total_market_impact = total_market_impact_;
    results.total_slippage = total_slippage_;
    results.total_transaction_costs = total_commission_ + total_market_impact_ + total_slippage_;
    
    double total_return = results.final_value - results.initial_capital;
    results.transaction_cost_ratio = total_return != 0 ? 
        results.total_transaction_costs / std::abs(total_return) : 0.0;
    
    // Trade statistics
    results.total_trades = trade_history_.size();
    if (!trade_history_.empty()) {
        double total_trade_value = 0.0;
        for (const auto& trade : trade_history_) {
            total_trade_value += std::abs(static_cast<double>(trade.quantity) * trade.execution_price);
        }
        results.average_trade_size = total_trade_value / trade_history_.size();
        results.turnover_rate = total_trade_value / (results.initial_capital * 2); // Annualized
    }
    
    // Risk metrics
    results.max_drawdown = results.performance.max_drawdown;
    results.sharpe_ratio = results.performance.sharpe_ratio;
    results.sortino_ratio = results.performance.sortino_ratio;
    results.calmar_ratio = results.performance.calmar_ratio;
    
    // Time series
    results.portfolio_values = portfolio_values_;
    results.returns = portfolio_returns_;
    results.trade_history = trade_history_;
    
    // Benchmark comparison
    if (!benchmark_prices_.empty()) {
        results.benchmark_symbol = config_.benchmark_symbol;
        results.benchmark_performance = calculate_benchmark_metrics();
        
        // Calculate relative metrics
        if (results.benchmark_performance.annual_volatility > 0) {
            double excess_return = results.performance.annual_return - results.benchmark_performance.annual_return;
            results.alpha = excess_return;
            results.information_ratio = excess_return / results.benchmark_performance.annual_volatility;
            results.tracking_error = results.benchmark_performance.annual_volatility;
            
            // Simple beta calculation (would need correlation for accuracy)
            results.beta = results.performance.annual_volatility / results.benchmark_performance.annual_volatility;
        }
    }
    
    return Result<BacktestResults>::success(std::move(results));
}

Result<void> AdvancedBacktester::execute_period(
    const DateTime& timestamp,
    const std::unordered_map<std::string, Price>& prices) {
    
    // Generate trading signals
    auto target_weights = strategy_->generate_signals(timestamp, prices, current_state_);
    
    // Execute trades to reach target portfolio
    return execute_trades(timestamp, prices, target_weights);
}

Result<void> AdvancedBacktester::execute_trades(
    const DateTime& timestamp,
    const std::unordered_map<std::string, Price>& prices,
    const std::unordered_map<std::string, double>& target_weights) {
    
    // Calculate required trades
    auto required_trades = calculate_required_trades(prices, target_weights);
    
    std::vector<ExecutedTrade> executed_trades;
    
    // Execute each required trade
    for (const auto& [symbol, quantity] : required_trades) {
        if (std::abs(quantity) < 1) continue; // Skip tiny trades
        
        auto price_it = prices.find(symbol);
        if (price_it == prices.end()) continue;
        
        Price market_price = price_it->second;
        double daily_volume = get_volume_at(symbol, timestamp);
        double volatility = get_volatility_at(symbol, timestamp);
        
        // Check liquidity constraints
        if (!config_.liquidity.is_trade_feasible(static_cast<double>(quantity), daily_volume)) {
            if (config_.enable_trade_splitting) {
                // Split large trades
                auto chunks = config_.liquidity.split_trade(static_cast<double>(quantity), daily_volume);
                
                for (double chunk : chunks) {
                    auto trade_result = execute_trade(timestamp, symbol, 
                                                    static_cast<Shares>(chunk), 
                                                    market_price, daily_volume, volatility);
                    if (trade_result.is_ok()) {
                        executed_trades.push_back(trade_result.value());
                    }
                }
            }
            continue;
        }
        
        // Execute trade
        auto trade_result = execute_trade(timestamp, symbol, quantity, 
                                        market_price, daily_volume, volatility);
        if (trade_result.is_ok()) {
            executed_trades.push_back(trade_result.value());
        }
    }
    
    // Update portfolio state
    update_portfolio_state(executed_trades);
    
    return Result<void>::success();
}

Result<ExecutedTrade> AdvancedBacktester::execute_trade(
    const DateTime& timestamp,
    const std::string& symbol,
    Shares quantity,
    Price market_price,
    double daily_volume,
    double volatility) {
    
    if (quantity == 0) {
        return Result<ExecutedTrade>::error(ErrorCode::InvalidInput,
            "Cannot execute zero quantity trade");
    }
    
    ExecutedTrade trade;
    trade.timestamp = timestamp;
    trade.symbol = symbol;
    trade.quantity = quantity;
    trade.market_price = market_price;
    trade.side = quantity > 0 ? TransactionSide::Buy : TransactionSide::Sell;
    trade.execution_algo = "Standard";
    
    // Calculate market impact
    double trade_value = std::abs(static_cast<double>(quantity)) * market_price;
    trade.market_impact = config_.market_impact.calculate_impact(
        static_cast<double>(quantity), daily_volume, volatility);
    
    // Calculate slippage
    trade.slippage = config_.slippage.calculate_slippage(
        static_cast<double>(quantity), volatility, rng_);
    
    // Calculate execution price including impact and slippage
    double price_adjustment = trade.market_impact + trade.slippage;
    if (quantity < 0) price_adjustment = -price_adjustment; // Sell gets worse price
    
    trade.execution_price = market_price * (1.0 + price_adjustment);
    
    // Calculate commission
    trade.commission = config_.commission.calculate_commission(trade_value, quantity);
    
    // Total transaction cost
    trade.total_cost = trade.commission + 
                      std::abs(trade_value * trade.market_impact) + 
                      std::abs(trade_value * trade.slippage);
    
    // Check if we have enough cash for buy orders
    if (quantity > 0) {
        double required_cash = trade_value + trade.total_cost;
        if (current_state_.cash < required_cash) {
            if (config_.enable_partial_fills) {
                // Scale down the trade
                double scale_factor = current_state_.cash / required_cash;
                quantity = static_cast<Shares>(quantity * scale_factor);
                if (quantity == 0) {
                    return Result<ExecutedTrade>::error(ErrorCode::InsufficientData,
                        "Insufficient cash for trade");
                }
                
                // Recalculate with scaled quantity
                trade.quantity = quantity;
                trade_value = std::abs(static_cast<double>(quantity)) * market_price;
                trade.commission = config_.commission.calculate_commission(trade_value, quantity);
                trade.total_cost = trade.commission + 
                                  std::abs(trade_value * trade.market_impact) + 
                                  std::abs(trade_value * trade.slippage);
            } else {
                return Result<ExecutedTrade>::error(ErrorCode::InsufficientData,
                    "Insufficient cash for trade");
            }
        }
    }
    
    // Update tracking
    total_commission_ += trade.commission;
    total_market_impact_ += std::abs(trade_value * trade.market_impact);
    total_slippage_ += std::abs(trade_value * trade.slippage);
    
    trade_history_.push_back(trade);
    
    return Result<ExecutedTrade>::success(trade);
}

std::unordered_map<std::string, Shares> AdvancedBacktester::calculate_required_trades(
    const std::unordered_map<std::string, Price>& prices,
    const std::unordered_map<std::string, double>& target_weights) const {
    
    std::unordered_map<std::string, Shares> required_trades;
    
    // Calculate target portfolio value (excluding cash buffer)
    double available_capital = current_state_.total_value * (1.0 - config_.cash_buffer);
    
    // Calculate current weights
    auto current_weights = current_state_.get_weights();
    
    // For each target weight, calculate required trade
    for (const auto& [symbol, target_weight] : target_weights) {
        if (target_weight < 0 || target_weight > config_.max_position_size) {
            continue; // Skip invalid weights
        }
        
        auto price_it = prices.find(symbol);
        if (price_it == prices.end()) continue;
        
        Price current_price = price_it->second;
        
        // Target position value
        double target_value = available_capital * target_weight;
        Shares target_shares = static_cast<Shares>(target_value / current_price);
        
        // Current position
        Shares current_shares = 0;
        auto pos_it = current_state_.positions.find(symbol);
        if (pos_it != current_state_.positions.end()) {
            current_shares = pos_it->second.shares;
        }
        
        // Required trade
        Shares required_quantity = target_shares - current_shares;
        
        if (std::abs(required_quantity) >= 1) { // Only trade if meaningful size
            required_trades[symbol] = required_quantity;
        }
    }
    
    // Handle positions not in target (liquidate)
    for (const auto& [symbol, position] : current_state_.positions) {
        if (target_weights.find(symbol) == target_weights.end() && position.shares != 0) {
            required_trades[symbol] = -position.shares; // Liquidate
        }
    }
    
    return required_trades;
}

void AdvancedBacktester::update_portfolio_state(const std::vector<ExecutedTrade>& executed_trades) {
    for (const auto& trade : executed_trades) {
        // Update cash
        double trade_value = static_cast<double>(trade.quantity) * trade.execution_price;
        current_state_.cash -= trade_value + trade.total_cost;
        
        // Update position
        auto& position = current_state_.positions[trade.symbol];
        position.shares += trade.quantity;
        position.price = trade.execution_price;
        position.timestamp = trade.timestamp.time_point();
        
        // Remove zero positions
        if (position.shares == 0) {
            current_state_.positions.erase(trade.symbol);
        }
        
        // Update total costs
        current_state_.total_commission += trade.commission;
        current_state_.total_market_impact += std::abs(trade_value * trade.market_impact);
        current_state_.total_slippage += std::abs(trade_value * trade.slippage);
    }
}

analytics::PerformanceMetrics AdvancedBacktester::calculate_performance_metrics() const {
    analytics::PerformanceMetrics metrics;
    
    if (portfolio_returns_.empty()) {
        return metrics; // Return empty metrics
    }
    
    // Basic calculations
    auto returns_vec = portfolio_returns_.values();
    
    // Calculate basic statistics
    double mean_return = std::accumulate(returns_vec.begin(), returns_vec.end(), 0.0) / returns_vec.size();
    
    double variance = 0.0;
    for (double ret : returns_vec) {
        variance += (ret - mean_return) * (ret - mean_return);
    }
    variance /= (returns_vec.size() - 1);
    double volatility = std::sqrt(variance);
    
    // Annualize (assuming daily returns)
    metrics.annual_return = mean_return * 252;
    metrics.annual_volatility = volatility * std::sqrt(252);
    
    // Sharpe ratio (assuming 0% risk-free rate)
    metrics.sharpe_ratio = metrics.annual_volatility > 0 ? 
        metrics.annual_return / metrics.annual_volatility : 0.0;
    
    // Calculate drawdown
    auto values_vec = portfolio_values_.values();
    double peak = values_vec[0];
    double max_dd = 0.0;
    
    for (double value : values_vec) {
        if (value > peak) peak = value;
        double drawdown = (peak - value) / peak;
        max_dd = std::max(max_dd, drawdown);
    }
    
    metrics.max_drawdown = max_dd;
    
    // Total return
    if (!values_vec.empty()) {
        metrics.total_return = (values_vec.back() / values_vec.front()) - 1.0;
    }
    
    // Sortino ratio (downside deviation)
    double downside_variance = 0.0;
    size_t downside_count = 0;
    for (double ret : returns_vec) {
        if (ret < 0) {
            downside_variance += ret * ret;
            downside_count++;
        }
    }
    
    if (downside_count > 0) {
        double downside_deviation = std::sqrt(downside_variance / downside_count) * std::sqrt(252);
        metrics.sortino_ratio = downside_deviation > 0 ? 
            metrics.annual_return / downside_deviation : 0.0;
    }
    
    // Calmar ratio
    metrics.calmar_ratio = metrics.max_drawdown > 0 ? 
        metrics.annual_return / metrics.max_drawdown : 0.0;
    
    return metrics;
}

analytics::PerformanceMetrics AdvancedBacktester::calculate_benchmark_metrics() const {
    analytics::PerformanceMetrics metrics;
    
    if (benchmark_prices_.empty()) {
        return metrics;
    }
    
    // Calculate benchmark returns
    auto benchmark_values = benchmark_prices_.values();
    std::vector<double> benchmark_returns;
    
    for (size_t i = 1; i < benchmark_values.size(); ++i) {
        double ret = (benchmark_values[i] - benchmark_values[i-1]) / benchmark_values[i-1];
        benchmark_returns.push_back(ret);
    }
    
    if (benchmark_returns.empty()) {
        return metrics;
    }
    
    // Calculate basic statistics
    double mean_return = std::accumulate(benchmark_returns.begin(), benchmark_returns.end(), 0.0) / benchmark_returns.size();
    
    double variance = 0.0;
    for (double ret : benchmark_returns) {
        variance += (ret - mean_return) * (ret - mean_return);
    }
    variance /= (benchmark_returns.size() - 1);
    double volatility = std::sqrt(variance);
    
    // Annualize
    metrics.annual_return = mean_return * 252;
    metrics.annual_volatility = volatility * std::sqrt(252);
    metrics.sharpe_ratio = metrics.annual_volatility > 0 ? 
        metrics.annual_return / metrics.annual_volatility : 0.0;
    
    // Total return
    metrics.total_return = (benchmark_values.back() / benchmark_values.front()) - 1.0;
    
    return metrics;
}

std::unordered_map<std::string, Price> AdvancedBacktester::get_prices_at(const DateTime& timestamp) const {
    std::unordered_map<std::string, Price> prices;
    
    for (const auto& [symbol, price_series] : price_data_) {
        // Simple linear search for matching timestamp
        const auto& timestamps = price_series.timestamps();
        const auto& values = price_series.values();
        
        for (size_t i = 0; i < timestamps.size(); ++i) {
            if (timestamps[i] == timestamp) {
                prices[symbol] = values[i];
                break;
            }
        }
    }
    
    return prices;
}

double AdvancedBacktester::get_volume_at(const std::string& symbol, const DateTime& timestamp) const {
    auto it = volume_data_.find(symbol);
    if (it != volume_data_.end()) {
        const auto& volume_series = it->second;
        const auto& timestamps = volume_series.timestamps();
        const auto& values = volume_series.values();
        
        for (size_t i = 0; i < timestamps.size(); ++i) {
            if (timestamps[i] == timestamp) {
                return values[i];
            }
        }
    }
    
    // Default volume if not available
    return 1000000.0; // 1M shares
}

double AdvancedBacktester::get_volatility_at(const std::string& symbol, const DateTime& timestamp) const {
    auto it = volatility_data_.find(symbol);
    if (it != volatility_data_.end()) {
        const auto& vol_series = it->second;
        const auto& timestamps = vol_series.timestamps();
        const auto& values = vol_series.values();
        
        for (size_t i = 0; i < timestamps.size(); ++i) {
            if (timestamps[i] == timestamp) {
                return values[i];
            }
        }
    }
    
    // Default volatility if not available
    return 0.02; // 2% daily volatility
}

}  // namespace pyfolio::backtesting