# Getting Started with Pyfolio C++

This guide will help you get up and running with Pyfolio C++, a high-performance financial portfolio analysis library.

## Table of Contents

1. [Installation](#installation)
2. [Basic Usage](#basic-usage)
3. [Core Concepts](#core-concepts)
4. [Performance Tips](#performance-tips)
5. [Common Patterns](#common-patterns)
6. [Troubleshooting](#troubleshooting)

## Installation

### Prerequisites

- **C++20 Compiler**: GCC 11+, Clang 13+, or MSVC 2022+
- **CMake**: Version 3.25 or newer
- **Dependencies**: Automatically fetched via CMake

### Quick Build

```bash
git clone https://github.com/your-org/pyfolio_cpp.git
cd pyfolio_cpp
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Advanced Build Options

```bash
# Debug build with coverage
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON ..

# Release with documentation
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_DOCUMENTATION=ON ..

# With static analysis
cmake -DENABLE_CPPCHECK=ON -DENABLE_CLANG_TIDY=ON ..
```

## Basic Usage

### Your First Analysis

```cpp
#include <pyfolio/pyfolio.h>
#include <iostream>

int main() {
    using namespace pyfolio;
    
    // Create sample return data
    std::vector<DateTime> dates = {
        DateTime::parse("2024-01-01").value(),
        DateTime::parse("2024-01-02").value(),
        DateTime::parse("2024-01-03").value(),
        DateTime::parse("2024-01-04").value(),
        DateTime::parse("2024-01-05").value()
    };
    
    std::vector<Return> returns = {0.01, -0.02, 0.015, -0.005, 0.008};
    
    // Create time series
    TimeSeries<Return> portfolio_returns(dates, returns);
    
    // Calculate basic metrics
    auto sharpe = performance::sharpe_ratio(portfolio_returns);
    auto max_dd = performance::max_drawdown(portfolio_returns);
    auto volatility = performance::volatility(portfolio_returns);
    
    if (sharpe.is_ok() && max_dd.is_ok() && volatility.is_ok()) {
        std::cout << "Sharpe Ratio: " << sharpe.value() << std::endl;
        std::cout << "Max Drawdown: " << max_dd.value() * 100 << "%" << std::endl;
        std::cout << "Volatility: " << volatility.value() * 100 << "%" << std::endl;
    }
    
    return 0;
}
```

### Loading Data from Files

```cpp
#include <pyfolio/io/data_loader.h>

// Load returns from CSV
auto returns_result = pyfolio::io::load_returns_csv("portfolio_returns.csv");
if (returns_result.is_ok()) {
    const auto& returns = returns_result.value();
    
    // Calculate comprehensive metrics
    auto metrics = pyfolio::analytics::calculate_performance_metrics(returns);
    if (metrics.is_ok()) {
        const auto& m = metrics.value();
        std::cout << "Annual Return: " << m.annual_return * 100 << "%" << std::endl;
        std::cout << "Sharpe Ratio: " << m.sharpe_ratio << std::endl;
    }
}
```

## Core Concepts

### 1. Result<T> Error Handling

Pyfolio C++ uses the `Result<T>` monad for robust error handling:

```cpp
// Always check for errors
auto result = calculate_something();
if (result.is_error()) {
    std::cerr << "Error: " << result.error().message << std::endl;
    return;
}

// Use the value safely
auto value = result.value();
```

### 2. TimeSeries Container

The `TimeSeries<T>` class is the foundation for all time-based analysis:

```cpp
TimeSeries<Return> returns;
returns.reserve(10000);  // Pre-allocate for performance

// Add data efficiently
for (const auto& [date, ret] : data) {
    returns.push_back(date, ret);
}

// Automatic sorting and validation
returns.sort_by_time();  // O(n log n) worst case, O(n) if already sorted
```

### 3. SIMD Optimization

Operations automatically use SIMD when available:

```cpp
// This automatically uses AVX2/SSE2 if available
auto correlation = series1.correlation(series2);
auto dot_product = series1.dot(series2);

// Explicit SIMD control
#ifdef PYFOLIO_HAS_AVX2
    // AVX2-specific optimizations available
#endif
```

### 4. Parallel Processing

```cpp
#include <pyfolio/core/parallel_algorithms.h>

// Parallel metric calculation
auto metrics = pyfolio::parallel::calculate_metrics(
    large_portfolio, 
    std::thread::hardware_concurrency()
);
```

## Performance Tips

### 1. Memory Management

```cpp
// Pre-allocate containers
TimeSeries<Return> returns;
returns.reserve(expected_size);

// Use move semantics
auto processed = std::move(raw_data).process_returns();

// Memory pools for high-frequency trading
pyfolio::PoolAllocator<Transaction> allocator(1000000);
```

### 2. Efficient Data Access

```cpp
// Use spans for zero-copy operations
std::span<const double> data_view = returns.values();

// Batch operations when possible
auto [mean, std_dev] = calculate_mean_and_std(returns);
```

### 3. Rolling Calculations

```cpp
// Optimized O(n) rolling calculations
auto rolling_sharpe = returns.rolling(252, [](auto window) {
    return calculate_sharpe_ratio(window);
});

// Built-in optimized versions
auto rolling_mean = returns.rolling_mean(30);
auto rolling_std = returns.rolling_std(30);
```

## Common Patterns

### 1. Comprehensive Analysis

```cpp
void analyze_portfolio(const TimeSeries<Return>& returns, 
                      const TimeSeries<Return>& benchmark) {
    // Basic metrics
    auto metrics = pyfolio::analytics::calculate_performance_metrics(
        returns, benchmark, 0.02, 252);
    
    if (metrics.is_error()) {
        std::cerr << "Analysis failed: " << metrics.error().message << std::endl;
        return;
    }
    
    const auto& m = metrics.value();
    
    // Print results
    std::cout << "=== Portfolio Analysis ===" << std::endl;
    std::cout << "Annual Return: " << m.annual_return * 100 << "%" << std::endl;
    std::cout << "Volatility: " << m.annual_volatility * 100 << "%" << std::endl;
    std::cout << "Sharpe Ratio: " << m.sharpe_ratio << std::endl;
    std::cout << "Max Drawdown: " << m.max_drawdown * 100 << "%" << std::endl;
    std::cout << "Beta: " << m.beta << std::endl;
    std::cout << "Alpha: " << m.alpha * 100 << "%" << std::endl;
}
```

### 2. Risk Analysis

```cpp
#include <pyfolio/risk/var.h>

void risk_analysis(const TimeSeries<Return>& returns) {
    // Value at Risk
    auto var_95 = pyfolio::risk::value_at_risk(returns, 0.95);
    auto var_99 = pyfolio::risk::value_at_risk(returns, 0.99);
    
    // Conditional VaR
    auto cvar_95 = pyfolio::risk::conditional_var(returns, 0.95);
    
    std::cout << "VaR (95%): " << var_95.value() * 100 << "%" << std::endl;
    std::cout << "VaR (99%): " << var_99.value() * 100 << "%" << std::endl;
    std::cout << "CVaR (95%): " << cvar_95.value() * 100 << "%" << std::endl;
}
```

### 3. Performance Attribution

```cpp
void attribution_analysis(const TimeSeries<Return>& portfolio,
                         const TimeSeries<Return>& benchmark,
                         const PositionSeries& positions) {
    auto attribution = pyfolio::attribution::brinson_attribution(
        portfolio, benchmark, positions);
    
    if (attribution.is_ok()) {
        const auto& attr = attribution.value();
        std::cout << "Selection Effect: " << attr.selection_effect * 100 << "%" << std::endl;
        std::cout << "Allocation Effect: " << attr.allocation_effect * 100 << "%" << std::endl;
    }
}
```

### 4. Visualization

```cpp
#include <pyfolio/visualization/plotly_enhanced.h>

void create_tear_sheet(const TimeSeries<Return>& returns) {
    pyfolio::visualization::PlotlyEnhanced plotter;
    
    // Create interactive charts
    auto equity_curve = plotter.plot_cumulative_returns(returns);
    auto drawdown_plot = plotter.plot_drawdown(returns);
    auto monthly_heatmap = plotter.plot_monthly_returns_heatmap(returns);
    
    // Save to HTML
    plotter.save_tear_sheet("portfolio_analysis.html", {
        equity_curve, drawdown_plot, monthly_heatmap
    });
}
```

## Troubleshooting

### Common Issues

1. **Compilation Errors**
   ```bash
   # Ensure C++20 support
   g++ --version  # Should be 11+ for GCC
   clang++ --version  # Should be 13+ for Clang
   ```

2. **Missing Dependencies**
   ```bash
   # Dependencies are auto-fetched, but check network access
   cmake --build . --target all  # Re-run if network issues
   ```

3. **Performance Issues**
   ```cpp
   // Enable compiler optimizations
   cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-march=native" ..
   
   // Check SIMD availability
   #ifdef PYFOLIO_HAS_AVX2
       std::cout << "AVX2 available" << std::endl;
   #endif
   ```

4. **Memory Issues**
   ```cpp
   // Use memory profiling
   cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZER=ON ..
   
   // Or use memory pools
   pyfolio::PoolAllocator<Position> allocator(100000);
   ```

### Debug Mode

```bash
# Build with debug information
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON ..
make

# Run with detailed error messages
export PYFOLIO_DEBUG=1
./your_program
```

### Performance Profiling

```bash
# Build with profiling
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ..

# Profile with perf (Linux)
perf record -g ./your_program
perf report

# Or use built-in benchmarks
./tests/performance_benchmarks
```

## Next Steps

1. **Explore Examples**: Check the `examples/` directory for more complex use cases
2. **Read API Documentation**: Browse the generated Doxygen docs at `build/docs/api/html/index.html`
3. **Performance Tuning**: See the optimization guide in `docs/PERFORMANCE.md`
4. **Contributing**: Read `CONTRIBUTING.md` for development guidelines

## Getting Help

- **Documentation**: Generated API docs in `build/docs/api/html/`
- **Examples**: Complete examples in `examples/` directory
- **Issues**: Report bugs on GitHub Issues
- **Discussions**: Join the GitHub Discussions for questions

Welcome to high-performance financial analysis with Pyfolio C++!