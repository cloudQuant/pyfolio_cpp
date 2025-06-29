#pragma once

/**
 * @file positions.h
 * @brief Portfolio positions types and utilities
 * @version 1.0.0
 * @date 2024
 *
 * This file provides a unified interface for all position-related functionality,
 * combining holdings, allocation, and position analysis.
 */

#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../core/types.h"
#include "allocation.h"
#include "holdings.h"

namespace pyfolio::positions {

/**
 * @brief Position analysis and manipulation utilities
 */
class PositionAnalyzer {
  public:
    /**
     * @brief Calculate total portfolio value at a given time
     */
    static Result<double> calculate_total_value(const std::unordered_map<std::string, Position>& positions);

    /**
     * @brief Calculate long/short exposure
     */
    static Result<std::pair<double, double>> calculate_exposures(
        const std::unordered_map<std::string, Position>& positions);

    /**
     * @brief Calculate portfolio leverage
     */
    static Result<double> calculate_leverage(const std::unordered_map<std::string, Position>& positions);

    /**
     * @brief Get top N positions by absolute value
     */
    static Result<std::vector<std::pair<std::string, Position>>> get_top_positions(
        const std::unordered_map<std::string, Position>& positions, size_t n = 10);

    /**
     * @brief Count number of holdings (non-zero positions)
     */
    static size_t count_holdings(const std::unordered_map<std::string, Position>& positions);
};

/**
 * @brief Position series utilities
 */
namespace utils {

/**
 * @brief Convert position series to exposure series
 */
Result<TimeSeries<std::pair<double, double>>> positions_to_exposures(
    const TimeSeries<std::unordered_map<std::string, Position>>& position_series);

/**
 * @brief Convert position series to leverage series
 */
Result<TimeSeries<double>> positions_to_leverage(
    const TimeSeries<std::unordered_map<std::string, Position>>& position_series);

/**
 * @brief Convert position series to holding count series
 */
Result<TimeSeries<int>> positions_to_holding_counts(
    const TimeSeries<std::unordered_map<std::string, Position>>& position_series);

}  // namespace utils

// Type aliases for convenience
using PositionSeries = TimeSeries<std::unordered_map<std::string, Position>>;

}  // namespace pyfolio::positions