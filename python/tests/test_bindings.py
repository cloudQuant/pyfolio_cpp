#!/usr/bin/env python3
"""
PyFolio C++ Python Bindings Test Suite

Comprehensive tests for the Python bindings to ensure functionality,
performance, and compatibility with Python data structures.
"""

import pytest
import numpy as np
import pandas as pd
import sys
import os
from datetime import datetime, timedelta

# Add parent directory to path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

try:
    import pyfolio_cpp as pf
    BINDINGS_AVAILABLE = True
except ImportError:
    BINDINGS_AVAILABLE = False
    pytest.skip("PyFolio C++ bindings not available", allow_module_level=True)

class TestCoreTypes:
    """Test core data types and basic functionality"""
    
    def test_datetime_creation(self):
        """Test DateTime creation and basic operations"""
        dt = pf.DateTime(2023, 6, 15)
        assert dt.year() == 2023
        assert dt.month() == 6
        assert dt.day() == 15
        
        # Test string representation
        date_str = dt.to_string()
        assert "2023" in date_str
        assert "06" in date_str or "6" in date_str
        
        # Test timestamp conversion
        timestamp = dt.timestamp()
        assert timestamp > 0
        
        # Test date arithmetic
        dt_plus_days = dt.add_days(10)
        assert dt_plus_days.day() == 25
    
    def test_datetime_from_timestamp(self):
        """Test DateTime creation from timestamp"""
        timestamp = 1687000000  # June 17, 2023
        dt = pf.DateTime.from_timestamp(timestamp)
        
        assert dt.year() == 2023
        assert dt.month() == 6
        assert dt.day() == 17
    
    def test_timeseries_creation(self):
        """Test TimeSeries creation and basic operations"""
        # Create sample data
        n_points = 100
        timestamps = np.arange(1640995200, 1640995200 + n_points * 86400, 86400, dtype=float)
        values = np.random.normal(0.001, 0.02, n_points)
        
        # Create TimeSeries
        ts = pf.TimeSeries.create(timestamps, values, "test_series")
        
        assert ts.size() == n_points
        assert ts.name() == "test_series"
        assert not ts.empty()
        
        # Test conversion back to numpy
        ts_back, vals_back = ts.to_numpy()
        
        np.testing.assert_array_almost_equal(timestamps, ts_back)
        np.testing.assert_array_almost_equal(values, vals_back)
    
    def test_price_timeseries(self):
        """Test PriceTimeSeries functionality"""
        n_points = 50
        timestamps = np.arange(1640995200, 1640995200 + n_points * 86400, 86400, dtype=float)
        prices = np.random.uniform(90, 110, n_points)
        
        price_ts = pf.PriceTimeSeries.create(timestamps, prices, "AAPL")
        
        assert price_ts.size() == n_points
        
        # Test conversion
        ts_back, prices_back = price_ts.to_numpy()
        np.testing.assert_array_almost_equal(prices, prices_back)


