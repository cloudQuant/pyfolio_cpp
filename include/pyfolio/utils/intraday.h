#pragma once

#include <algorithm>
#include <map>
#include <numeric>
#include <pyfolio/core/datetime.h>
#include <pyfolio/core/error_handling.h>
#include <pyfolio/core/time_series.h>
#include <pyfolio/core/types.h>
#include <pyfolio/positions/positions.h>
#include <pyfolio/transactions/transaction.h>
#include <string>
#include <vector>

namespace pyfolio::intraday {

struct IntradayDetectionResult {
    bool is_intraday;
    double threshold_ratio;
    DateTime optimal_sampling_time;
    std::string detection_method;
    double confidence_score;
    std::map<std::string, double> symbol_ratios;
};

struct IntradayEstimationConfig {
    int eod_hour              = 23;    // End of day hour
    int eod_minute            = 0;     // End of day minute
    double position_threshold = 0.25;  // Threshold for intraday detection
    bool use_rolling_max      = true;  // Use rolling maximum for exposure
    int rolling_window        = 5;     // Rolling window for exposure calculation
};

class IntradayAnalyzer {
  public:
    /**
     * Detects if a strategy is intraday by analyzing the ratio of positions to transactions
     * Equivalent to Python pyfolio.utils.detect_intraday()
     *
     * @param positions Portfolio positions over time
     * @param transactions Transaction history
     * @param threshold Detection threshold (default 0.25 from Python)
     * @return Detection result with intraday classification
     */
    Result<IntradayDetectionResult> detect_intraday(const PositionSeries& positions,
                                                    const TransactionSeries& transactions, double threshold = 0.25);

    /**
     * Estimates intraday positions by finding optimal sampling times
     * Equivalent to Python pyfolio.utils.estimate_intraday()
     *
     * @param returns Return series
     * @param positions Position series
     * @param transactions Transaction series
     * @param config Configuration for estimation
     * @return Estimated intraday positions
     */
    Result<PositionSeries> estimate_intraday_positions(const TimeSeries<Return>& returns,
                                                       const PositionSeries& positions,
                                                       const TransactionSeries& transactions,
                                                       const IntradayEstimationConfig& config = {});

    /**
     * Comprehensive intraday check and processing logic
     * Equivalent to Python pyfolio.utils.check_intraday()
     *
     * @param estimate Estimation result from detection
     * @param returns Return series
     * @param positions Position series
     * @param transactions Transaction series
     * @return Processed position series appropriate for strategy type
     */
    Result<PositionSeries> check_and_process_intraday(const IntradayDetectionResult& detection,
                                                      const TimeSeries<Return>& returns,
                                                      const PositionSeries& positions,
                                                      const TransactionSeries& transactions);

  private:
    /**
     * Finds the time of peak exposure based on transaction volume
     * Used to determine optimal sampling time for intraday strategies
     */
    DateTime find_peak_exposure_time(const std::vector<transactions::TransactionRecord>& daily_transactions);

    /**
     * Calculates the ratio of position values to transaction volumes
     * Core logic for intraday detection
     */
    double calculate_position_transaction_ratio(const PositionSeries& positions, const TransactionSeries& transactions);

    /**
     * Calculates position-to-transaction ratio for a specific symbol
     */
    double calculate_symbol_ratio(const PositionSeries& positions, const TransactionSeries& transactions,
                                  const std::string& symbol);

    /**
     * Extracts transactions for a specific date
     */
    std::vector<transactions::TransactionRecord> get_transactions_for_date(const TransactionSeries& transactions,
                                                                           const DateTime& date);

    /**
     * Calculates rolling maximum exposure for better intraday estimation
     */
    std::vector<double> calculate_rolling_max_exposure(const std::vector<double>& exposures, int window);

    /**
     * Groups transactions by time of day to find peak trading times
     */
    std::map<int, double> group_transactions_by_hour(const std::vector<transactions::TransactionRecord>& transactions);

    /**
     * Estimates end-of-day positions based on transaction flow
     */
    Result<Position> estimate_eod_positions(const DateTime& date,
                                            const std::vector<transactions::TransactionRecord>& daily_transactions,
                                            const Position& previous_positions);

    /**
     * Validates intraday detection results for consistency
     */
    bool validate_detection_results(const IntradayDetectionResult& result, const PositionSeries& positions,
                                    const TransactionSeries& transactions);

    /**
     * Calculates confidence score for intraday detection
     * Based on consistency across multiple symbols and dates
     */
    double calculate_confidence_score(const std::map<std::string, double>& symbol_ratios, double overall_ratio,
                                      double threshold);
};

/**
 * Utility functions for intraday analysis
 */
namespace utils {

/**
 * Checks if a given time is within trading hours
 */
bool is_trading_hours(const DateTime& dt, int start_hour = 9, int end_hour = 16);

/**
 * Normalizes position values for comparison across different scales
 */
std::vector<double> normalize_positions(const std::vector<double>& positions);

/**
 * Calculates the average absolute deviation for variability measurement
 */
double calculate_average_absolute_deviation(const std::vector<double>& values);

/**
 * Interpolates missing positions based on transaction flow
 */
Result<std::vector<double>> interpolate_missing_positions(
    const std::vector<DateTime>& dates, const std::vector<double>& known_positions,
    const std::vector<transactions::TransactionRecord>& transactions);

}  // namespace utils

}  // namespace pyfolio::intraday