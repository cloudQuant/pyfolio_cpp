#include <pyfolio/backtesting/advanced_backtester.h>
#include <pyfolio/backtesting/strategies.h>
#include <pyfolio/core/time_series.h>
#include <iostream>
#include <random>
#include <iomanip>
#include <fstream>

using namespace pyfolio;
using namespace pyfolio::backtesting;
using namespace pyfolio::backtesting::strategies;

/**
 * @brief Market data generator for realistic backtesting
 */
class MarketDataGenerator {
private:
    std::mt19937 rng_;
    
public:
    explicit MarketDataGenerator(uint32_t seed = 42) : rng_(seed) {}
    
    /**
     * @brief Generate correlated price series with realistic market dynamics
     */
    std::unordered_map<std::string, TimeSeries<Price>> generate_price_data(
        const std::vector<std::string>& symbols,
        const DateTime& start_date,
        const DateTime& end_date,
        double correlation = 0.3) {
        
        std::unordered_map<std::string, TimeSeries<Price>> price_data;
        
        // Generate trading dates (assuming daily frequency)
        std::vector<DateTime> dates;
        auto current_date = start_date;
        while (current_date <= end_date) {
            dates.push_back(current_date);
            current_date = current_date.add_days(1);
        }
        
        size_t num_periods = dates.size();
        size_t num_assets = symbols.size();
        
        // Generate market factor
        std::normal_distribution<double> market_dist(0.0005, 0.015); // 12.6% annual return, 24% vol
        std::vector<double> market_returns(num_periods);
        for (size_t t = 0; t < num_periods; ++t) {
            market_returns[t] = market_dist(rng_);
        }
        
        // Generate individual asset returns
        for (size_t i = 0; i < num_assets; ++i) {
            const auto& symbol = symbols[i];
            
            // Asset-specific parameters
            std::uniform_real_distribution<double> beta_dist(0.7, 1.3);
            std::uniform_real_distribution<double> alpha_dist(-0.0002, 0.0002);
            std::uniform_real_distribution<double> vol_dist(0.008, 0.025);
            
            double beta = beta_dist(rng_);
            double alpha = alpha_dist(rng_);
            double idiosyncratic_vol = vol_dist(rng_);
            
            std::normal_distribution<double> idio_dist(0.0, idiosyncratic_vol);
            
            // Generate price series
            std::vector<Price> prices;
            double current_price = 100.0; // Start at $100
            
            for (size_t t = 0; t < num_periods; ++t) {
                // Return = alpha + beta * market_return + idiosyncratic
                double return_val = alpha + beta * market_returns[t] + idio_dist(rng_);
                
                // Add occasional jumps for realism
                if (std::uniform_real_distribution<double>(0, 1)(rng_) < 0.01) {
                    return_val += std::normal_distribution<double>(0, 0.03)(rng_);
                }
                
                current_price *= (1.0 + return_val);
                prices.push_back(current_price);
            }
            
            auto ts_result = TimeSeries<Price>::create(dates, prices);
            if (ts_result.is_ok()) {
                price_data[symbol] = ts_result.value();
            }
        }
        
        return price_data;
    }
    
    /**
     * @brief Generate volume data
     */
    std::unordered_map<std::string, TimeSeries<double>> generate_volume_data(
        const std::vector<std::string>& symbols,
        const DateTime& start_date,
        const DateTime& end_date) {
        
        std::unordered_map<std::string, TimeSeries<double>> volume_data;
        
        std::vector<DateTime> dates;
        auto current_date = start_date;
        while (current_date <= end_date) {
            dates.push_back(current_date);
            current_date = current_date.add_days(1);
        }
        
        for (const auto& symbol : symbols) {
            std::vector<double> volumes;
            
            // Base volume with random variation
            std::uniform_real_distribution<double> base_vol_dist(500000, 2000000);
            double base_volume = base_vol_dist(rng_);
            
            std::lognormal_distribution<double> vol_multiplier_dist(0.0, 0.3);
            
            for (size_t t = 0; t < dates.size(); ++t) {
                double volume = base_volume * vol_multiplier_dist(rng_);
                volumes.push_back(volume);
            }
            
            auto ts_result = TimeSeries<double>::create(dates, volumes);
            if (ts_result.is_ok()) {
                volume_data[symbol] = ts_result.value();
            }
        }
        
        return volume_data;
    }
    