class TestAnalytics:
    """Test analytics functionality"""
    
    def create_sample_returns(self, n_points=252):
        """Helper to create sample returns data"""
        timestamps = np.arange(1640995200, 1640995200 + n_points * 86400, 86400, dtype=float)
        returns = np.random.normal(0.0008, 0.02, n_points)  # Daily returns
        return pf.TimeSeries.create(timestamps, returns, "returns")
    
    def test_performance_metrics(self):
        """Test performance metrics calculation"""
        returns = self.create_sample_returns(252)
        
        metrics = pf.analytics.calculate_performance_metrics(returns)
        
        # Check that metrics are calculated
        assert hasattr(metrics, 'total_return')
        assert hasattr(metrics, 'annual_return')
        assert hasattr(metrics, 'annual_volatility')
        assert hasattr(metrics, 'sharpe_ratio')
        assert hasattr(metrics, 'max_drawdown')
        
        # Sanity checks
        assert -1 <= metrics.total_return <= 5  # Reasonable range
        assert 0 <= metrics.annual_volatility <= 2  # Reasonable volatility
        assert metrics.max_drawdown >= 0  # Drawdown should be positive
    
    def test_sharpe_ratio(self):
        """Test Sharpe ratio calculation"""
        returns = self.create_sample_returns()
        
        sharpe = pf.analytics.calculate_sharpe_ratio(returns, 0.02)  # 2% risk-free rate
        
        assert isinstance(sharpe, float)
        assert -5 <= sharpe <= 5  # Reasonable range for Sharpe ratio
    
    def test_var_calculation(self):
        """Test VaR calculation"""
        returns = self.create_sample_returns()
        
        var_95 = pf.analytics.calculate_var(returns, 0.05)
        var_99 = pf.analytics.calculate_var(returns, 0.01)
        
        assert isinstance(var_95, float)
        assert isinstance(var_99, float)
        assert var_95 < 0  # VaR should be negative
        assert var_99 < var_95  # 99% VaR should be more extreme
    
    def test_cvar_calculation(self):
        """Test CVaR calculation"""
        returns = self.create_sample_returns()
        
        cvar_95 = pf.analytics.calculate_cvar(returns, 0.05)
        
        assert isinstance(cvar_95, float)
        assert cvar_95 < 0  # CVaR should be negative
    
    def test_rolling_metrics(self):
        """Test rolling metrics calculations"""
        returns = self.create_sample_returns(200)
        benchmark = self.create_sample_returns(200)
        
        window = 21
        
        # Rolling Sharpe
        rolling_sharpe = pf.analytics.rolling_sharpe(returns, window, 0.0)
        assert rolling_sharpe.size() == returns.size() - window + 1
        
        # Rolling volatility
        rolling_vol = pf.analytics.rolling_volatility(returns, window)
        assert rolling_vol.size() == returns.size() - window + 1
        
        # Rolling beta
        rolling_beta = pf.analytics.rolling_beta(returns, benchmark, window)
        assert rolling_beta.size() == returns.size() - window + 1


