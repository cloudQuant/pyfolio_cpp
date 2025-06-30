#!/bin/bash

# Simple test coverage analysis for passing tests
set -e

echo "Starting simple test coverage analysis..."

# Build only the tests that compile successfully
echo "Building passing tests..."
make test_core test_datetime_only test_statistics_only test_time_series_only \
     test_performance_only test_transactions_only test_positions_only \
     test_attribution_only test_risk_only test_bayesian_only \
     test_capacity_only test_regime_only test_integration_only \
     test_tear_sheets_only test_rolling_metrics_only test_data_loader_only \
     test_visualization_only test_cached_performance_only \
     test_parallel_algorithms_only test_plotly_enhanced_only \
     test_streaming_analysis_only test_gpu_acceleration_only \
     test_options_pricing test_advanced_backtesting_only -j$(nproc) || echo "Some tests failed to build"

# Run the tests that were built successfully
echo "Running available tests..."
TESTS_TO_RUN=""

# Check which test executables exist and add them to the list
for test in test_core test_datetime_only test_statistics_only test_time_series_only \
            test_performance_only test_transactions_only test_positions_only \
            test_attribution_only test_risk_only test_bayesian_only \
            test_capacity_only test_regime_only test_integration_only \
            test_tear_sheets_only test_rolling_metrics_only test_data_loader_only \
            test_visualization_only test_cached_performance_only \
            test_parallel_algorithms_only test_plotly_enhanced_only \
            test_streaming_analysis_only test_gpu_acceleration_only \
            test_options_pricing test_advanced_backtesting_only; do
    if [ -f "tests/$test" ]; then
        echo "Found test: $test"
        TESTS_TO_RUN="$TESTS_TO_RUN tests/$test"
    fi
done

# Run available tests
echo "Running tests: $TESTS_TO_RUN"
for test in $TESTS_TO_RUN; do
    echo "Running $test..."
    if ./$test --gtest_output=xml:${test##*/}_results.xml; then
        echo "✓ $test passed"
    else
        echo "✗ $test failed"
    fi
done

# Count passing tests
PASSING_TESTS=$(echo $TESTS_TO_RUN | wc -w)
echo "Successfully ran $PASSING_TESTS test executables"

# Generate basic coverage report if any coverage data exists
if command -v lcov &> /dev/null; then
    echo "Generating coverage report..."
    
    # Capture coverage data
    lcov --capture --directory . --output-file coverage_simple.info 2>/dev/null || true
    
    if [ -f coverage_simple.info ]; then
        # Remove system files
        lcov --remove coverage_simple.info '/usr/*' '_deps/*' --output-file coverage_filtered.info 2>/dev/null || true
        
        if [ -f coverage_filtered.info ]; then
            # Generate summary
            echo "Coverage Summary:"
            lcov --list coverage_filtered.info 2>/dev/null || echo "Could not generate coverage summary"
            
            # Generate HTML report
            if command -v genhtml &> /dev/null; then
                genhtml coverage_filtered.info --output-directory coverage_html_simple 2>/dev/null || true
                if [ -d coverage_html_simple ]; then
                    echo "HTML coverage report generated: coverage_html_simple/index.html"
                fi
            fi
        fi
    fi
else
    echo "lcov not available, skipping coverage report"
fi

echo "Simple coverage analysis complete!"