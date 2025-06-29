#!/usr/bin/env python3
"""
PyFolio C++ Pandas Integration Example

This example demonstrates seamless integration between PyFolio C++
and pandas DataFrames, showing how to work with real-world financial
data formats and convert between pandas and PyFolio data structures.
"""

import sys
import os
import numpy as np
import pandas as pd
from datetime import datetime, timedelta
import warnings
warnings.filterwarnings('ignore')

# Add the parent directory to path to import pyfolio_cpp
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

try:
    import pyfolio_cpp as pf
    print(f"PyFolio C++ version: {pf.version()}")
except ImportError as e:
    print(f"Error importing PyFolio C++: {e}")
    print("Please build the library first: cd python && pip install -e .")
    sys.exit(1)

def create_sample_financial_data():
    """Create sample financial data in pandas format"""
    print("Creating sample financial data in pandas format...")
    
    # Create date range
    dates = pd.date_range('2023-01-01', '2023-12-31', freq='D')
    
    # Simulate multiple assets
    symbols = ['AAPL', 'MSFT', 'GOOGL', 'AMZN', 'TSLA']
    
    np.random.seed(42)
    
    # Generate correlated price data
    n_assets = len(symbols)
    n_days = len(dates)
    
    # Create correlation matrix
    correlation = np.random.uniform(0.3, 0.7, (n_assets, n_assets))
    correlation = (correlation + correlation.T) / 2
    np.fill_diagonal(correlation, 1.0)
    
    # Generate correlated returns
    returns = np.random.multivariate_normal(
        mean=[0.0008] * n_assets,  # Daily expected returns
        cov=correlation * 0.0004,  # Covariance matrix
        size=n_days
    )
    
    # Convert to prices (starting from different initial prices)
    initial_prices = [150, 300, 2500, 3200, 800]
    prices_data = {}
    
    for i, symbol in enumerate(symbols):
        price_series = [initial_prices[i]]
        for daily_return in returns[:, i]:
            price_series.append(price_series[-1] * (1 + daily_return))
        prices_data[symbol] = price_series[1:]  # Remove initial price
    
    # Create pandas DataFrame
    prices_df = pd.DataFrame(prices_data, index=dates)
    
    # Calculate returns
    returns_df = prices_df.pct_change().dropna()
    
    # Create volume data (random but realistic)
    volume_data = {}
    for symbol in symbols:
        volume_data[symbol] = np.random.lognormal(
            mean=np.log(1000000), sigma=0.5, size=len(dates)
        )
    volume_df = pd.DataFrame(volume_data, index=dates)
    
    print(f"Created data for {len(symbols)} symbols over {len(dates)} days")
    print(f"Price data shape: {prices_df.shape}")
    print(f"Returns data shape: {returns_df.shape}")
    
    return prices_df, returns_df, volume_df

def pandas_to_pyfolio_timeseries(df, symbol, name_suffix=""):
    """Convert pandas Series to PyFolio TimeSeries"""
    series = df[symbol]
    
    # Convert pandas timestamps to Unix timestamps
    timestamps = np.array([ts.timestamp() for ts in series.index])
    values = series.values.astype(float)
    
    # Create PyFolio TimeSeries
    name = f"{symbol}{name_suffix}"
    if symbol in df.columns and hasattr(df[symbol], 'dtype') and 'float' in str(df[symbol].dtype):
        return pf.TimeSeries.create(timestamps, values, name)
    else:
        return pf.PriceTimeSeries.create(timestamps, values, name)

def pyfolio_to_pandas_series(timeseries, name):
    """Convert PyFolio TimeSeries to pandas Series"""
    timestamps, values = timeseries.to_numpy()
    
    # Convert Unix timestamps back to pandas datetime
    index = pd.to_datetime(timestamps, unit='s')
    
    return pd.Series(values, index=index, name=name)