class TestBacktesting:
    """Test backtesting functionality"""
    
    def create_sample_price_data(self, n_days=100):
        """Helper to create sample price data"""
        timestamps = np.arange(1640995200, 1640995200 + n_days * 86400, 86400, dtype=float)
        
        # Generate correlated price series
        np.random.seed(42)
        returns_aapl = np.random.normal(0.0008, 0.02, n_days)
        returns_msft = np.random.normal(0.0006, 0.018, n_days)
        
        prices_aapl = np.cumprod(1 + returns_aapl) * 150
        prices_msft = np.cumprod(1 + returns_msft) * 300
        
        aapl_ts = pf.PriceTimeSeries.create(timestamps, prices_aapl, "AAPL")
        msft_ts = pf.PriceTimeSeries.create(timestamps, prices_msft, "MSFT")
        
        return aapl_ts, msft_ts, timestamps[0], timestamps[-1]
    
    def test_backtest_config(self):
        """Test backtest configuration"""
        config = pf.backtesting.BacktestConfig()
        
        # Test default values
        assert config.initial_capital == 1000000.0
        assert config.enable_partial_fills == True
        assert config.cash_buffer == 0.05
        
        # Test modification
        config.initial_capital = 500000.0
        assert config.initial_capital == 500000.0
    
    def test_commission_structure(self):
        """Test commission structure"""
        commission = pf.backtesting.CommissionStructure()
        
        # Test percentage commission
        commission.type = pf.backtesting.CommissionType.Percentage
        commission.rate = 0.001
        
        trade_value = 10000.0
        quantity = 100
        comm = commission.calculate_commission(trade_value, quantity)
        
        assert comm == 10.0  # 0.1% of 10000
    
    def test_market_impact(self):
        """Test market impact calculation"""
        impact = pf.backtesting.MarketImpactConfig()
        impact.model = pf.backtesting.MarketImpactModel.SquareRoot
        impact.impact_coefficient = 0.1
        
        trade_size = 1000
        daily_volume = 10000
        volatility = 0.02
        
        impact_value = impact.calculate_impact(trade_size, daily_volume, volatility)
        
        assert isinstance(impact_value, float)
        assert impact_value > 0  # Positive impact for buy order
    
    def test_basic_backtest(self):
        """Test basic backtesting functionality"""
        aapl_ts, msft_ts, start_ts, end_ts = self.create_sample_price_data(50)
        
        # Create configuration
        config = pf.backtesting.BacktestConfig()
        config.start_date = pf.DateTime.from_timestamp(int(start_ts))
        config.end_date = pf.DateTime.from_timestamp(int(end_ts))
        config.initial_capital = 100000.0
        
        # Create backtester
        backtester = pf.backtesting.AdvancedBacktester(config)
        
        # Load data
        backtester.load_price_data("AAPL", aapl_ts)
        backtester.load_price_data("MSFT", msft_ts)
        
        # Create strategy
        symbols = ["AAPL", "MSFT"]
        strategy = pf.backtesting.BuyAndHoldStrategy(symbols)
        backtester.set_strategy(strategy)
        
        # Run backtest
        results = backtester.run_backtest()
        
        # Verify results
        assert results.initial_capital == 100000.0
        assert results.final_value > 0
        assert results.total_trades >= 0
        assert hasattr(results, 'performance')
        assert hasattr(results, 'trade_history')
    
    def test_trading_strategies(self):
        """Test different trading strategies"""
        symbols = ["AAPL", "MSFT"]
        
        # Buy and Hold
        bah_strategy = pf.backtesting.BuyAndHoldStrategy(symbols)
        assert bah_strategy.get_name() == "Buy and Hold"
        
        # Equal Weight
        eq_strategy = pf.backtesting.EqualWeightStrategy(symbols, 21)
        assert "Equal Weight" in eq_strategy.get_name()
        
        # Momentum
        mom_strategy = pf.backtesting.MomentumStrategy(symbols, 20, 1)
        assert "Momentum" in mom_strategy.get_name()


