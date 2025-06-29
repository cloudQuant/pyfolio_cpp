#!/usr/bin/env python3
"""
Build and Test Script for PyFolio C++ Python Bindings

This script demonstrates how to build the Python bindings and run
basic functionality tests to verify the installation.
"""

import os
import sys
import subprocess
import tempfile
import shutil
from pathlib import Path

def run_command(command, cwd=None, capture_output=True):
    """Run a shell command and return the result"""
    print(f"Running: {command}")
    if cwd:
        print(f"  Working directory: {cwd}")
    
    try:
        result = subprocess.run(
            command, 
            shell=True, 
            cwd=cwd,
            capture_output=capture_output,
            text=True,
            check=False
        )
        
        if result.returncode != 0:
            print(f"Command failed with return code {result.returncode}")
            if result.stdout:
                print("STDOUT:", result.stdout)
            if result.stderr:
                print("STDERR:", result.stderr)
            return False
        
        if not capture_output:
            return True
            
        if result.stdout:
            print("Output:", result.stdout.strip())
        
        return True
        
    except Exception as e:
        print(f"Error running command: {e}")
        return False

def check_dependencies():
    """Check if all required dependencies are available"""
    print("=== Checking Dependencies ===")
    
    dependencies = {
        'python': 'python --version',
        'cmake': 'cmake --version',
        'pybind11': 'python -c "import pybind11; print(pybind11.__version__)"',
        'numpy': 'python -c "import numpy; print(numpy.__version__)"',
        'pandas': 'python -c "import pandas; print(pandas.__version__)"',
    }
    
    all_available = True
    
    for name, check_cmd in dependencies.items():
        print(f"Checking {name}...")
        if run_command(check_cmd):
            print(f"  ✓ {name} available")
        else:
            print(f"  ✗ {name} NOT available")
            all_available = False
    
    return all_available

def build_project():
    """Build the project using pip"""
    print("\n=== Building PyFolio C++ Python Bindings ===")
    
    # Get project root directory
    script_dir = Path(__file__).parent
    python_dir = script_dir.parent
    project_root = python_dir.parent
    
    print(f"Project root: {project_root}")
    print(f"Python bindings dir: {python_dir}")
    
    # Install in development mode
    build_cmd = f"pip install -e ."
    
    print("Building and installing Python bindings...")
    success = run_command(build_cmd, cwd=python_dir, capture_output=False)
    
    if success:
        print("✓ Build completed successfully")
    else:
        print("✗ Build failed")
    
    return success

def test_basic_functionality():
    """Test basic functionality of the Python bindings"""
    print("\n=== Testing Basic Functionality ===")
    
    try:
        # Test import
        print("Testing import...")
        import pyfolio_cpp as pf
        print(f"✓ Successfully imported pyfolio_cpp version {pf.version()}")
        
        # Test DateTime
        print("Testing DateTime...")
        dt = pf.DateTime(2023, 6, 15)
        assert dt.year() == 2023
        assert dt.month() == 6
        assert dt.day() == 15
        print(f"✓ DateTime works: {dt.to_string()}")
        
        # Test TimeSeries creation
        print("Testing TimeSeries...")
        import numpy as np
        
        n_points = 100
        timestamps = np.arange(1640995200, 1640995200 + n_points * 86400, 86400, dtype=float)
        values = np.random.normal(0.001, 0.02, n_points)
        
        ts = pf.TimeSeries.create(timestamps, values, "test_series")
        assert ts.size() == n_points
        print(f"✓ TimeSeries created with {ts.size()} points")
        
        # Test analytics
        print("Testing analytics...")
        metrics = pf.analytics.calculate_performance_metrics(ts)
        assert hasattr(metrics, 'sharpe_ratio')
        print(f"✓ Performance metrics calculated: Sharpe = {metrics.sharpe_ratio:.3f}")
        
        # Test risk analytics
        print("Testing risk analytics...")
        var_95 = pf.analytics.calculate_var(ts, 0.05)
        cvar_95 = pf.analytics.calculate_cvar(ts, 0.05)
        print(f"✓ Risk metrics: VaR(95%) = {var_95:.4f}, CVaR(95%) = {cvar_95:.4f}")
        
        # Test rolling metrics
        print("Testing rolling metrics...")
        if ts.size() >= 21:
            rolling_sharpe = pf.analytics.rolling_sharpe(ts, 21, 0.0)
            print(f"✓ Rolling Sharpe calculated: {rolling_sharpe.size()} points")
        
        # Test sample data creation
        print("Testing sample data creation...")
        start_date = pf.DateTime(2023, 1, 1)
        end_date = pf.DateTime(2023, 3, 31)
        sample_data = pf.create_sample_data(start_date, end_date, 100.0, 0.02)
        print(f"✓ Sample data created: {sample_data.size()} points")
        
        print("\n✓ All basic functionality tests passed!")
        return True
        
    except Exception as e:
        print(f"\n✗ Basic functionality test failed: {e}")
        import traceback
        traceback.print_exc()
        return False