def demonstrate_pandas_conversion():
    """Demonstrate conversion between pandas and PyFolio formats"""
    print("\n=== Pandas Integration Demo ===")
    
    # Create sample data
    prices_df, returns_df, volume_df = create_sample_financial_data()
    
    # Show original pandas data
    print(f"\nOriginal pandas data:")
    print(f"Prices (first 5 rows):")
    print(prices_df.head())
    print(f"\nReturns (first 5 rows):")
    print(returns_df.head())
    
    # Convert specific symbol to PyFolio TimeSeries
    symbol = 'AAPL'
    print(f"\nConverting {symbol} data to PyFolio TimeSeries...")
    
    aapl_prices = pandas_to_pyfolio_timeseries(prices_df, symbol, "_prices")
    aapl_returns = pandas_to_pyfolio_timeseries(returns_df, symbol, "_returns")
    
    print(f"PyFolio price series size: {aapl_prices.size()}")
    print(f"PyFolio returns series size: {aapl_returns.size()}")
    
    # Calculate performance metrics using PyFolio C++
    print(f"\nCalculating performance metrics for {symbol}...")
    metrics = pf.analytics.calculate_performance_metrics(aapl_returns)
    
    print(f"Performance Metrics for {symbol}:")
    print(f"  Annual Return: {metrics.annual_return:.3f} ({metrics.annual_return*100:.1f}%)")
    print(f"  Annual Volatility: {metrics.annual_volatility:.3f} ({metrics.annual_volatility*100:.1f}%)")
    print(f"  Sharpe Ratio: {metrics.sharpe_ratio:.3f}")
    print(f"  Max Drawdown: {metrics.max_drawdown:.3f} ({metrics.max_drawdown*100:.1f}%)")
    
    # Convert back to pandas for comparison
    print(f"\nConverting PyFolio results back to pandas...")
    aapl_prices_pandas = pyfolio_to_pandas_series(aapl_prices, f"{symbol}_prices")
    aapl_returns_pandas = pyfolio_to_pandas_series(aapl_returns, f"{symbol}_returns")
    
    print(f"Converted prices series length: {len(aapl_prices_pandas)}")
    print(f"Converted returns series length: {len(aapl_returns_pandas)}")
    
    # Verify data integrity
    original_prices = prices_df[symbol]
    original_returns = returns_df[symbol]
    
    price_diff = np.abs(original_prices.values - aapl_prices_pandas.values).max()
    returns_diff = np.abs(original_returns.values - aapl_returns_pandas.values).max()
    
    print(f"\nData integrity check:")
    print(f"  Max price difference: {price_diff:.10f}")
    print(f"  Max returns difference: {returns_diff:.10f}")
    print(f"  Data integrity: {'✓ PASSED' if price_diff < 1e-6 and returns_diff < 1e-10 else '✗ FAILED'}")
    
    return prices_df, returns_df, volume_df

