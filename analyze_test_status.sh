#!/bin/bash

# Analyze test status and pass rate
set +e  # Don't exit on errors

echo "PyFolio C++ Test Status Analysis"
echo "================================"
echo

# Find all test executables
TESTS=$(find . -name "test_*_only" -type f | sort)
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
TOTAL_INDIVIDUAL_TESTS=0
PASSED_INDIVIDUAL_TESTS=0

echo "Running all available test suites..."
echo

for test in $TESTS; do
    echo "Running $test..."
    
    # Run the test and capture output
    output=$(timeout 60 $test 2>&1)
    exit_code=$?
    
    # Extract test counts from Google Test output
    if echo "$output" | grep -q "\[==========\]"; then
        # Extract individual test counts
        individual_tests=$(echo "$output" | grep "\[==========\].*ran\." | sed -n 's/.*\[\s*\([0-9]*\)\s*tests from.*/\1/p' | head -1)
        if [[ -n "$individual_tests" ]]; then
            TOTAL_INDIVIDUAL_TESTS=$((TOTAL_INDIVIDUAL_TESTS + individual_tests))
        fi
        
        # Count passed individual tests
        if echo "$output" | grep -q "\[  PASSED  \]"; then
            passed_individual=$(echo "$output" | grep "\[  PASSED  \]" | tail -1 | sed -n 's/.*\[\s*PASSED\s*\]\s*\([0-9]*\).*/\1/p')
            if [[ -n "$passed_individual" ]]; then
                PASSED_INDIVIDUAL_TESTS=$((PASSED_INDIVIDUAL_TESTS + passed_individual))
            fi
        fi
    fi
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    if [ $exit_code -eq 0 ]; then
        echo "  ✓ PASSED"
        PASSED_TESTS=$((PASSED_TESTS + 1))
    else
        echo "  ✗ FAILED (exit code: $exit_code)"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        
        # Show failure details
        if echo "$output" | grep -q "\[  FAILED  \]"; then
            echo "    Failed tests:"
            echo "$output" | grep "\[  FAILED  \]" | sed 's/^/      /'
        fi
    fi
    echo
done

echo "Test Suite Summary:"
echo "==================="
echo "Total test suites: $TOTAL_TESTS"
echo "Passed test suites: $PASSED_TESTS"
echo "Failed test suites: $FAILED_TESTS"

if [ $TOTAL_TESTS -gt 0 ]; then
    SUITE_PASS_RATE=$(echo "scale=2; $PASSED_TESTS * 100 / $TOTAL_TESTS" | bc -l 2>/dev/null || echo "N/A")
    echo "Test suite pass rate: $SUITE_PASS_RATE%"
fi

echo
echo "Individual Test Summary:"
echo "========================"
echo "Total individual tests: $TOTAL_INDIVIDUAL_TESTS"
echo "Passed individual tests: $PASSED_INDIVIDUAL_TESTS"

if [ $TOTAL_INDIVIDUAL_TESTS -gt 0 ]; then
    INDIVIDUAL_PASS_RATE=$(echo "scale=2; $PASSED_INDIVIDUAL_TESTS * 100 / $TOTAL_INDIVIDUAL_TESTS" | bc -l 2>/dev/null || echo "N/A")
    echo "Individual test pass rate: $INDIVIDUAL_PASS_RATE%"
fi

echo
echo "Test Coverage Analysis:"
echo "======================="

# Check if we can generate coverage data
if command -v lcov &> /dev/null; then
    echo "Generating coverage report..."
    
    # Reset coverage data
    lcov --zerocounters --directory . 2>/dev/null || true
    
    # Run a subset of passing tests to generate coverage
    PASSING_TESTS=""
    for test in $TESTS; do
        if timeout 30 $test >/dev/null 2>&1; then
            PASSING_TESTS="$PASSING_TESTS $test"
        fi
    done
    
    echo "Re-running passing tests for coverage: $(echo $PASSING_TESTS | wc -w) tests"
    for test in $PASSING_TESTS; do
        echo "  Running $test for coverage..."
        timeout 30 $test >/dev/null 2>&1 || true
    done
    
    # Capture coverage
    lcov --capture --directory . --output-file test_coverage.info 2>/dev/null || true
    
    if [ -f test_coverage.info ]; then
        # Filter out system files
        lcov --remove test_coverage.info '/usr/*' '_deps/*' 'tests/*' 'examples/*' \
             --output-file test_coverage_filtered.info 2>/dev/null || true
        
        if [ -f test_coverage_filtered.info ]; then
            echo
            echo "Coverage Summary:"
            lcov --list test_coverage_filtered.info 2>/dev/null | tail -20 || echo "Could not generate coverage summary"
            
            # Extract coverage percentage
            COVERAGE_PERCENT=$(lcov --list test_coverage_filtered.info 2>/dev/null | grep "Total:" | awk '{print $4}' | sed 's/%//' || echo "0")
            
            if [[ -n "$COVERAGE_PERCENT" ]]; then
                echo
                echo "Current test coverage: $COVERAGE_PERCENT%"
                
                if (( $(echo "$COVERAGE_PERCENT >= 100" | bc -l 2>/dev/null || echo 0) )); then
                    echo "🎉 EXCELLENT: 100% test coverage achieved!"
                elif (( $(echo "$COVERAGE_PERCENT >= 95" | bc -l 2>/dev/null || echo 0) )); then
                    echo "✅ VERY GOOD: >95% test coverage"
                elif (( $(echo "$COVERAGE_PERCENT >= 90" | bc -l 2>/dev/null || echo 0) )); then
                    echo "✅ GOOD: >90% test coverage"
                elif (( $(echo "$COVERAGE_PERCENT >= 80" | bc -l 2>/dev/null || echo 0) )); then
                    echo "⚠️  ACCEPTABLE: >80% test coverage"
                else
                    echo "❌ NEEDS IMPROVEMENT: <80% test coverage"
                fi
            fi
            
            # Generate HTML report
            if command -v genhtml &> /dev/null; then
                genhtml test_coverage_filtered.info --output-directory coverage_html_final 2>/dev/null || true
                if [ -d coverage_html_final ]; then
                    echo "📊 HTML coverage report: coverage_html_final/index.html"
                fi
            fi
        fi
    fi
else
    echo "lcov not available - install with: brew install lcov"
fi

echo
echo "Recommendations for 100% Test Coverage:"
echo "======================================="

if [ $FAILED_TESTS -gt 0 ]; then
    echo "1. Fix $FAILED_TESTS failing test suites to improve pass rate"
fi

echo "2. Add missing test cases for uncovered code paths"
echo "3. Ensure edge cases and error conditions are tested"
echo "4. Add integration tests for complex workflows"
echo "5. Test exception handling and boundary conditions"

echo
echo "Analysis complete!"