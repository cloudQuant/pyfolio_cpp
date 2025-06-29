#!/usr/bin/env python3
"""
PyFolio C++ Basic Usage Example

This example demonstrates the core functionality of PyFolio C++
including time series creation, performance metrics calculation,
and basic backtesting capabilities.
"""

import sys
import os
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from datetime import datetime, timedelta

# Add the parent directory to path to import pyfolio_cpp
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

try:
    import pyfolio_cpp as pf
    print(f"PyFolio C++ version: {pf.version()}")
except ImportError as e:
    print(f"Error importing PyFolio C++: {e}")
    print("Please build the library first: cd python && pip install -e .")
    sys.exit(1)

def create_sample_portfolio_data():
    """Create sample portfolio data for demonstration"""
    print("Creating sample portfolio data...")
    
    # Create date range
    start_date = pf.DateTime(2023, 1, 1)
    end_date = pf.DateTime(2023, 12, 31)
    
    # Create sample return series with different characteristics
    portfolio_returns = pf.create_sample_data(start_date, end_date, 0.0, 0.015)
    benchmark_returns = pf.create_sample_data(start_date, end_date, 0.0, 0.012)
    
    print(f"Created {portfolio_returns.size()} data points")
    return portfolio_returns, benchmark_returns, start_date, end_date

def demonstrate_performance_analytics():
    """Demonstrate performance analytics capabilities"""
    print("\n=== Performance Analytics Demo ===")
    
    portfolio_returns, benchmark_returns, start_date, end_date = create_sample_portfolio_data()
    
    # Calculate comprehensive performance metrics
    print("Calculating performance metrics...")
    portfolio_metrics = pf.analytics.calculate_performance_metrics(portfolio_returns)
    benchmark_metrics = pf.analytics.calculate_performance_metrics(benchmark_returns)
    
    print(f"\nPortfolio Performance:")
    print(f"  Total Return: {portfolio_metrics.total_return:.3f}")
    print(f"  Annual Return: {portfolio_metrics.annual_return:.3f}")
    print(f"  Annual Volatility: {portfolio_metrics.annual_volatility:.3f}")
    print(f"  Sharpe Ratio: {portfolio_metrics.sharpe_ratio:.3f}")
    print(f"  Sortino Ratio: {portfolio_metrics.sortino_ratio:.3f}")
    print(f"  Calmar Ratio: {portfolio_metrics.calmar_ratio:.3f}")
    print(f"  Max Drawdown: {portfolio_metrics.max_drawdown:.3f}")
    print(f"  Win Rate: {portfolio_metrics.win_rate:.3f}")
    print(f"  Profit Factor: {portfolio_metrics.profit_factor:.3f}")
    
    print(f"\nBenchmark Performance:")
    print(f"  Sharpe Ratio: {benchmark_metrics.sharpe_ratio:.3f}")
    print(f"  Max Drawdown: {benchmark_metrics.max_drawdown:.3f}")
    
    # Calculate specific risk metrics
    print("\nRisk Analytics:")
    var_95 = pf.analytics.calculate_var(portfolio_returns, 0.05)
    var_99 = pf.analytics.calculate_var(portfolio_returns, 0.01)
    cvar_95 = pf.analytics.calculate_cvar(portfolio_returns, 0.05)
    
    print(f"  VaR (95%): {var_95:.4f}")
    print(f"  VaR (99%): {var_99:.4f}")
    print(f"  CVaR (95%): {cvar_95:.4f}")
    
    return portfolio_returns, benchmark_returns

def demonstrate_rolling_metrics():
    """Demonstrate rolling metrics calculations"""
    print("\n=== Rolling Metrics Demo ===")
    
    portfolio_returns, benchmark_returns = demonstrate_performance_analytics()
    
    # Calculate rolling metrics
    window = 21  # 1-month window
    print(f"Calculating rolling metrics with {window}-day window...")
    
    rolling_sharpe = pf.analytics.rolling_sharpe(portfolio_returns, window, 0.0)
    rolling_vol = pf.analytics.rolling_volatility(portfolio_returns, window)
    rolling_beta = pf.analytics.rolling_beta(portfolio_returns, benchmark_returns, window)
    
    print(f"Rolling Sharpe - Mean: {np.mean([s for _, s in rolling_sharpe.to_numpy()[1]]):.3f}")
    print(f"Rolling Volatility - Mean: {np.mean([v for _, v in rolling_vol.to_numpy()[1]]):.3f}")
    print(f"Rolling Beta - Mean: {np.mean([b for _, b in rolling_beta.to_numpy()[1]]):.3f}")

