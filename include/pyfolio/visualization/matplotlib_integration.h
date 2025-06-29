#pragma once

// Matplotlib-cpp integration for pyfolio_cpp
// Requires: pip install matplotlib-cpp or equivalent C++ matplotlib binding

#include "../visualization/plotting.h"
#include <pyfolio/analytics/performance_metrics.h>
#include <pyfolio/analytics/statistics.h>
#include <pyfolio/core/error_handling.h>
#include <pyfolio/core/time_series.h>
#include <pyfolio/core/types.h>
#include <pyfolio/positions/positions.h>
#include <pyfolio/transactions/transaction.h>

// Conditional compilation based on matplotlib availability
#ifdef PYFOLIO_ENABLE_MATPLOTLIB
#include <matplotlibcpp.h>
namespace plt = matplotlibcpp;
#endif

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace pyfolio::visualization {

// PlotConfig is defined in visualization/plotting.h

struct TearSheetConfig {
    bool include_benchmark           = true;
    bool include_drawdown            = true;
    bool include_rolling_metrics     = true;
    bool include_monthly_heatmap     = true;
    bool include_return_distribution = true;
    bool include_top_holdings        = false;
    bool include_turnover            = false;
    std::pair<int, int> figsize      = {15, 20};
    std::string save_path            = "";
};

/**
 * Main plotting class that provides Python pyfolio equivalent visualization
 * Uses matplotlib-cpp for C++ integration with matplotlib
 */
class PyfolioPlotter {
  public:
    PyfolioPlotter();
    ~PyfolioPlotter();

    // ========================================================================
    // Core Time Series Plots (equivalent to plotting.py functions)
    // ========================================================================

    /**
     * Plot cumulative returns over time
     * Equivalent to Python pyfolio.plotting.plot_rolling_returns()
     */
    Result<void> plot_cumulative_returns(const TimeSeries<Return>& returns, const TimeSeries<Return>& benchmark = {},
                                         const PlotConfig& config = {});

    /**
     * Plot underwater drawdown chart
     * Equivalent to Python pyfolio.plotting.plot_drawdown_underwater()
     */
    Result<void> plot_drawdown_underwater(const TimeSeries<Return>& returns, const PlotConfig& config = {});

    /**
     * Plot monthly returns heatmap
     * Equivalent to Python pyfolio.plotting.plot_monthly_returns_heatmap()
     */
    Result<void> plot_monthly_returns_heatmap(const TimeSeries<Return>& returns, const PlotConfig& config = {});

    /**
     * Plot annual returns bar chart
     * Equivalent to Python pyfolio.plotting.plot_annual_returns()
     */
    Result<void> plot_annual_returns(const TimeSeries<Return>& returns, const TimeSeries<Return>& benchmark = {},
                                     const PlotConfig& config = {});

    // ========================================================================
    // Rolling Metrics Plots
    // ========================================================================

    /**
     * Plot rolling Sharpe ratio
     * Equivalent to Python pyfolio.plotting.plot_rolling_sharpe()
     */
    Result<void> plot_rolling_sharpe(const TimeSeries<Return>& returns, int window = 63, double risk_free_rate = 0.02,
                                     const PlotConfig& config = {});

    /**
     * Plot rolling volatility
     * Equivalent to Python pyfolio.plotting.plot_rolling_volatility()
     */
    Result<void> plot_rolling_volatility(const TimeSeries<Return>& returns, int window = 63,
                                         const PlotConfig& config = {});

    /**
     * Plot rolling beta vs benchmark
     * Equivalent to Python pyfolio.plotting.plot_rolling_beta()
     */
    Result<void> plot_rolling_beta(const TimeSeries<Return>& returns, const TimeSeries<Return>& benchmark,
                                   int window = 126, const PlotConfig& config = {});

    // ========================================================================
    // Distribution and Risk Plots
    // ========================================================================

    /**
     * Plot return distribution histogram
     * Equivalent to Python pyfolio.plotting.plot_monthly_returns_dist()
     */
    Result<void> plot_return_distribution(const TimeSeries<Return>& returns, const PlotConfig& config = {});

    /**
     * Plot return quantiles box plot
     * Equivalent to Python pyfolio.plotting.plot_return_quantiles()
     */
    Result<void> plot_return_quantiles(const TimeSeries<Return>& returns, const PlotConfig& config = {});

    // ========================================================================
    // Position and Holdings Plots
    // ========================================================================

    /**
     * Plot long/short exposure over time
     * Equivalent to Python pyfolio.plotting.plot_exposures()
     */
    Result<void> plot_exposures(const PositionSeries& positions, const PlotConfig& config = {});

    /**
     * Plot number of holdings over time
     * Equivalent to Python pyfolio.plotting.plot_holdings()
     */
    Result<void> plot_holdings(const PositionSeries& positions, const PlotConfig& config = {});

    /**
     * Plot top holdings allocation over time
     * Equivalent to Python pyfolio.plotting.show_and_plot_top_positions()
     */
    Result<void> plot_top_holdings(const PositionSeries& positions, int top_n = 10, const PlotConfig& config = {});

    // ========================================================================
    // Transaction and Turnover Plots
    // ========================================================================

    /**
     * Plot portfolio turnover over time
     * Equivalent to Python pyfolio.plotting.plot_turnover()
     */
    Result<void> plot_turnover(const PositionSeries& positions, const TransactionSeries& transactions = {},
                               const PlotConfig& config = {});

    /**
     * Plot round trip lifetimes
     * Equivalent to Python pyfolio.plotting.plot_round_trip_lifetimes()
     */
    Result<void> plot_round_trip_lifetimes(const TransactionSeries& transactions, const PlotConfig& config = {});

    // ========================================================================
    // Complete Tear Sheets
    // ========================================================================

    /**
     * Create full tear sheet with all major plots
     * Equivalent to Python pyfolio.tears.create_full_tear_sheet()
     */
    Result<void> create_full_tear_sheet(const TimeSeries<Return>& returns, const TimeSeries<Return>& benchmark = {},
                                        const PositionSeries& positions       = {},
                                        const TransactionSeries& transactions = {}, const TearSheetConfig& config = {});

    /**
     * Create simple tear sheet with key plots only
     * Equivalent to Python pyfolio.tears.create_simple_tear_sheet()
     */
    Result<void> create_simple_tear_sheet(const TimeSeries<Return>& returns, const TimeSeries<Return>& benchmark = {},
                                          const TearSheetConfig& config = {});

    /**
     * Create returns-focused tear sheet
     * Equivalent to Python pyfolio.tears.create_returns_tear_sheet()
     */
    Result<void> create_returns_tear_sheet(const TimeSeries<Return>& returns, const TimeSeries<Return>& benchmark = {},
                                           const TearSheetConfig& config = {});

  private:
    // ========================================================================
    // Internal Helper Methods
    // ========================================================================

    /**
     * Initialize matplotlib settings and style
     */
    void initialize_matplotlib();

    /**
     * Convert DateTime vector to numeric vector for plotting
     */
    std::vector<double> datetime_to_numeric(const std::vector<DateTime>& dates);

    /**
     * Setup plot with common styling
     */
    void setup_plot(const PlotConfig& config);

    /**
     * Save plot if requested
     */
    Result<void> save_plot_if_requested(const PlotConfig& config);

    /**
     * Calculate monthly returns for heatmap
     */
    std::vector<std::vector<double>> calculate_monthly_returns_matrix(const TimeSeries<Return>& returns);

    /**
     * Calculate drawdown series for underwater plot
     */
    std::vector<double> calculate_drawdown_series(const TimeSeries<Return>& returns);

    /**
     * Calculate long/short exposures from positions
     */
    std::pair<std::vector<double>, std::vector<double>> calculate_exposures(const PositionSeries& positions);

    /**
     * Get month and year labels for heatmap
     */
    std::pair<std::vector<std::string>, std::vector<int>> get_monthly_labels(const std::vector<DateTime>& dates);

    /**
     * Create subplot layout for tear sheets
     */
    void create_subplot_layout(int rows, int cols);

    /**
     * Add performance statistics table to plot
     */
    void add_performance_table(const TimeSeries<Return>& returns, const TimeSeries<Return>& benchmark,
                               const std::pair<double, double>& position = {0.02, 0.7});

    /**
     * Format axis labels and ticks
     */
    void format_axes(const PlotConfig& config);

    /**
     * Add recession shading (if recession periods are available)
     */
    void add_recession_shading(const std::vector<DateTime>& dates);

    /**
     * Validate that matplotlib is available
     */
    bool is_matplotlib_available() const;

    // ========================================================================
    // Member Variables
    // ========================================================================

    bool matplotlib_initialized_;
    PlotConfig default_config_;
    std::map<std::string, std::string> style_settings_;
};

/**
 * Utility functions for visualization
 */
namespace plot_utils {

/**
 * Convert returns to cumulative returns for plotting
 */
std::vector<double> returns_to_cumulative(const std::vector<Return>& returns);

/**
 * Create color palette for multiple series
 */
std::vector<std::string> create_color_palette(int n_colors);

/**
 * Format percentage for display
 */
std::string format_percentage(double value, int decimals = 1);

/**
 * Create date labels for x-axis
 */
std::vector<std::string> create_date_labels(const std::vector<DateTime>& dates, const std::string& format = "%Y-%m");

/**
 * Calculate optimal bin count for histograms
 */
int calculate_optimal_bins(const std::vector<double>& data);

/**
 * Smooth data for better visualization
 */
std::vector<double> smooth_data(const std::vector<double>& data, int window = 5);

}  // namespace plot_utils

}  // namespace pyfolio::visualization