#!/bin/bash
#
# Static Analysis Script for pyfolio_cpp
# Runs clang-tidy, cppcheck, and clang-format on the codebase
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"
ANALYSIS_OUTPUT_DIR="${BUILD_DIR}/static_analysis"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== Pyfolio C++ Static Analysis ===${NC}"
echo "Project root: ${PROJECT_ROOT}"
echo "Build directory: ${BUILD_DIR}"
echo "Analysis output: ${ANALYSIS_OUTPUT_DIR}"
echo

# Create directories
mkdir -p "${ANALYSIS_OUTPUT_DIR}"
mkdir -p "${PROJECT_ROOT}/scripts"

# Check for required tools
echo -e "${BLUE}Checking for static analysis tools...${NC}"

CLANG_TIDY=$(which clang-tidy 2>/dev/null || echo "")
CPPCHECK=$(which cppcheck 2>/dev/null || echo "")
CLANG_FORMAT=$(which clang-format 2>/dev/null || echo "")

if [[ -n "$CLANG_TIDY" ]]; then
    echo -e "${GREEN}✓ clang-tidy found: ${CLANG_TIDY}${NC}"
else
    echo -e "${YELLOW}⚠ clang-tidy not found${NC}"
fi

if [[ -n "$CPPCHECK" ]]; then
    echo -e "${GREEN}✓ cppcheck found: ${CPPCHECK}${NC}"
else
    echo -e "${YELLOW}⚠ cppcheck not found${NC}"
fi

if [[ -n "$CLANG_FORMAT" ]]; then
    echo -e "${GREEN}✓ clang-format found: ${CLANG_FORMAT}${NC}"
else
    echo -e "${YELLOW}⚠ clang-format not found${NC}"
fi

echo

# Parse command line arguments
FORMAT_ONLY=false
TIDY_ONLY=false
CPPCHECK_ONLY=false
HELP=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --format-only)
            FORMAT_ONLY=true
            shift
            ;;
        --tidy-only)
            TIDY_ONLY=true
            shift
            ;;
        --cppcheck-only)
            CPPCHECK_ONLY=true
            shift
            ;;
        --help|-h)
            HELP=true
            shift
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            HELP=true
            shift
            ;;
    esac
done

if [[ "$HELP" == true ]]; then
    echo "Usage: $0 [OPTIONS]"
    echo
    echo "Options:"
    echo "  --format-only     Run only clang-format"
    echo "  --tidy-only       Run only clang-tidy"
    echo "  --cppcheck-only   Run only cppcheck"
    echo "  --help, -h        Show this help message"
    echo
    echo "If no options are specified, all available tools will be run."
    exit 0
fi

# Function to run clang-format
run_clang_format() {
    if [[ -z "$CLANG_FORMAT" ]]; then
        echo -e "${YELLOW}Skipping clang-format (not available)${NC}"
        return
    fi
    
    echo -e "${BLUE}Running clang-format...${NC}"
    
    # Find all C++ files
    find "${PROJECT_ROOT}/include" "${PROJECT_ROOT}/src" "${PROJECT_ROOT}/examples" "${PROJECT_ROOT}/tests" \
        -name "*.h" -o -name "*.hpp" -o -name "*.cpp" -o -name "*.cc" 2>/dev/null | \
    while read -r file; do
        echo "Formatting: $(basename "$file")"
        "$CLANG_FORMAT" -i -style=file "$file"
    done
    
    echo -e "${GREEN}✓ Code formatting completed${NC}"
    echo
}

# Function to run clang-tidy
run_clang_tidy() {
    if [[ -z "$CLANG_TIDY" ]]; then
        echo -e "${YELLOW}Skipping clang-tidy (not available)${NC}"
        return
    fi
    
    echo -e "${BLUE}Running clang-tidy analysis...${NC}"
    
    local output_file="${ANALYSIS_OUTPUT_DIR}/clang-tidy-report.txt"
    local summary_file="${ANALYSIS_OUTPUT_DIR}/clang-tidy-summary.txt"
    
    # Clear previous report
    > "$output_file"
    > "$summary_file"
    
    # Find all C++ files and run clang-tidy
    find "${PROJECT_ROOT}/include" "${PROJECT_ROOT}/src" \
        -name "*.h" -o -name "*.hpp" -o -name "*.cpp" -o -name "*.cc" 2>/dev/null | \
    while read -r file; do
        echo "Analyzing: $(basename "$file")"
        "$CLANG_TIDY" --config-file="${PROJECT_ROOT}/.clang-tidy" \
                      --header-filter="${PROJECT_ROOT}/include/.*" \
                      "$file" >> "$output_file" 2>&1 || true
    done
    
    # Generate summary
    echo "=== Clang-Tidy Analysis Summary ===" > "$summary_file"
    echo "Generated: $(date)" >> "$summary_file"
    echo >> "$summary_file"
    
    # Count warnings and errors
    local warning_count=$(grep -c "warning:" "$output_file" 2>/dev/null || echo "0")
    local error_count=$(grep -c "error:" "$output_file" 2>/dev/null || echo "0")
    local note_count=$(grep -c "note:" "$output_file" 2>/dev/null || echo "0")
    
    echo "Issues found:" >> "$summary_file"
    echo "  Errors: $error_count" >> "$summary_file"
    echo "  Warnings: $warning_count" >> "$summary_file"
    echo "  Notes: $note_count" >> "$summary_file"
    echo >> "$summary_file"
    
    # Show most common warnings
    echo "Most common warnings:" >> "$summary_file"
    grep "warning:" "$output_file" 2>/dev/null | \
        sed 's/.*warning: \([^[]*\).*/\1/' | \
        sort | uniq -c | sort -nr | head -10 >> "$summary_file" 2>/dev/null || true
    
    echo -e "${GREEN}✓ clang-tidy analysis completed${NC}"
    echo "  Report: $output_file"
    echo "  Summary: $summary_file"
    echo "  Errors: $error_count, Warnings: $warning_count, Notes: $note_count"
    echo
}