def demonstrate_backtesting():
    """Demonstrate backtesting capabilities"""
    print("\n=== Backtesting Demo ===")
    
    # Create sample price data
    start_date = pf.DateTime(2023, 1, 1)
    end_date = pf.DateTime(2023, 6, 30)
    
    # Generate price series for two assets
    aapl_prices = pf.create_sample_data(start_date, end_date, 150.0, 0.02)
    msft_prices = pf.create_sample_data(start_date, end_date, 300.0, 0.018)
    
    # Convert to price time series
    aapl_price_ts = pf.PriceTimeSeries.create(*aapl_prices.to_numpy(), "AAPL")
    msft_price_ts = pf.PriceTimeSeries.create(*msft_prices.to_numpy(), "MSFT")
    
    # Configure backtest
    config = pf.backtesting.BacktestConfig()
    config.start_date = start_date
    config.end_date = end_date
    config.initial_capital = 1000000.0
    
    # Set commission structure (0.1% per trade)
    config.commission.type = pf.backtesting.CommissionType.Percentage
    config.commission.rate = 0.001
    
    # Set market impact model
    config.market_impact.model = pf.backtesting.MarketImpactModel.SquareRoot
    config.market_impact.impact_coefficient = 0.05
    
    print(f"Backtest configuration:")
    print(f"  Period: {start_date.to_string()} to {end_date.to_string()}")
    print(f"  Initial Capital: ${config.initial_capital:,.0f}")
    print(f"  Commission: {config.commission.rate*100:.1f}%")
    print(f"  Market Impact: {config.market_impact.model}")
    
    # Create backtester
    backtester = pf.backtesting.AdvancedBacktester(config)
    
    # Load data
    backtester.load_price_data("AAPL", aapl_price_ts)
    backtester.load_price_data("MSFT", msft_price_ts)
    
    # Create and set strategy (Buy and Hold)
    symbols = ["AAPL", "MSFT"]
    strategy = pf.backtesting.BuyAndHoldStrategy(symbols)
    backtester.set_strategy(strategy)
    
    print(f"Running backtest with {strategy.get_name()} strategy...")
    
    # Run backtest
    results = backtester.run_backtest()
    
    # Display results
    print(f"\nBacktest Results:")
    print(f"  Final Value: ${results.final_value:,.2f}")
    total_return = (results.final_value / results.initial_capital - 1) * 100
    print(f"  Total Return: {total_return:.2f}%")
    print(f"  Sharpe Ratio: {results.sharpe_ratio:.3f}")
    print(f"  Max Drawdown: {results.max_drawdown*100:.2f}%")
    print(f"  Total Trades: {results.total_trades}")
    print(f"  Total Commission: ${results.total_commission:,.2f}")
    print(f"  Total Market Impact: ${results.total_market_impact:,.2f}")
    print(f"  Total Transaction Costs: ${results.total_commission + results.total_market_impact:,.2f}")
    
    # Show detailed report
    print(f"\nDetailed Report:")
    print(results.generate_report())
    
    return results

def demonstrate_machine_learning():
    """Demonstrate machine learning regime detection"""
    print("\n=== Machine Learning Demo ===")
    
    # Create sample data with regime changes
    start_date = pf.DateTime(2022, 1, 1)
    end_date = pf.DateTime(2023, 12, 31)
    
    returns = pf.create_sample_data(start_date, end_date, 0.0, 0.02)
    
    # Initialize regime detector
    detector = pf.ml.MLRegimeDetector(pf.ml.RegimeDetectionMethod.RandomForest)
    
    print("Detecting market regimes using Random Forest...")
    regimes = detector.detect_regimes(returns)
    regime_probs = detector.get_regime_probabilities(returns)
    
    print(f"Detected {regimes.size()} regime classifications")
    print(f"Regime probabilities shape: {regime_probs.size()}")