def demonstrate_portfolio_analysis():
    """Demonstrate portfolio-level analysis with pandas integration"""
    print("\n=== Portfolio Analysis Demo ===")
    
    prices_df, returns_df, volume_df = create_sample_financial_data()
    
    # Define portfolio weights
    symbols = list(prices_df.columns)
    weights = np.array([0.3, 0.25, 0.2, 0.15, 0.1])  # Portfolio weights
    
    print(f"Analyzing portfolio with weights:")
    for symbol, weight in zip(symbols, weights):
        print(f"  {symbol}: {weight*100:.1f}%")
    
    # Calculate portfolio returns using pandas
    portfolio_returns_pandas = (returns_df * weights).sum(axis=1)
    
    print(f"\nPortfolio returns calculated (pandas): {len(portfolio_returns_pandas)} observations")
    
    # Convert to PyFolio TimeSeries for advanced analytics
    portfolio_ts = pandas_to_pyfolio_timeseries(
        pd.DataFrame({'portfolio': portfolio_returns_pandas}), 
        'portfolio', 
        "_returns"
    )
    
    # Calculate comprehensive performance metrics
    print(f"Calculating comprehensive performance metrics...")
    portfolio_metrics = pf.analytics.calculate_performance_metrics(portfolio_ts)
    
    print(f"\nPortfolio Performance:")
    print(f"  Total Return: {portfolio_metrics.total_return:.3f} ({portfolio_metrics.total_return*100:.1f}%)")
    print(f"  Annual Return: {portfolio_metrics.annual_return:.3f} ({portfolio_metrics.annual_return*100:.1f}%)")
    print(f"  Annual Volatility: {portfolio_metrics.annual_volatility:.3f} ({portfolio_metrics.annual_volatility*100:.1f}%)")
    print(f"  Sharpe Ratio: {portfolio_metrics.sharpe_ratio:.3f}")
    print(f"  Sortino Ratio: {portfolio_metrics.sortino_ratio:.3f}")
    print(f"  Calmar Ratio: {portfolio_metrics.calmar_ratio:.3f}")
    print(f"  Max Drawdown: {portfolio_metrics.max_drawdown:.3f} ({portfolio_metrics.max_drawdown*100:.1f}%)")
    
    # Calculate risk metrics
    print(f"\nRisk Analytics:")
    var_95 = pf.analytics.calculate_var(portfolio_ts, 0.05)
    var_99 = pf.analytics.calculate_var(portfolio_ts, 0.01)
    cvar_95 = pf.analytics.calculate_cvar(portfolio_ts, 0.05)
    
    print(f"  VaR (95%): {var_95:.4f} ({var_95*100:.2f}%)")
    print(f"  VaR (99%): {var_99:.4f} ({var_99*100:.2f}%)")
    print(f"  CVaR (95%): {cvar_95:.4f} ({cvar_95*100:.2f}%)")
    
    # Compare with individual asset performance
    print(f"\nIndividual Asset Performance Comparison:")
    asset_metrics = []
    
    for symbol in symbols:
        asset_returns = pandas_to_pyfolio_timeseries(returns_df, symbol, "_returns")
        asset_perf = pf.analytics.calculate_performance_metrics(asset_returns)
        asset_metrics.append({
            'Symbol': symbol,
            'Annual Return': asset_perf.annual_return,
            'Volatility': asset_perf.annual_volatility,
            'Sharpe Ratio': asset_perf.sharpe_ratio,
            'Max Drawdown': asset_perf.max_drawdown
        })
    
    # Create comparison DataFrame
    comparison_df = pd.DataFrame(asset_metrics)
    comparison_df = comparison_df.round(3)
    
    print(comparison_df.to_string(index=False))
    
    # Add portfolio to comparison
    portfolio_row = {
        'Symbol': 'PORTFOLIO',
        'Annual Return': portfolio_metrics.annual_return,
        'Volatility': portfolio_metrics.annual_volatility,
        'Sharpe Ratio': portfolio_metrics.sharpe_ratio,
        'Max Drawdown': portfolio_metrics.max_drawdown
    }
    
    print(f"\nPortfolio vs Best Individual Asset:")
    best_sharpe_idx = comparison_df['Sharpe Ratio'].idxmax()
    best_asset = comparison_df.iloc[best_sharpe_idx]
    
    print(f"  Best Individual Asset ({best_asset['Symbol']}):")
    print(f"    Sharpe Ratio: {best_asset['Sharpe Ratio']:.3f}")
    print(f"    Max Drawdown: {best_asset['Max Drawdown']:.3f}")
    
    print(f"  Portfolio:")
    print(f"    Sharpe Ratio: {portfolio_row['Sharpe Ratio']:.3f}")
    print(f"    Max Drawdown: {portfolio_row['Max Drawdown']:.3f}")
    
    diversification_benefit = portfolio_row['Sharpe Ratio'] > best_asset['Sharpe Ratio']
    print(f"  Diversification Benefit: {'✓ YES' if diversification_benefit else '✗ NO'}")

def demonstrate_rolling_analysis():
    """Demonstrate rolling analysis with pandas integration"""
    print("\n=== Rolling Analysis Demo ===")
    
    prices_df, returns_df, volume_df = create_sample_financial_data()
    
    # Focus on one asset for rolling analysis
    symbol = 'AAPL'
    asset_returns = pandas_to_pyfolio_timeseries(returns_df, symbol, "_returns")
    
    # Calculate rolling metrics
    windows = [21, 63, 252]  # 1 month, 3 months, 1 year
    
    for window in windows:
        if asset_returns.size() < window:
            continue
            
        print(f"\nRolling metrics with {window}-day window:")
        
        # Rolling Sharpe ratio
        rolling_sharpe = pf.analytics.rolling_sharpe(asset_returns, window, 0.0)
        sharpe_ts, sharpe_values = rolling_sharpe.to_numpy()
        
        print(f"  Rolling Sharpe Ratio:")
        print(f"    Mean: {np.mean(sharpe_values):.3f}")
        print(f"    Std: {np.std(sharpe_values):.3f}")
        print(f"    Min: {np.min(sharpe_values):.3f}")
        print(f"    Max: {np.max(sharpe_values):.3f}")
        
        # Rolling volatility
        rolling_vol = pf.analytics.rolling_volatility(asset_returns, window)
        vol_ts, vol_values = rolling_vol.to_numpy()
        
        print(f"  Rolling Volatility (annualized):")
        print(f"    Mean: {np.mean(vol_values):.3f} ({np.mean(vol_values)*100:.1f}%)")
        print(f"    Std: {np.std(vol_values):.3f}")
        print(f"    Min: {np.min(vol_values):.3f} ({np.min(vol_values)*100:.1f}%)")
        print(f"    Max: {np.max(vol_values):.3f} ({np.max(vol_values)*100:.1f}%)")

