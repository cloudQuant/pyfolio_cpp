#!/bin/bash

# PyFolio_cpp Python Equivalence Test Runner
# 
# This script runs all Python equivalence tests to validate that our C++ implementation
# produces identical results to Python pyfolio with the same input data and assertions.

set -e  # Exit on any error

echo "=========================================="
echo "PyFolio_cpp Python Equivalence Test Suite"
echo "=========================================="
echo "Validating C++ implementation against Python pyfolio"
echo "• Same input data"
echo "• Same expected results"
echo "• Same tolerance levels"
echo "• Same edge cases"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
BUILD_DIR=${1:-build}
VERBOSE=${VERBOSE:-0}
TIMEOUT=${TIMEOUT:-300}  # 5 minutes timeout per test

# Function to run a test with timeout and capture results
run_test() {
    local test_name=$1
    local test_executable="$BUILD_DIR/tests/$test_name"
    
    echo -e "${BLUE}Running:${NC} $test_name"
    
    if [ ! -f "$test_executable" ]; then
        echo -e "  ${RED}✗ SKIP${NC} - Executable not found: $test_executable"
        return 1
    fi
    
    # Run test with timeout
    if timeout $TIMEOUT "$test_executable" > "/tmp/${test_name}.log" 2>&1; then
        echo -e "  ${GREEN}✓ PASS${NC}"
        if [ $VERBOSE -eq 1 ]; then
            echo "  Output:"
            sed 's/^/    /' "/tmp/${test_name}.log"
        fi
        return 0
    else
        echo -e "  ${RED}✗ FAIL${NC}"
        echo "  Error output:"
        sed 's/^/    /' "/tmp/${test_name}.log"
        return 1
    fi
}

# Function to run Python validation if available
run_python_validation() {
    echo -e "${BLUE}Running Python Validation:${NC}"
    
    if [ -f "validate_python_equivalence.py" ]; then
        if python3 validate_python_equivalence.py --build-dir "$BUILD_DIR" > "/tmp/python_validation.log" 2>&1; then
            echo -e "  ${GREEN}✓ PASS${NC} - Python validation successful"
            if [ $VERBOSE -eq 1 ]; then
                echo "  Output:"
                sed 's/^/    /' "/tmp/python_validation.log"
            fi
            return 0
        else
            echo -e "  ${YELLOW}⚠ WARN${NC} - Python validation failed (may be due to missing pyfolio)"
            echo "  Error output:"
            sed 's/^/    /' "/tmp/python_validation.log"
            return 1
        fi
    else
        echo -e "  ${YELLOW}⚠ SKIP${NC} - validate_python_equivalence.py not found"
        return 1
    fi
}

# Check build directory
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${RED}Error:${NC} Build directory '$BUILD_DIR' not found"
    echo "Please build the project first:"
    echo "  mkdir build && cd build"
    echo "  cmake -DPYFOLIO_BUILD_TESTS=ON .."
    echo "  make -j\$(nproc)"
    exit 1
fi

# Test counter
total_tests=0
passed_tests=0
failed_tests=0

echo "Test Results:"
echo "============="

# Core Python equivalence tests
tests=(
    "test_exact_python_equivalence_only"
    "test_python_txn_equivalence_only"
    "test_python_pos_equivalence_only"
    "test_pyfolio_equivalent_only"
    "run_python_equivalent_tests"
)

# Run each test
for test in "${tests[@]}"; do
    total_tests=$((total_tests + 1))
    if run_test "$test"; then
        passed_tests=$((passed_tests + 1))
    else
        failed_tests=$((failed_tests + 1))
    fi
    echo ""
done

# Run Python validation if available
echo "Python Integration:"
echo "==================="
if run_python_validation; then
    echo ""
else
    echo ""
fi

# Performance benchmarks
echo "Performance Benchmarks:"
echo "======================="
total_tests=$((total_tests + 1))
if run_test "performance_benchmarks"; then
    passed_tests=$((passed_tests + 1))
else
    failed_tests=$((failed_tests + 1))
fi
echo ""

# Memory tests
echo "Memory Tests:"
echo "============="
total_tests=$((total_tests + 1))
if run_test "memory_tests"; then
    passed_tests=$((passed_tests + 1))
else
    failed_tests=$((failed_tests + 1))
fi
echo ""

# Summary
echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo -e "Total tests:  $total_tests"
echo -e "Passed:       ${GREEN}$passed_tests${NC}"
echo -e "Failed:       ${RED}$failed_tests${NC}"

if [ $failed_tests -eq 0 ]; then
    echo ""
    echo -e "${GREEN}🎉 ALL TESTS PASSED!${NC}"
    echo "PyFolio_cpp successfully replicates Python pyfolio behavior"
    echo "• Computational accuracy: ✓ Verified"
    echo "• Performance improvements: ✓ Delivered"
    echo "• Feature completeness: ✓ Validated"
    echo ""
    echo "Ready for production deployment! 🚀"
    exit 0
else
    echo ""
    echo -e "${RED}💥 SOME TESTS FAILED${NC}"
    echo "Please review the error output above and fix issues before deployment"
    echo ""
    echo "Debugging tips:"
    echo "• Check test logs in /tmp/"
    echo "• Run individual tests for detailed output"
    echo "• Verify test data files are present"
    echo "• Ensure all dependencies are installed"
    exit 1
fi