def create_performance_visualization(results):
    """Create performance visualization using matplotlib"""
    print("\n=== Creating Performance Visualization ===")
    
    try:
        # Extract portfolio values for plotting
        timestamps, values = results.portfolio_values.to_numpy()
        
        # Convert timestamps to datetime objects for matplotlib
        dates = [datetime.fromtimestamp(ts) for ts in timestamps]
        
        # Create the plot
        plt.figure(figsize=(12, 8))
        
        # Portfolio value over time
        plt.subplot(2, 2, 1)
        plt.plot(dates, values, 'b-', linewidth=2, label='Portfolio Value')
        plt.title('Portfolio Value Over Time')
        plt.xlabel('Date')
        plt.ylabel('Value ($)')
        plt.legend()
        plt.grid(True, alpha=0.3)
        
        # Returns distribution
        if hasattr(results, 'returns') and results.returns.size() > 0:
            _, returns = results.returns.to_numpy()
            
            plt.subplot(2, 2, 2)
            plt.hist(returns, bins=50, alpha=0.7, color='green', edgecolor='black')
            plt.title('Returns Distribution')
            plt.xlabel('Daily Returns')
            plt.ylabel('Frequency')
            plt.grid(True, alpha=0.3)
        
        # Cumulative returns
        cumulative_returns = [(v / values[0] - 1) * 100 for v in values]
        
        plt.subplot(2, 2, 3)
        plt.plot(dates, cumulative_returns, 'r-', linewidth=2)
        plt.title('Cumulative Returns (%)')
        plt.xlabel('Date')
        plt.ylabel('Return (%)')
        plt.grid(True, alpha=0.3)
        
        # Drawdown
        peak = values[0]
        drawdowns = []
        for value in values:
            if value > peak:
                peak = value
            drawdown = (value - peak) / peak * 100
            drawdowns.append(drawdown)
        
        plt.subplot(2, 2, 4)
        plt.fill_between(dates, drawdowns, 0, color='red', alpha=0.3)
        plt.plot(dates, drawdowns, 'r-', linewidth=1)
        plt.title('Drawdown (%)')
        plt.xlabel('Date')
        plt.ylabel('Drawdown (%)')
        plt.grid(True, alpha=0.3)
        
        plt.tight_layout()
        
        # Save the plot
        output_path = "pyfolio_cpp_performance.png"
        plt.savefig(output_path, dpi=300, bbox_inches='tight')
        print(f"Performance visualization saved to: {output_path}")
        
        # Show the plot if running interactively
        if hasattr(plt, 'show'):
            plt.show()
        
    except Exception as e:
        print(f"Error creating visualization: {e}")

def demonstrate_numpy_integration():
    """Demonstrate NumPy integration"""
    print("\n=== NumPy Integration Demo ===")
    
    # Create NumPy arrays
    dates = np.arange(np.datetime64('2023-01-01'), np.datetime64('2023-12-31'), dtype='datetime64[D]')
    timestamps = np.array([d.astype('datetime64[s]').astype(int) for d in dates], dtype=float)
    
    # Generate random returns
    np.random.seed(42)
    returns = np.random.normal(0.0008, 0.02, len(timestamps))  # Daily returns
    
    # Create TimeSeries from NumPy arrays
    print("Creating TimeSeries from NumPy arrays...")
    ts = pf.TimeSeries.create(timestamps, returns, "numpy_returns")
    
    print(f"Created TimeSeries with {ts.size()} data points")
    
    # Convert back to NumPy
    np_timestamps, np_returns = ts.to_numpy()
    
    print(f"Converted back to NumPy arrays:")
    print(f"  Timestamps shape: {np_timestamps.shape}")
    print(f"  Returns shape: {np_returns.shape}")
    print(f"  Returns mean: {np.mean(np_returns):.6f}")
    print(f"  Returns std: {np.std(np_returns):.6f}")
    
    # Calculate metrics
    metrics = pf.analytics.calculate_performance_metrics(ts)
    print(f"  Calculated Sharpe ratio: {metrics.sharpe_ratio:.3f}")

def main():
    """Main demonstration function"""
    print("=== PyFolio C++ Python Bindings Demo ===")
    print("Demonstrating high-performance portfolio analytics\n")
    
    try:
        # Check library version
        print(f"Library version: {pf.version()}")
        
        # Run demonstrations
        demonstrate_performance_analytics()
        demonstrate_rolling_metrics()
        
        # Run backtesting demo and get results
        results = demonstrate_backtesting()
        
        # Create visualization
        create_performance_visualization(results)
        
        # Demonstrate ML capabilities
        demonstrate_machine_learning()
        
        # Demonstrate NumPy integration
        demonstrate_numpy_integration()
        
        print("\n=== Demo completed successfully! ===")
        print("\nKey features demonstrated:")
        print("✓ Time series creation and manipulation")
        print("✓ Performance metrics calculation")
        print("✓ Risk analytics (VaR, CVaR)")
        print("✓ Rolling window calculations")
        print("✓ Advanced backtesting with transaction costs")
        print("✓ Machine learning regime detection")
        print("✓ NumPy array integration")
        print("✓ Performance visualization")
        
    except Exception as e:
        print(f"Error during demonstration: {e}")
        import traceback
        traceback.print_exc()
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())