# Function to run cppcheck
run_cppcheck() {
    if [[ -z "$CPPCHECK" ]]; then
        echo -e "${YELLOW}Skipping cppcheck (not available)${NC}"
        return
    fi
    
    echo -e "${BLUE}Running cppcheck analysis...${NC}"
    
    local xml_output="${ANALYSIS_OUTPUT_DIR}/cppcheck-report.xml"
    local txt_output="${ANALYSIS_OUTPUT_DIR}/cppcheck-report.txt"
    local summary_file="${ANALYSIS_OUTPUT_DIR}/cppcheck-summary.txt"
    
    # Run cppcheck with XML output
    "$CPPCHECK" --enable=all \
                --std=c++20 \
                --suppress=missingIncludeSystem \
                --inline-suppr \
                --xml \
                --xml-version=2 \
                --include="${PROJECT_ROOT}/include" \
                "${PROJECT_ROOT}/include" \
                "${PROJECT_ROOT}/src" \
                2> "$xml_output" || true
    
    # Convert XML to human-readable format
    "$CPPCHECK" --enable=all \
                --std=c++20 \
                --suppress=missingIncludeSystem \
                --inline-suppr \
                --include="${PROJECT_ROOT}/include" \
                "${PROJECT_ROOT}/include" \
                "${PROJECT_ROOT}/src" \
                2> "$txt_output" || true
    
    # Generate summary
    echo "=== Cppcheck Analysis Summary ===" > "$summary_file"
    echo "Generated: $(date)" >> "$summary_file"
    echo >> "$summary_file"
    
    # Count issues by severity
    local error_count=$(grep -c "error" "$txt_output" 2>/dev/null || echo "0")
    local warning_count=$(grep -c "warning" "$txt_output" 2>/dev/null || echo "0")
    local style_count=$(grep -c "style" "$txt_output" 2>/dev/null || echo "0")
    local performance_count=$(grep -c "performance" "$txt_output" 2>/dev/null || echo "0")
    
    echo "Issues found:" >> "$summary_file"
    echo "  Errors: $error_count" >> "$summary_file"
    echo "  Warnings: $warning_count" >> "$summary_file"
    echo "  Style: $style_count" >> "$summary_file"
    echo "  Performance: $performance_count" >> "$summary_file"
    echo >> "$summary_file"
    
    echo -e "${GREEN}✓ cppcheck analysis completed${NC}"
    echo "  XML Report: $xml_output"
    echo "  Text Report: $txt_output"
    echo "  Summary: $summary_file"
    echo "  Errors: $error_count, Warnings: $warning_count, Style: $style_count, Performance: $performance_count"
    echo
}

# Main execution
echo -e "${BLUE}Starting static analysis...${NC}"
echo

if [[ "$FORMAT_ONLY" == true ]]; then
    run_clang_format
elif [[ "$TIDY_ONLY" == true ]]; then
    run_clang_tidy
elif [[ "$CPPCHECK_ONLY" == true ]]; then
    run_cppcheck
else
    # Run all available tools
    run_clang_format
    run_clang_tidy
    run_cppcheck
fi

echo -e "${GREEN}=== Static Analysis Complete ===${NC}"
echo "All reports are available in: ${ANALYSIS_OUTPUT_DIR}/"
echo

# Generate combined summary if we ran multiple tools
if [[ "$FORMAT_ONLY" != true && "$TIDY_ONLY" != true && "$CPPCHECK_ONLY" != true ]]; then
    combined_summary="${ANALYSIS_OUTPUT_DIR}/combined-summary.txt"
    echo "=== Combined Static Analysis Summary ===" > "$combined_summary"
    echo "Generated: $(date)" >> "$combined_summary"
    echo >> "$combined_summary"
    
    if [[ -f "${ANALYSIS_OUTPUT_DIR}/clang-tidy-summary.txt" ]]; then
        cat "${ANALYSIS_OUTPUT_DIR}/clang-tidy-summary.txt" >> "$combined_summary"
        echo >> "$combined_summary"
    fi
    
    if [[ -f "${ANALYSIS_OUTPUT_DIR}/cppcheck-summary.txt" ]]; then
        cat "${ANALYSIS_OUTPUT_DIR}/cppcheck-summary.txt" >> "$combined_summary"
        echo >> "$combined_summary"
    fi
    
    echo "Combined summary: $combined_summary"
fi