#pragma once

#include <map>
#include <pyfolio/core/error_handling.h>
#include <pyfolio/core/types.h>
#include <pyfolio/positions/positions.h>
#include <pyfolio/transactions/transaction.h>
#include <string>
#include <vector>

namespace pyfolio::analytics {

enum class TurnoverDenominator {
    AGB,  // Average Gross Book (default in Python pyfolio)
    PortfolioValue,
    NetLiquidation,
    TotalAssets
};

struct TurnoverResult {
    std::vector<double> daily_turnover;
    double average_turnover;
    double annualized_turnover;
    std::vector<double> rolling_agb;
    std::vector<double> position_changes;
    std::vector<DateTime> dates;
    TurnoverDenominator denominator_used;

    // Additional statistics
    double turnover_volatility;
    double max_daily_turnover;
    double min_daily_turnover;
    std::vector<double> monthly_turnover;
};

struct AdvancedTurnoverMetrics {
    TurnoverResult basic_turnover;

    // Symbol-level turnover analysis
    std::map<std::string, double> symbol_turnover_contribution;
    std::map<std::string, double> symbol_avg_turnover;

    // Turnover decomposition
    double buy_turnover;
    double sell_turnover;
    double rebalancing_turnover;
    double cash_flow_turnover;

    // Rolling metrics
    std::vector<double> rolling_30d_turnover;
    std::vector<double> rolling_90d_turnover;

    // Correlation with returns
    double turnover_return_correlation;
    double turnover_volatility_correlation;
};

class EnhancedTurnoverAnalyzer {
  public:
    /**
     * Calculate enhanced turnover with multiple denominator options
     * Replicates and extends Python pyfolio.txn.get_turnover()
     *
     * @param positions Position series over time
     * @param transactions Transaction series (optional for validation)
     * @param denominator Method for calculating turnover denominator
     * @return Comprehensive turnover analysis result
     */
    Result<TurnoverResult> calculate_enhanced_turnover(const PositionSeries& positions,
                                                       const TransactionSeries& transactions = {},
                                                       TurnoverDenominator denominator = TurnoverDenominator::AGB);

    /**
     * Calculate comprehensive turnover metrics including decomposition
     *
     * @param positions Position series
     * @param transactions Transaction series
     * @param returns Return series for correlation analysis
     * @return Advanced turnover metrics
     */
    Result<AdvancedTurnoverMetrics> calculate_comprehensive_turnover_metrics(const PositionSeries& positions,
                                                                             const TransactionSeries& transactions,
                                                                             const TimeSeries<Return>& returns = {});

    /**
     * Calculate Average Gross Book (AGB) using rolling window
     * Core calculation for Python pyfolio compatibility
     *
     * @param positions Position series
     * @param window Rolling window size (default 2 as in Python)
     * @return Rolling AGB values
     */
    Result<std::vector<double>> calculate_rolling_agb(const PositionSeries& positions, int window = 2);

    /**
     * Calculate net position changes between periods
     *
     * @param positions Position series
     * @return Daily position changes
     */
    Result<std::vector<double>> calculate_net_position_changes(const PositionSeries& positions);

    /**
     * Decompose turnover into different components
     *
     * @param positions Position series
     * @param transactions Transaction series
     * @return Turnover decomposition by type
     */
    Result<std::map<std::string, double>> decompose_turnover(const PositionSeries& positions,
                                                             const TransactionSeries& transactions);

  private:
    /**
     * Calculate AGB for a single position snapshot
     */
    double calculate_agb_for_date(const Position& snapshot);

    /**
     * Calculate rolling mean with proper handling of edge cases
     */
    std::vector<double> rolling_mean(const std::vector<double>& values, int window);

    /**
     * Calculate turnover using different denominators
     */
    Result<std::vector<double>> calculate_turnover_with_denominator(const std::vector<double>& position_changes,
                                                                    const std::vector<double>& denominator_values);

    /**
     * Extract symbol-level position changes
     */
    std::map<std::string, std::vector<double>> calculate_symbol_position_changes(const PositionSeries& positions);

    /**
     * Calculate monthly aggregated turnover
     */
    std::vector<double> aggregate_monthly_turnover(const std::vector<double>& daily_turnover,
                                                   const std::vector<DateTime>& dates);

    /**
     * Calculate correlation between turnover and other metrics
     */
    double calculate_correlation(const std::vector<double>& series1, const std::vector<double>& series2);

    /**
     * Validate inputs for turnover calculation
     */
    bool validate_turnover_inputs(const PositionSeries& positions, const TransactionSeries& transactions);

    /**
     * Handle edge cases in turnover calculation
     */
    std::vector<double> handle_turnover_edge_cases(const std::vector<double>& raw_turnover);
};

/**
 * Utility functions for turnover analysis
 */
namespace turnover_utils {

/**
 * Convert daily turnover to annualized
 */
double annualize_turnover(double daily_turnover, int trading_days_per_year = 252);

/**
 * Calculate turnover statistics
 */
struct TurnoverStatistics {
    double mean;
    double median;
    double std_dev;
    double skewness;
    double kurtosis;
    double percentile_95;
    double percentile_99;
};

TurnoverStatistics calculate_turnover_statistics(const std::vector<double>& turnover);

/**
 * Detect high turnover periods
 */
std::vector<std::pair<DateTime, DateTime>> detect_high_turnover_periods(const std::vector<double>& turnover,
                                                                        const std::vector<DateTime>& dates,
                                                                        double threshold_percentile = 0.95);

/**
 * Calculate turnover efficiency metrics
 */
struct TurnoverEfficiency {
    double information_ratio_per_turnover;
    double sharpe_ratio_per_turnover;
    double return_per_turnover;
    double alpha_per_turnover;
};

Result<TurnoverEfficiency> calculate_turnover_efficiency(const TurnoverResult& turnover,
                                                         const TimeSeries<Return>& returns,
                                                         const TimeSeries<Return>& benchmark = {});

}  // namespace turnover_utils

}  // namespace pyfolio::analytics