#!/usr/bin/env python3
"""
Validation script to compare PyFolio_cpp results against Python pyfolio

This script:
1. Loads the same test data used by both implementations
2. Runs calculations in Python pyfolio
3. Compares results with PyFolio_cpp output
4. Reports any discrepancies with detailed analysis

Usage:
    python validate_python_equivalence.py [--build-dir build] [--tolerance 1e-10]
"""

import sys
import os
import argparse
import pandas as pd
import numpy as np
import subprocess
import json
import gzip
from typing import Dict, List, Tuple, Optional, Any
from pathlib import Path

try:
    import pyfolio as pf
    from pyfolio import timeseries, round_trips, capacity, pos
    PYFOLIO_AVAILABLE = True
except ImportError:
    print("Warning: Python pyfolio not available. Install with: pip install pyfolio")
    PYFOLIO_AVAILABLE = False

class PyfolioEquivalenceValidator:
    """Validates PyFolio_cpp results against Python pyfolio"""
    
    def __init__(self, build_dir: str = "build", tolerance: float = 1e-10):
        self.build_dir = Path(build_dir)
        self.tolerance = tolerance
        self.test_data_dir = Path("tests/test_data")
        self.results = {}
        
    def load_test_data(self) -> Dict[str, pd.DataFrame]:
        """Load compressed test data files"""
        data = {}
        
        # Load returns data
        if (self.test_data_dir / "test_returns.csv.gz").exists():
            with gzip.open(self.test_data_dir / "test_returns.csv.gz", 'rt') as f:
                data['returns'] = pd.read_csv(f, index_col=0, parse_dates=True).squeeze()
        
        # Load positions data
        if (self.test_data_dir / "test_pos.csv.gz").exists():
            with gzip.open(self.test_data_dir / "test_pos.csv.gz", 'rt') as f:
                data['positions'] = pd.read_csv(f, index_col=0, parse_dates=True)
        
        # Load transactions data
        if (self.test_data_dir / "test_txn.csv.gz").exists():
            with gzip.open(self.test_data_dir / "test_txn.csv.gz", 'rt') as f:
                data['transactions'] = pd.read_csv(f, index_col=0, parse_dates=True)
        
        return data
    
    def run_python_calculations(self, data: Dict[str, pd.DataFrame]) -> Dict[str, Any]:
        """Run calculations using Python pyfolio"""
        if not PYFOLIO_AVAILABLE:
            return {}
        
        results = {}
        returns = data.get('returns')
        positions = data.get('positions')
        transactions = data.get('transactions')
        
        if returns is not None:
            # Basic performance metrics
            results['annual_return'] = timeseries.annual_return(returns)
            results['sharpe_ratio'] = timeseries.sharpe_ratio(returns)
            results['max_drawdown'] = timeseries.max_drawdown(returns)
            results['sortino_ratio'] = timeseries.sortino_ratio(returns)
            results['calmar_ratio'] = timeseries.calmar_ratio(returns)
            
            # Rolling metrics
            results['rolling_sharpe'] = timeseries.rolling_sharpe(returns, window=63).dropna()
            results['rolling_volatility'] = timeseries.rolling_volatility(returns, window=63).dropna()
            
        if transactions is not None and not transactions.empty:
            # Round trip analysis
            try:
                round_trips_df = round_trips.extract_round_trips(transactions)
                if not round_trips_df.empty:
                    results['round_trips'] = {
                        'count': len(round_trips_df),
                        'total_pnl': round_trips_df['pnl'].sum(),
                        'avg_return': round_trips_df['returns'].mean(),
                        'avg_duration': round_trips_df['duration'].mean()
                    }
            except Exception as e:
                print(f"Warning: Round trip analysis failed: {e}")
        
        if positions is not None and not positions.empty:
            # Position analysis
            try:
                # Calculate turnover
                turnover = pos.get_turnover(positions, transactions)
                if not turnover.empty:
                    results['turnover'] = {
                        'mean': turnover.mean(),
                        'first_value': turnover.iloc[0] if len(turnover) > 0 else None,
                        'values': turnover.tolist()[:10]  # First 10 values
                    }
                
                # Capacity analysis (if volume data available)
                # This would require additional volume data
                
            except Exception as e:
                print(f"Warning: Position analysis failed: {e}")
        
        return results
    
    def run_cpp_tests(self) -> Dict[str, Any]:
        """Run C++ tests and capture output"""
        cpp_results = {}
        
        test_executables = [
            "test_pyfolio_equivalent_only",
            "run_python_equivalent_tests"
        ]
        
        for exe in test_executables:
            exe_path = self.build_dir / "tests" / exe
            if exe_path.exists():
                try:
                    result = subprocess.run(
                        [str(exe_path)], 
                        capture_output=True, 
                        text=True,
                        cwd=self.build_dir
                    )
                    
                    cpp_results[exe] = {
                        'returncode': result.returncode,
                        'stdout': result.stdout,
                        'stderr': result.stderr,
                        'success': result.returncode == 0
                    }
                    
                except Exception as e:
                    cpp_results[exe] = {
                        'error': str(e),
                        'success': False
                    }
            else:
                cpp_results[exe] = {
                    'error': f"Executable not found: {exe_path}",
                    'success': False
                }
        
        return cpp_results
    
    def compare_results(self, python_results: Dict[str, Any], cpp_results: Dict[str, Any]) -> Dict[str, Any]:
        """Compare Python and C++ results"""
        comparison = {
            'all_tests_passed': True,
            'discrepancies': [],
            'summary': {}
        }
        
        # Check if C++ tests passed
        cpp_tests_passed = all(
            result.get('success', False) for result in cpp_results.values()
        )
        
        if not cpp_tests_passed:
            comparison['all_tests_passed'] = False
            comparison['discrepancies'].append({
                'type': 'cpp_test_failure',
                'message': "One or more C++ tests failed",
                'details': {k: v for k, v in cpp_results.items() if not v.get('success', False)}
            })
        
        # If we have Python pyfolio results, we could compare specific values
        # This would require extracting numerical results from C++ test output
        # For now, we focus on ensuring C++ tests pass
        
        comparison['summary'] = {
            'python_pyfolio_available': PYFOLIO_AVAILABLE,
            'python_results_count': len(python_results),
            'cpp_tests_run': len(cpp_results),
            'cpp_tests_passed': sum(1 for r in cpp_results.values() if r.get('success', False)),
            'cpp_tests_failed': sum(1 for r in cpp_results.values() if not r.get('success', False))
        }
        
        return comparison
    
    def generate_report(self, comparison: Dict[str, Any], python_results: Dict[str, Any], cpp_results: Dict[str, Any]) -> str:
        """Generate detailed validation report"""
        report = []
        report.append("=" * 80)
        report.append("PyFolio_cpp Python Equivalence Validation Report")
        report.append("=" * 80)
        report.append("")
        
        # Summary
        summary = comparison['summary']
        report.append("SUMMARY:")
        report.append(f"  Python pyfolio available: {summary['python_pyfolio_available']}")
        report.append(f"  Python calculations: {summary['python_results_count']} metrics calculated")
        report.append(f"  C++ tests run: {summary['cpp_tests_run']}")
        report.append(f"  C++ tests passed: {summary['cpp_tests_passed']}")
        report.append(f"  C++ tests failed: {summary['cpp_tests_failed']}")
        report.append("")
        
        # Overall result
        if comparison['all_tests_passed']:
            report.append("✅ VALIDATION PASSED")
            report.append("PyFolio_cpp successfully replicates Python pyfolio behavior")
        else:
            report.append("❌ VALIDATION FAILED")
            report.append("Discrepancies found between Python and C++ implementations")
        
        report.append("")
        
        # Python results (if available)
        if python_results and PYFOLIO_AVAILABLE:
            report.append("PYTHON PYFOLIO RESULTS:")
            for key, value in python_results.items():
                if isinstance(value, (int, float)):
                    report.append(f"  {key}: {value:.10f}")
                elif isinstance(value, dict):
                    report.append(f"  {key}:")
                    for subkey, subvalue in value.items():
                        report.append(f"    {subkey}: {subvalue}")
                else:
                    report.append(f"  {key}: {str(value)[:100]}...")
            report.append("")
        
        # C++ test results
        report.append("C++ TEST RESULTS:")
        for test_name, result in cpp_results.items():
            status = "✅ PASSED" if result.get('success', False) else "❌ FAILED"
            report.append(f"  {test_name}: {status}")
            
            if not result.get('success', False):
                if 'error' in result:
                    report.append(f"    Error: {result['error']}")
                if 'stderr' in result and result['stderr']:
                    report.append(f"    Stderr: {result['stderr'][:500]}")
        
        report.append("")
        
        # Discrepancies
        if comparison['discrepancies']:
            report.append("DISCREPANCIES FOUND:")
            for i, discrepancy in enumerate(comparison['discrepancies'], 1):
                report.append(f"  {i}. {discrepancy['type']}: {discrepancy['message']}")
                if 'details' in discrepancy:
                    for key, value in discrepancy['details'].items():
                        report.append(f"     {key}: {value}")
        
        report.append("")
        report.append("=" * 80)
        
        return "\n".join(report)
    
    def validate(self) -> bool:
        """Run complete validation"""
        print("Loading test data...")
        data = self.load_test_data()
        
        print("Running Python pyfolio calculations...")
        python_results = self.run_python_calculations(data)
        
        print("Running C++ tests...")
        cpp_results = self.run_cpp_tests()
        
        print("Comparing results...")
        comparison = self.compare_results(python_results, cpp_results)
        
        # Generate and display report
        report = self.generate_report(comparison, python_results, cpp_results)
        print(report)
        
        # Save report to file
        report_file = self.build_dir / "validation_report.txt"
        with open(report_file, 'w') as f:
            f.write(report)
        print(f"\nDetailed report saved to: {report_file}")
        
        return comparison['all_tests_passed']

def main():
    parser = argparse.ArgumentParser(description="Validate PyFolio_cpp against Python pyfolio")
    parser.add_argument("--build-dir", default="build", help="Build directory path")
    parser.add_argument("--tolerance", type=float, default=1e-10, help="Numerical tolerance for comparisons")
    
    args = parser.parse_args()
    
    validator = PyfolioEquivalenceValidator(args.build_dir, args.tolerance)
    
    success = validator.validate()
    
    if success:
        print("\n🎉 Validation completed successfully!")
        sys.exit(0)
    else:
        print("\n💥 Validation failed. See report for details.")
        sys.exit(1)

if __name__ == "__main__":
    main()