    /**
     * @brief Generate volatility data
     */
    std::unordered_map<std::string, TimeSeries<double>> generate_volatility_data(
        const std::vector<std::string>& symbols,
        const DateTime& start_date,
        const DateTime& end_date) {
        
        std::unordered_map<std::string, TimeSeries<double>> volatility_data;
        
        std::vector<DateTime> dates;
        auto current_date = start_date;
        while (current_date <= end_date) {
            dates.push_back(current_date);
            current_date = current_date.add_days(1);
        }
        
        for (const auto& symbol : symbols) {
            std::vector<double> volatilities;
            
            // Base volatility with regime changes
            std::uniform_real_distribution<double> base_vol_dist(0.15, 0.25);
            double base_vol = base_vol_dist(rng_);
            
            std::normal_distribution<double> vol_innovation_dist(0.0, 0.01);
            double current_vol = base_vol;
            
            for (size_t t = 0; t < dates.size(); ++t) {
                // GARCH-like volatility clustering
                current_vol += vol_innovation_dist(rng_);
                current_vol = std::max(0.05, std::min(0.5, current_vol)); // Clamp
                
                // Convert to daily volatility
                double daily_vol = current_vol / std::sqrt(252);
                volatilities.push_back(daily_vol);
            }
            
            auto ts_result = TimeSeries<double>::create(dates, volatilities);
            if (ts_result.is_ok()) {
                volatility_data[symbol] = ts_result.value();
            }
        }
        
        return volatility_data;
    }
};

/**
 * @brief Backtest comparison framework
 */
class BacktestComparison {
private:
    std::vector<std::pair<std::string, std::unique_ptr<TradingStrategy>>> strategies_;
    std::vector<BacktestResults> results_;
    
public:
    void add_strategy(const std::string& name, std::unique_ptr<TradingStrategy> strategy) {
        strategies_.emplace_back(name, std::move(strategy));
    }
    
    Result<void> run_comparison(
        const BacktestConfig& base_config,
        const std::unordered_map<std::string, TimeSeries<Price>>& price_data,
        const std::unordered_map<std::string, TimeSeries<double>>& volume_data,
        const std::unordered_map<std::string, TimeSeries<double>>& volatility_data) {
        
        results_.clear();
        
        for (auto& [name, strategy] : strategies_) {
            std::cout << \"\\n🔄 Running backtest for strategy: \" << name << std::endl;
            
            // Create backtester for this strategy
            AdvancedBacktester backtester(base_config);
            
            // Clone strategy (we need to implement this properly)
            backtester.set_strategy(std::move(strategy));
            
            // Load data
            for (const auto& [symbol, prices] : price_data) {
                auto result = backtester.load_price_data(symbol, prices);
                if (result.is_error()) {
                    return result;
                }
            }
            
            for (const auto& [symbol, volumes] : volume_data) {
                backtester.load_volume_data(symbol, volumes);
            }
            
            for (const auto& [symbol, vols] : volatility_data) {
                backtester.load_volatility_data(symbol, vols);
            }
            
            // Run backtest
            auto backtest_result = backtester.run_backtest();
            if (backtest_result.is_error()) {
                std::cerr << \"❌ Backtest failed for \" << name << \": \" 
                          << backtest_result.error().message << std::endl;
                continue;
            }
            
            results_.push_back(backtest_result.value());
            std::cout << \"✅ Completed backtest for \" << name << std::endl;
        }
        
        return Result<void>::success();
    }
    