def test_numpy_integration():
    """Test NumPy integration"""
    print("\n=== Testing NumPy Integration ===")
    
    try:
        import numpy as np
        import pyfolio_cpp as pf
        
        # Create large NumPy arrays
        n_points = 10000
        timestamps = np.arange(1640995200, 1640995200 + n_points * 86400, 86400, dtype=float)
        returns = np.random.normal(0.0005, 0.02, n_points)
        
        print(f"Created NumPy arrays with {n_points} points")
        
        # Convert to TimeSeries
        ts = pf.TimeSeries.create(timestamps, returns, "numpy_test")
        print(f"✓ Converted to TimeSeries: {ts.size()} points")
        
        # Convert back to NumPy
        ts_back, vals_back = ts.to_numpy()
        
        # Verify data integrity
        max_ts_diff = np.abs(timestamps - ts_back).max()
        max_val_diff = np.abs(returns - vals_back).max()
        
        print(f"✓ Data integrity check:")
        print(f"  Max timestamp difference: {max_ts_diff}")
        print(f"  Max value difference: {max_val_diff}")
        
        assert max_ts_diff < 1e-6
        assert max_val_diff < 1e-10
        
        # Performance test
        import time
        
        start_time = time.time()
        metrics = pf.analytics.calculate_performance_metrics(ts)
        calc_time = time.time() - start_time
        
        print(f"✓ Performance test: {calc_time:.4f} seconds for {n_points} points")
        print(f"  Calculated Sharpe ratio: {metrics.sharpe_ratio:.3f}")
        
        return True
        
    except Exception as e:
        print(f"✗ NumPy integration test failed: {e}")
        import traceback
        traceback.print_exc()
        return False

def test_pandas_integration():
    """Test pandas integration"""
    print("\n=== Testing Pandas Integration ===")
    
    try:
        import pandas as pd
        import numpy as np
        import pyfolio_cpp as pf
        
        # Create pandas DataFrame
        dates = pd.date_range('2023-01-01', '2023-12-31', freq='D')
        returns = np.random.normal(0.0008, 0.015, len(dates))
        df = pd.DataFrame({'returns': returns}, index=dates)
        
        print(f"Created pandas DataFrame with {len(df)} rows")
        
        # Convert to PyFolio TimeSeries
        timestamps = np.array([ts.timestamp() for ts in df.index])
        values = df['returns'].values
        
        ts = pf.TimeSeries.create(timestamps, values, "pandas_returns")
        print(f"✓ Converted to TimeSeries: {ts.size()} points")
        
        # Calculate metrics
        metrics = pf.analytics.calculate_performance_metrics(ts)
        print(f"✓ Calculated metrics from pandas data:")
        print(f"  Annual Return: {metrics.annual_return*100:.2f}%")
        print(f"  Sharpe Ratio: {metrics.sharpe_ratio:.3f}")
        
        # Convert results back to pandas
        ts_numpy, vals_numpy = ts.to_numpy()
        result_series = pd.Series(
            vals_numpy, 
            index=pd.to_datetime(ts_numpy, unit='s'),
            name='pyfolio_returns'
        )
        
        print(f"✓ Converted back to pandas Series: {len(result_series)} points")
        
        # Verify data integrity
        original_values = df['returns'].values
        converted_values = result_series.values
        max_diff = np.abs(original_values - converted_values).max()
        
        print(f"✓ Data integrity: max difference = {max_diff}")
        assert max_diff < 1e-10
        
        return True
        
    except ImportError:
        print("pandas not available - skipping pandas integration test")
        return True
    except Exception as e:
        print(f"✗ Pandas integration test failed: {e}")
        import traceback
        traceback.print_exc()
        return False

def run_performance_benchmark():
    """Run a simple performance benchmark"""
    print("\n=== Performance Benchmark ===")
    
    try:
        import time
        import numpy as np
        import pyfolio_cpp as pf
        
        # Test different dataset sizes
        sizes = [1000, 10000, 100000]
        
        for size in sizes:
            print(f"\nTesting with {size} data points:")
            
            # Create data
            timestamps = np.arange(1640995200, 1640995200 + size * 86400, 86400, dtype=float)
            returns = np.random.normal(0.0008, 0.02, size)
            
            # Time TimeSeries creation
            start_time = time.time()
            ts = pf.TimeSeries.create(timestamps, returns, f"benchmark_{size}")
            creation_time = time.time() - start_time
            
            # Time performance metrics calculation
            start_time = time.time()
            metrics = pf.analytics.calculate_performance_metrics(ts)
            calc_time = time.time() - start_time
            
            print(f"  Creation time: {creation_time:.4f}s")
            print(f"  Metrics calculation: {calc_time:.4f}s")
            print(f"  Sharpe ratio: {metrics.sharpe_ratio:.3f}")
            
            # Rolling metrics test (if enough data)
            if size >= 252:
                start_time = time.time()
                rolling_sharpe = pf.analytics.rolling_sharpe(ts, 21, 0.0)
                rolling_time = time.time() - start_time
                print(f"  Rolling Sharpe (21-day): {rolling_time:.4f}s ({rolling_sharpe.size()} points)")
        
        return True
        
    except Exception as e:
        print(f"✗ Performance benchmark failed: {e}")
        import traceback
        traceback.print_exc()
        return False

def main():
    """Main function to run all tests"""
    print("=== PyFolio C++ Python Bindings Build and Test ===")
    
    # Check dependencies
    if not check_dependencies():
        print("\n✗ Missing dependencies. Please install required packages.")
        return 1
    
    # Build project
    if not build_project():
        print("\n✗ Build failed. Cannot proceed with tests.")
        return 1
    
    # Run tests
    tests = [
        test_basic_functionality,
        test_numpy_integration,
        test_pandas_integration,
        run_performance_benchmark,
    ]
    
    all_passed = True
    for test in tests:
        if not test():
            all_passed = False
    
    # Summary
    print("\n" + "="*60)
    if all_passed:
        print("✓ ALL TESTS PASSED!")
        print("\nPyFolio C++ Python bindings are working correctly.")
        print("You can now use the library in your Python projects:")
        print("  import pyfolio_cpp as pf")
        print("  print(pf.version())")
        return 0
    else:
        print("✗ SOME TESTS FAILED!")
        print("\nPlease check the error messages above and fix any issues.")
        return 1

if __name__ == "__main__":
    sys.exit(main())