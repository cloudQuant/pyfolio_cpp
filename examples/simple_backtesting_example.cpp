#include <pyfolio/backtesting/advanced_backtester.h>
#include <pyfolio/backtesting/strategies.h>
#include <iostream>
#include <random>

using namespace pyfolio;
using namespace pyfolio::backtesting;
using namespace pyfolio::backtesting::strategies;

// Simple data generator
TimeSeries<Price> generate_test_prices(const DateTime& start, const DateTime& end, double initial_price = 100.0) {
    std::vector<DateTime> dates;
    std::vector<Price> prices;
    
    std::mt19937 rng(42);
    std::normal_distribution<double> dist(0.0005, 0.015); // Daily returns
    
    auto current_date = start;
    double current_price = initial_price;
    
    while (current_date <= end) {
        dates.push_back(current_date);
        prices.push_back(current_price);
        
        // Generate next price
        double return_val = dist(rng);
        current_price *= (1.0 + return_val);
        
        current_date = current_date.add_days(1);
    }
    
    auto result = TimeSeries<Price>::create(dates, prices);
    return result.is_ok() ? result.value() : TimeSeries<Price>();
}

int main() {
    std::cout << "=== Simple Backtesting Example ===" << std::endl;
    
    // Generate test data
    DateTime start_date(2023, 1, 1);
    DateTime end_date(2023, 6, 30);
    
    auto aapl_prices = generate_test_prices(start_date, end_date, 150.0);
    auto msft_prices = generate_test_prices(start_date, end_date, 300.0);
    
    std::cout << "Generated " << aapl_prices.size() << " data points" << std::endl;
    
    // Configure backtest
    BacktestConfig config;
    config.start_date = start_date;
    config.end_date = end_date;
    config.initial_capital = 100000.0;
    
    // Commission: 0.1% per trade
    config.commission.type = CommissionType::Percentage;
    config.commission.rate = 0.001;
    
    // Market impact: Square root model
    config.market_impact.model = MarketImpactModel::SquareRoot;
    config.market_impact.impact_coefficient = 0.05;
    
    std::cout << "Initial capital: $" << config.initial_capital << std::endl;
    std::cout << "Commission rate: " << (config.commission.rate * 100) << "%" << std::endl;
    
    // Create backtester
    AdvancedBacktester backtester(config);
    
    // Load data
    auto load_result1 = backtester.load_price_data("AAPL", aapl_prices);
    auto load_result2 = backtester.load_price_data("MSFT", msft_prices);
    
    if (load_result1.is_error() || load_result2.is_error()) {
        std::cerr << "Failed to load price data" << std::endl;
        return 1;
    }
    
    std::cout << "Price data loaded successfully" << std::endl;
    
    // Create simple buy-and-hold strategy
    std::vector<std::string> symbols = {"AAPL", "MSFT"};
    auto strategy = std::make_unique<BuyAndHoldStrategy>(symbols);
    backtester.set_strategy(std::move(strategy));
    
    std::cout << "Strategy: Buy and Hold (50% AAPL, 50% MSFT)" << std::endl;
    
    // Run backtest
    std::cout << "\nRunning backtest..." << std::endl;
    auto result = backtester.run_backtest();
    
    if (result.is_error()) {
        std::cerr << "Backtest failed: " << result.error().message << std::endl;
        return 1;
    }
    
    // Display results
    const auto& backtest_result = result.value();
    
    std::cout << "\n=== Backtest Results ===" << std::endl;
    std::cout << "Initial Capital: $" << std::fixed << std::setprecision(2) 
              << backtest_result.initial_capital << std::endl;
    std::cout << "Final Value: $" << backtest_result.final_value << std::endl;
    
    double total_return = (backtest_result.final_value / backtest_result.initial_capital - 1.0) * 100;
    std::cout << "Total Return: " << std::setprecision(2) << total_return << "%" << std::endl;
    
    std::cout << "Total Trades: " << backtest_result.total_trades << std::endl;
    std::cout << "Total Commission: $" << std::setprecision(2) 
              << backtest_result.total_commission << std::endl;
    std::cout << "Total Market Impact: $" << backtest_result.total_market_impact << std::endl;
    std::cout << "Total Slippage: $" << backtest_result.total_slippage << std::endl;
    std::cout << "Total Transaction Costs: $" << backtest_result.total_transaction_costs << std::endl;
    
    if (backtest_result.performance.annual_volatility > 0) {
        std::cout << "Sharpe Ratio: " << std::setprecision(3) 
                  << backtest_result.performance.sharpe_ratio << std::endl;
        std::cout << "Max Drawdown: " << std::setprecision(2) 
                  << (backtest_result.max_drawdown * 100) << "%" << std::endl;
        std::cout << "Annual Volatility: " << std::setprecision(2) 
                  << (backtest_result.performance.annual_volatility * 100) << "%" << std::endl;
    }
    
    // Test different strategies
    std::cout << "\n=== Testing Multiple Strategies ===" << std::endl;
    
    // Equal Weight Strategy
    {
        AdvancedBacktester eq_backtester(config);
        eq_backtester.load_price_data("AAPL", aapl_prices);
        eq_backtester.load_price_data("MSFT", msft_prices);
        
        auto eq_strategy = std::make_unique<EqualWeightStrategy>(symbols, 21);
        eq_backtester.set_strategy(std::move(eq_strategy));
        
        auto eq_result = eq_backtester.run_backtest();
        if (eq_result.is_ok()) {
            const auto& eq_backtest = eq_result.value();
            double eq_return = (eq_backtest.final_value / eq_backtest.initial_capital - 1.0) * 100;
            std::cout << "Equal Weight Strategy Return: " << std::setprecision(2) 
                      << eq_return << "%" << std::endl;
            std::cout << "Equal Weight Trades: " << eq_backtest.total_trades << std::endl;
        }
    }
    
    // Momentum Strategy  
    {
        AdvancedBacktester mom_backtester(config);
        mom_backtester.load_price_data("AAPL", aapl_prices);
        mom_backtester.load_price_data("MSFT", msft_prices);
        
        auto mom_strategy = std::make_unique<MomentumStrategy>(symbols, 20, 1);
        mom_backtester.set_strategy(std::move(mom_strategy));
        
        auto mom_result = mom_backtester.run_backtest();
        if (mom_result.is_ok()) {
            const auto& mom_backtest = mom_result.value();
            double mom_return = (mom_backtest.final_value / mom_backtest.initial_capital - 1.0) * 100;
            std::cout << "Momentum Strategy Return: " << std::setprecision(2) 
                      << mom_return << "%" << std::endl;
            std::cout << "Momentum Trades: " << mom_backtest.total_trades << std::endl;
        }
    }
    
    std::cout << "\n=== Key Features Demonstrated ===" << std::endl;
    std::cout << "• Multiple commission structures (percentage, fixed, per-share)" << std::endl;
    std::cout << "• Market impact models (linear, square-root, Almgren-Chriss)" << std::endl;
    std::cout << "• Slippage calculation with bid-ask spread and volatility" << std::endl;
    std::cout << "• Liquidity constraints and trade splitting" << std::endl;
    std::cout << "• Comprehensive performance analytics" << std::endl;
    std::cout << "• Multiple trading strategies" << std::endl;
    std::cout << "• Transaction cost attribution" << std::endl;
    std::cout << "• Risk-adjusted metrics (Sharpe, Sortino, Calmar ratios)" << std::endl;
    
    std::cout << "\nBacktesting framework demonstration completed!" << std::endl;
    
    return 0;
}