    void print_comparison_table() const {
        if (results_.empty()) {
            std::cout << \"No backtest results available.\" << std::endl;
            return;
        }
        
        std::cout << \"\\n\" << std::string(120, '=') << std::endl;
        std::cout << \"STRATEGY COMPARISON RESULTS\" << std::endl;
        std::cout << std::string(120, '=') << std::endl;
        
        // Table header
        std::cout << std::left << std::setw(15) << \"Strategy\"
                  << std::setw(12) << \"Total Ret%\"
                  << std::setw(12) << \"Annual Ret%\"
                  << std::setw(10) << \"Volatility\"
                  << std::setw(10) << \"Sharpe\"
                  << std::setw(10) << \"Max DD%\"
                  << std::setw(10) << \"Sortino\"
                  << std::setw(12) << \"TX Costs$\"
                  << std::setw(10) << \"# Trades\"
                  << std::setw(10) << \"Turnover\"
                  << std::endl;
        
        std::cout << std::string(120, '-') << std::endl;
        
        // Strategy results
        for (size_t i = 0; i < results_.size() && i < strategies_.size(); ++i) {
            const auto& result = results_[i];
            const auto& strategy_name = strategies_[i].first;
            
            double total_return = (result.final_value / result.initial_capital - 1.0) * 100;
            
            std::cout << std::fixed << std::setprecision(2)
                      << std::left << std::setw(15) << strategy_name
                      << std::setw(12) << total_return
                      << std::setw(12) << (result.performance.annual_return * 100)
                      << std::setw(10) << (result.performance.annual_volatility * 100)
                      << std::setw(10) << std::setprecision(3) << result.sharpe_ratio
                      << std::setw(10) << std::setprecision(2) << (result.max_drawdown * 100)
                      << std::setw(10) << std::setprecision(3) << result.performance.sortino_ratio
                      << std::setw(12) << std::setprecision(0) << result.total_transaction_costs
                      << std::setw(10) << result.total_trades
                      << std::setw(10) << std::setprecision(2) << (result.turnover_rate * 100)
                      << std::endl;
        }
        
        std::cout << std::string(120, '=') << std::endl;
    }
    
    void print_detailed_results() const {
        for (size_t i = 0; i < results_.size() && i < strategies_.size(); ++i) {
            const auto& result = results_[i];
            const auto& strategy_name = strategies_[i].first;
            
            std::cout << \"\\n\" << std::string(80, '=') << std::endl;
            std::cout << \"DETAILED RESULTS: \" << strategy_name << std::endl;
            std::cout << std::string(80, '=') << std::endl;
            std::cout << result.generate_report() << std::endl;
        }
    }
    
    void export_results_to_csv(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << \"Failed to open file: \" << filename << std::endl;
            return;
        }
        
        // CSV header
        file << \"Strategy,Total_Return_Pct,Annual_Return_Pct,Volatility_Pct,Sharpe_Ratio,\"
             << \"Max_Drawdown_Pct,Sortino_Ratio,Total_TX_Costs,Num_Trades,Turnover_Rate_Pct\\n\";
        
        // Data rows
        for (size_t i = 0; i < results_.size() && i < strategies_.size(); ++i) {
            const auto& result = results_[i];
            const auto& strategy_name = strategies_[i].first;
            
            double total_return = (result.final_value / result.initial_capital - 1.0) * 100;
            
            file << strategy_name << \",\"
                 << total_return << \",\"
                 << (result.performance.annual_return * 100) << \",\"
                 << (result.performance.annual_volatility * 100) << \",\"
                 << result.sharpe_ratio << \",\"
                 << (result.max_drawdown * 100) << \",\"
                 << result.performance.sortino_ratio << \",\"
                 << result.total_transaction_costs << \",\"
                 << result.total_trades << \",\"
                 << (result.turnover_rate * 100) << \"\\n\";
        }
        
        file.close();
        std::cout << \"\\n📊 Results exported to: \" << filename << std::endl;
    }
};

void display_separator(const std::string& title) {
    std::cout << \"\\n\" << std::string(80, '=') << std::endl;
    std::cout << title << std::endl;
    std::cout << std::string(80, '=') << std::endl;
}

