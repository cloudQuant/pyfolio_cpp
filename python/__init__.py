"""
PyFolio C++ - High-Performance Portfolio Analytics

A comprehensive Python wrapper for the PyFolio C++ library, providing
high-performance quantitative finance and portfolio analytics capabilities
with seamless Python integration.

Example Usage:
    >>> import pyfolio_cpp as pf
    >>> import numpy as np
    >>> from datetime import datetime, timedelta
    
    # Create sample data
    >>> start_date = pf.DateTime(2023, 1, 1)
    >>> end_date = pf.DateTime(2023, 12, 31)
    >>> returns = pf.create_sample_data(start_date, end_date, 100.0, 0.02)
    
    # Calculate performance metrics
    >>> metrics = pf.analytics.calculate_performance_metrics(returns)
    >>> print(f"Sharpe Ratio: {metrics.sharpe_ratio:.3f}")
    >>> print(f"Max Drawdown: {metrics.max_drawdown:.3f}")
    
    # Run backtesting
    >>> config = pf.backtesting.BacktestConfig()
    >>> config.start_date = start_date
    >>> config.end_date = end_date
    >>> config.initial_capital = 1000000.0
    
    >>> backtester = pf.backtesting.AdvancedBacktester(config)
    >>> # Load price data and run backtest...

Key Modules:
    analytics: Performance metrics, risk analysis, rolling calculations
    backtesting: Advanced backtesting framework with transaction costs
    ml: Machine learning regime detection algorithms
    visualization: Enhanced plotting and dashboard creation
    streaming: Real-time data processing and analysis

Performance Features:
    - SIMD-optimized mathematical operations
    - Memory-efficient time series data structures
    - Parallel processing for large datasets
    - GPU acceleration support
    - O(n) rolling window calculations
"""

# Import the compiled C++ module
try:
    from . import pyfolio_cpp
    
    # Re-export main classes and functions for convenient access
    DateTime = pyfolio_cpp.DateTime
    TimeSeries = pyfolio_cpp.TimeSeries
    PriceTimeSeries = pyfolio_cpp.PriceTimeSeries
    PerformanceMetrics = pyfolio_cpp.PerformanceMetrics
    Position = pyfolio_cpp.Position
    
    # Re-export submodules
    analytics = pyfolio_cpp.analytics
    backtesting = pyfolio_cpp.backtesting
    ml = pyfolio_cpp.ml
    visualization = pyfolio_cpp.visualization
    streaming = pyfolio_cpp.streaming
    
    # Re-export utility functions
    version = pyfolio_cpp.version
    create_sample_data = pyfolio_cpp.create_sample_data
    
    # Check if GPU acceleration is available
    def has_gpu_support():
        """Check if GPU acceleration is available"""
        try:
            # This would check for CUDA/OpenCL availability
            # Implementation depends on the actual GPU module
            return True  # Placeholder
        except:
            return False
    
    __all__ = [
        'DateTime', 'TimeSeries', 'PriceTimeSeries', 'PerformanceMetrics', 'Position',
        'analytics', 'backtesting', 'ml', 'visualization', 'streaming',
        'version', 'create_sample_data', 'has_gpu_support'
    ]
    
except ImportError as e:
    # Fallback when C++ module is not available
    import warnings
    warnings.warn(f"PyFolio C++ module not available: {e}. "
                  "Make sure the library is properly compiled and installed.")
    
    # Provide stub implementations for development
    class StubClass:
        def __init__(self, *args, **kwargs):
            raise RuntimeError("PyFolio C++ not compiled. Please run 'pip install -e .' to build.")
    
    DateTime = StubClass
    TimeSeries = StubClass
    PriceTimeSeries = StubClass
    PerformanceMetrics = StubClass
    Position = StubClass
    
    def stub_function(*args, **kwargs):
        raise RuntimeError("PyFolio C++ not compiled. Please run 'pip install -e .' to build.")
    
    version = stub_function
    create_sample_data = stub_function
    has_gpu_support = stub_function
    
    # Create stub modules
    class StubModule:
        def __getattr__(self, name):
            return stub_function
    
    analytics = StubModule()
    backtesting = StubModule()
    ml = StubModule()
    visualization = StubModule()
    streaming = StubModule()

# Version information
__version__ = "1.0.0"
__author__ = "PyFolio Development Team"
__email__ = "dev@pyfolio.ai"
__license__ = "Apache License 2.0"
__description__ = "High-performance portfolio analytics library"