def demonstrate_backtesting_with_pandas():
    """Demonstrate backtesting using pandas data"""
    print("\n=== Backtesting with Pandas Data Demo ===")
    
    prices_df, returns_df, volume_df = create_sample_financial_data()
    
    # Use subset of data for backtesting
    backtest_start = prices_df.index[0]
    backtest_end = prices_df.index[len(prices_df)//2]  # Use first half for backtesting
    
    print(f"Backtesting period: {backtest_start.date()} to {backtest_end.date()}")
    
    # Filter data for backtesting period
    bt_prices = prices_df.loc[backtest_start:backtest_end]
    bt_symbols = ['AAPL', 'MSFT', 'GOOGL']  # Use subset of symbols
    
    print(f"Using symbols: {bt_symbols}")
    print(f"Backtesting data points: {len(bt_prices)}")
    
    # Convert to PyFolio format
    price_series = {}
    for symbol in bt_symbols:
        price_series[symbol] = pandas_to_pyfolio_timeseries(bt_prices, symbol, "_prices")
    
    # Configure backtest
    config = pf.backtesting.BacktestConfig()
    config.start_date = pf.DateTime.from_timestamp(int(backtest_start.timestamp()))
    config.end_date = pf.DateTime.from_timestamp(int(backtest_end.timestamp()))
    config.initial_capital = 1000000.0
    
    # Configure transaction costs
    config.commission.type = pf.backtesting.CommissionType.Percentage
    config.commission.rate = 0.001  # 0.1%
    
    config.market_impact.model = pf.backtesting.MarketImpactModel.SquareRoot
    config.market_impact.impact_coefficient = 0.05
    
    print(f"\nBacktest Configuration:")
    print(f"  Initial Capital: ${config.initial_capital:,.0f}")
    print(f"  Commission Rate: {config.commission.rate*100:.1f}%")
    print(f"  Market Impact Model: SquareRoot")
    
    # Create and run backtest
    backtester = pf.backtesting.AdvancedBacktester(config)
    
    # Load price data
    for symbol in bt_symbols:
        backtester.load_price_data(symbol, price_series[symbol])
    
    # Use Equal Weight strategy
    strategy = pf.backtesting.EqualWeightStrategy(bt_symbols, 21)  # Rebalance monthly
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
    print(f"  Total Transaction Costs: ${results.total_commission + results.total_market_impact:,.2f}")
    
    # Convert results back to pandas for further analysis
    portfolio_ts, portfolio_values = results.portfolio_values.to_numpy()
    portfolio_dates = pd.to_datetime(portfolio_ts, unit='s')
    portfolio_series = pd.Series(portfolio_values, index=portfolio_dates, name='Portfolio Value')
    
    print(f"\nPortfolio performance series created with {len(portfolio_series)} data points")
    print(f"Portfolio value range: ${portfolio_series.min():,.0f} - ${portfolio_series.max():,.0f}")
    
    # Calculate buy-and-hold benchmark
    equal_weights = 1.0 / len(bt_symbols)
    benchmark_value = (bt_prices[bt_symbols] * equal_weights).sum(axis=1) * config.initial_capital / bt_prices[bt_symbols].iloc[0].sum()
    
    print(f"\nBenchmark Comparison (Equal Weight Buy & Hold):")
    benchmark_return = (benchmark_value.iloc[-1] / benchmark_value.iloc[0] - 1) * 100
    print(f"  Benchmark Total Return: {benchmark_return:.2f}%")
    print(f"  Strategy vs Benchmark: {total_return - benchmark_return:+.2f}%")

def main():
    """Main demonstration function"""
    print("=== PyFolio C++ Pandas Integration Demo ===")
    print("Demonstrating seamless pandas DataFrame integration\n")
    
    try:
        # Run demonstrations
        demonstrate_pandas_conversion()
        demonstrate_portfolio_analysis()
        demonstrate_rolling_analysis()
        demonstrate_backtesting_with_pandas()
        
        print("\n=== Pandas Integration Demo completed successfully! ===")
        print("\nKey integration features demonstrated:")
        print("✓ Pandas DataFrame to PyFolio TimeSeries conversion")
        print("✓ PyFolio TimeSeries to pandas Series conversion")
        print("✓ Data integrity preservation during conversion")
        print("✓ Portfolio-level analysis with multiple assets")
        print("✓ Individual vs portfolio performance comparison")
        print("✓ Rolling metrics calculation and analysis")
        print("✓ Backtesting with pandas-sourced data")
        print("✓ Results conversion back to pandas format")
        print("\nThis demonstrates PyFolio C++ can seamlessly integrate")
        print("with existing pandas-based financial analysis workflows!")
        
    except Exception as e:
        print(f"Error during demonstration: {e}")
        import traceback
        traceback.print_exc()
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())