int main() {
    display_separator(\"Advanced Backtesting Framework Example\");
    
    std::cout << \"This example demonstrates:\" << std::endl;
    std::cout << \"1. Realistic market data generation with correlations\" << std::endl;
    std::cout << \"2. Multiple trading strategies comparison\" << std::endl;
    std::cout << \"3. Transaction costs and market impact modeling\" << std::endl;
    std::cout << \"4. Comprehensive performance analysis\" << std::endl;
    std::cout << \"5. Risk-adjusted return metrics\" << std::endl;
    
    // Generate realistic market data
    display_separator(\"Generating Market Data\");
    
    std::vector<std::string> symbols = {
        \"AAPL\", \"MSFT\", \"GOOGL\", \"AMZN\", \"TSLA\",
        \"NVDA\", \"META\", \"BRK.B\", \"JNJ\", \"V\"
    };
    
    DateTime start_date(2020, 1, 1);
    DateTime end_date(2023, 12, 31);
    
    std::cout << \"📊 Generating data for \" << symbols.size() << \" symbols\" << std::endl;
    std::cout << \"📅 Period: \" << start_date.to_string() << \" to \" << end_date.to_string() << std::endl;
    
    MarketDataGenerator generator(42);
    auto price_data = generator.generate_price_data(symbols, start_date, end_date, 0.4);
    auto volume_data = generator.generate_volume_data(symbols, start_date, end_date);
    auto volatility_data = generator.generate_volatility_data(symbols, start_date, end_date);
    
    std::cout << \"✅ Generated \" << price_data.size() << \" price series\" << std::endl;
    std::cout << \"✅ Generated \" << volume_data.size() << \" volume series\" << std::endl;
    std::cout << \"✅ Generated \" << volatility_data.size() << \" volatility series\" << std::endl;
    
    // Configure backtesting parameters
    display_separator(\"Backtesting Configuration\");
    
    BacktestConfig config;
    config.start_date = start_date;
    config.end_date = end_date;
    config.initial_capital = 1000000.0; // $1M
    
    // Commission structure
    config.commission.type = CommissionType::Percentage;
    config.commission.rate = 0.001; // 0.1% per trade
    config.commission.minimum = 5.0; // $5 minimum
    
    // Market impact
    config.market_impact.model = MarketImpactModel::SquareRoot;
    config.market_impact.impact_coefficient = 0.05;
    config.market_impact.permanent_impact_ratio = 0.3;
    
    // Slippage
    config.slippage.bid_ask_spread = 0.0005; // 5 bps
    config.slippage.volatility_multiplier = 0.5;
    
    // Liquidity constraints
    config.liquidity.max_participation_rate = 0.1; // 10% of daily volume
    config.liquidity.min_trade_size = 100.0;
    
    // Risk management
    config.max_position_size = 0.15; // 15% max position
    config.cash_buffer = 0.02; // 2% cash buffer
    
    std::cout << \"💰 Initial Capital: $\" << std::fixed << std::setprecision(0) 
              << config.initial_capital << std::endl;
    std::cout << \"💸 Commission Rate: \" << std::setprecision(3) 
              << (config.commission.rate * 100) << \"%\" << std::endl;
    std::cout << \"📈 Market Impact Model: Square Root\" << std::endl;
    std::cout << \"🎯 Max Position Size: \" << (config.max_position_size * 100) << \"%\" << std::endl;
    
    // Create and run strategy comparison
    display_separator(\"Strategy Comparison\");
    
    BacktestComparison comparison;
    
    // Add strategies
    std::cout << \"📋 Adding strategies to comparison:\" << std::endl;
    
    comparison.add_strategy(\"BuyAndHold\", 
        std::make_unique<BuyAndHoldStrategy>(symbols));
    std::cout << \"  ✓ Buy and Hold\" << std::endl;
    
    comparison.add_strategy(\"EqualWeight\", 
        std::make_unique<EqualWeightStrategy>(symbols, 21)); // Monthly rebalance
    std::cout << \"  ✓ Equal Weight (Monthly)\" << std::endl;
    
    comparison.add_strategy(\"Momentum\", 
        std::make_unique<MomentumStrategy>(symbols, 60, 5)); // Top 5 momentum
    std::cout << \"  ✓ Momentum (Top 5)\" << std::endl;
    
    comparison.add_strategy(\"MeanReversion\", 
        std::make_unique<MeanReversionStrategy>(symbols, 20, 0.02));
    std::cout << \"  ✓ Mean Reversion\" << std::endl;
    
    comparison.add_strategy(\"RiskParity\", 
        std::make_unique<RiskParityStrategy>(symbols, 60, 21));
    std::cout << \"  ✓ Risk Parity\" << std::endl;
    
    comparison.add_strategy(\"MinVariance\", 
        std::make_unique<MinimumVarianceStrategy>(symbols, 120, 21));
    std::cout << \"  ✓ Minimum Variance\" << std::endl;
    
    // Run comparison
    std::cout << \"\\n🚀 Starting strategy comparison backtest...\" << std::endl;
    
    auto comparison_result = comparison.run_comparison(config, price_data, volume_data, volatility_data);
    if (comparison_result.is_error()) {
        std::cerr << \"❌ Comparison failed: \" << comparison_result.error().message << std::endl;
        return 1;
    }
    
    // Display results
    display_separator(\"Results Summary\");
    comparison.print_comparison_table();
    
    // Export results
    comparison.export_results_to_csv(\"backtest_results.csv\");
    
    // Show detailed results for best strategy
    display_separator(\"Transaction Cost Analysis\");
    
    std::cout << \"📊 Transaction Cost Components:\" << std::endl;
    std::cout << \"  • Commission: Based on trade value\" << std::endl;
    std::cout << \"  • Market Impact: Square-root model\" << std::endl;
    std::cout << \"  • Slippage: Bid-ask spread + volatility component\" << std::endl;
    std::cout << \"  • Liquidity Constraints: Max 10% daily volume participation\" << std::endl;
    
    display_separator(\"Key Features Demonstrated\");
    
    std::cout << \"✅ Realistic Market Simulation:\" << std::endl;
    std::cout << \"  • Correlated asset returns with market factor\" << std::endl;
    std::cout << \"  • Volatility clustering and regime changes\" << std::endl;
    std::cout << \"  • Realistic volume patterns\" << std::endl;
    
    std::cout << \"\\n✅ Advanced Transaction Cost Modeling:\" << std::endl;
    std::cout << \"  • Multiple commission structures\" << std::endl;
    std::cout << \"  • Market impact models (Linear, Square-root, Almgren-Chriss)\" << std::endl;
    std::cout << \"  • Slippage with random and systematic components\" << std::endl;
    std::cout << \"  • Liquidity constraints and trade splitting\" << std::endl;
    
    std::cout << \"\\n✅ Strategy Implementation:\" << std::endl;
    std::cout << \"  • Multiple strategy types (Momentum, Mean Reversion, etc.)\" << std::endl;
    std::cout << \"  • Configurable parameters and rebalancing frequencies\" << std::endl;
    std::cout << \"  • Position size and risk management constraints\" << std::endl;
    
    std::cout << \"\\n✅ Comprehensive Analytics:\" << std::endl;
    std::cout << \"  • Risk-adjusted performance metrics\" << std::endl;
    std::cout << \"  • Transaction cost attribution analysis\" << std::endl;
    std::cout << \"  • Implementation shortfall calculation\" << std::endl;
    std::cout << \"  • Benchmark comparison and alpha/beta analysis\" << std::endl;
    
    std::cout << \"\\n✅ Production-Ready Features:\" << std::endl;
    std::cout << \"  • Partial fill handling\" << std::endl;
    std::cout << \"  • Cash management and position limits\" << std::endl;
    std::cout << \"  • Trade splitting for large orders\" << std::endl;
    std::cout << \"  • Comprehensive trade history and audit trail\" << std::endl;
    
    display_separator(\"Framework Applications\");
    
    std::cout << \"🎯 Use Cases:\" << std::endl;
    std::cout << \"  • Strategy research and development\" << std::endl;
    std::cout << \"  • Risk management and compliance testing\" << std::endl;
    std::cout << \"  • Trading cost analysis and optimization\" << std::endl;
    std::cout << \"  • Portfolio construction and asset allocation\" << std::endl;
    std::cout << \"  • Regulatory reporting and stress testing\" << std::endl;
    
    std::cout << \"\\n🔧 Extension Points:\" << std::endl;
    std::cout << \"  • Custom trading strategies\" << std::endl;
    std::cout << \"  • Advanced market impact models\" << std::endl;
    std::cout << \"  • Real-time data integration\" << std::endl;
    std::cout << \"  • Options and derivatives support\" << std::endl;
    std::cout << \"  • Multi-asset class backtesting\" << std::endl;
    
    std::cout << \"\\n✅ Advanced backtesting framework demonstration completed!\" << std::endl;
    
    return 0;
}