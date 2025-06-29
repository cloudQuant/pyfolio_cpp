#pragma once

/**
 * @file pyfolio.h
 * @brief Main header file for pyfolio_cpp - High-performance portfolio analysis library
 * @version 1.0.0
 * @date 2024
 *
 * Pyfolio_cpp is a modern C++20 reimplementation of the Python pyfolio library,
 * designed for high-performance portfolio performance analysis and risk assessment.
 *
 * Key features:
 * - 10-100x performance improvement over Python pyfolio
 * - Modern C++20 with concepts, ranges, and coroutines
 * - Header-only library for easy integration
 * - Comprehensive error handling with Result<T> monad
 * - SIMD optimization for mathematical operations
 * - Thread-safe parallel processing capabilities
 */

// Core infrastructure
#include "core/dataframe.h"
#include "core/datetime.h"
#include "core/error_handling.h"
#include "core/time_series.h"
#include "core/types.h"

// Mathematical foundations
#include "math/statistics.h"

// Performance analysis
#include "performance/drawdown.h"
#include "performance/ratios.h"
#include "performance/returns.h"
#include "performance/rolling_metrics.h"

// Risk analysis
#include "risk/factor_exposure.h"
#include "risk/var.h"

// Portfolio positions
#include "positions/allocation.h"
#include "positions/holdings.h"

// Transaction analysis
#include "transactions/round_trips.h"
#include "transactions/transaction.h"

// Utilities
#include "utils/validation.h"

// Reporting
#include "reporting/interesting_periods.h"
#include "reporting/tears.h"

// Data I/O
#include "io/data_loader.h"

// Visualization
#include "visualization/plotting.h"

namespace pyfolio {

/**
 * @brief Library version information
 */
constexpr int VERSION_MAJOR          = 1;
constexpr int VERSION_MINOR          = 0;
constexpr int VERSION_PATCH          = 0;
constexpr const char* VERSION_STRING = "1.0.0";

/**
 * @brief Initialize the pyfolio library
 * Sets up logging, allocators, and other global state
 */
void initialize();

/**
 * @brief Cleanup pyfolio library resources
 */
void finalize();

}  // namespace pyfolio