class TestMachineLearning:
    """Test machine learning functionality"""
    
    def create_sample_regime_data(self):
        """Create sample data with regime characteristics"""
        n_points = 500
        timestamps = np.arange(1640995200, 1640995200 + n_points * 86400, 86400, dtype=float)
        
        # Create data with two regimes
        regime_1 = np.random.normal(0.001, 0.01, n_points // 2)  # Low vol regime
        regime_2 = np.random.normal(-0.0005, 0.03, n_points // 2)  # High vol regime
        
        returns = np.concatenate([regime_1, regime_2])
        
        return pf.TimeSeries.create(timestamps, returns, "regime_data")
    
    def test_regime_detection_methods(self):
        """Test different regime detection methods"""
        methods = [
            pf.ml.RegimeDetectionMethod.RandomForest,
            pf.ml.RegimeDetectionMethod.SVM,
            pf.ml.RegimeDetectionMethod.NeuralNetwork,
        ]
        
        returns = self.create_sample_regime_data()
        
        for method in methods:
            detector = pf.ml.MLRegimeDetector(method)
            
            # Test regime detection
            regimes = detector.detect_regimes(returns)
            assert regimes.size() > 0
            
            # Test regime probabilities
            probabilities = detector.get_regime_probabilities(returns)
            assert probabilities.size() > 0


class TestUtilities:
    """Test utility functions"""
    
    def test_version(self):
        """Test version function"""
        version = pf.version()
        assert isinstance(version, str)
        assert len(version) > 0
    
    def test_create_sample_data(self):
        """Test sample data creation"""
        start_date = pf.DateTime(2023, 1, 1)
        end_date = pf.DateTime(2023, 3, 31)
        
        sample_data = pf.create_sample_data(start_date, end_date, 100.0, 0.02)
        
        assert sample_data.size() > 0
        assert sample_data.name() == "sample_data"
        
        # Verify data characteristics
        _, values = sample_data.to_numpy()
        assert len(values) > 80  # Approximately 90 days
        assert np.all(values > 0)  # All values should be positive


class TestNumpyIntegration:
    """Test NumPy integration"""
    
    def test_numpy_array_conversion(self):
        """Test conversion to/from NumPy arrays"""
        # Create NumPy data
        n_points = 100
        timestamps = np.linspace(1640995200, 1640995200 + n_points * 86400, n_points)
        values = np.random.normal(0.001, 0.02, n_points)
        
        # Convert to TimeSeries
        ts = pf.TimeSeries.create(timestamps, values, "numpy_test")
        
        # Convert back to NumPy
        ts_back, vals_back = ts.to_numpy()
        
        # Verify data integrity
        np.testing.assert_array_almost_equal(timestamps, ts_back, decimal=5)
        np.testing.assert_array_almost_equal(values, vals_back, decimal=10)
    
    def test_large_array_handling(self):
        """Test handling of large arrays"""
        n_points = 10000
        timestamps = np.arange(1640995200, 1640995200 + n_points * 86400, 86400, dtype=float)
        values = np.random.normal(0.0005, 0.015, n_points)
        
        # This should not raise memory errors
        ts = pf.TimeSeries.create(timestamps, values, "large_array")
        assert ts.size() == n_points
        
        # Performance metrics should still work
        metrics = pf.analytics.calculate_performance_metrics(ts)
        assert hasattr(metrics, 'sharpe_ratio')


class TestErrorHandling:
    """Test error handling and edge cases"""
    
    def test_invalid_timeseries_creation(self):
        """Test error handling for invalid TimeSeries creation"""
        timestamps = np.array([1640995200, 1640995300])
        values = np.array([0.1, 0.2, 0.3])  # Mismatched sizes
        
        with pytest.raises(RuntimeError):
            pf.TimeSeries.create(timestamps, values, "invalid")
    
    def test_empty_timeseries(self):
        """Test handling of empty TimeSeries"""
        empty_ts = pf.TimeSeries()
        assert empty_ts.size() == 0
        assert empty_ts.empty() == True
    
    def test_insufficient_data_for_rolling(self):
        """Test rolling metrics with insufficient data"""
        small_ts = pf.TimeSeries.create(
            np.array([1640995200, 1640995300]),
            np.array([0.1, 0.2]),
            "small"
        )
        
        # This should handle gracefully or raise appropriate error
        try:
            rolling_sharpe = pf.analytics.rolling_sharpe(small_ts, 50, 0.0)
            # If it succeeds, should return empty or very small result
            assert rolling_sharpe.size() == 0
        except RuntimeError:
            # This is also acceptable behavior
            pass


class TestPerformance:
    """Performance tests to ensure bindings are efficient"""
    
    def test_large_dataset_performance(self):
        """Test performance with large datasets"""
        import time
        
        # Create large dataset
        n_points = 100000
        timestamps = np.arange(1640995200, 1640995200 + n_points * 86400, 86400, dtype=float)
        values = np.random.normal(0.0005, 0.02, n_points)
        
        # Time TimeSeries creation
        start_time = time.time()
        ts = pf.TimeSeries.create(timestamps, values, "perf_test")
        creation_time = time.time() - start_time
        
        assert creation_time < 1.0  # Should be fast
        assert ts.size() == n_points
        
        # Time metrics calculation
        start_time = time.time()
        metrics = pf.analytics.calculate_performance_metrics(ts)
        calc_time = time.time() - start_time
        
        assert calc_time < 0.5  # Should be very fast
        assert hasattr(metrics, 'sharpe_ratio')


if __name__ == "__main__":
    # Run tests if executed directly
    import sys
    
    if BINDINGS_AVAILABLE:
        print(f"Running PyFolio C++ bindings tests...")
        print(f"Library version: {pf.version()}")
        
        # Run pytest
        pytest.main([__file__, "-v"])
    else:
        print("PyFolio C++ bindings not available. Please build the library first.")
        sys.exit(1)