#pragma once

#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../core/types.h"
#include "../performance/drawdown.h"
#include "../performance/returns.h"
#include "../performance/rolling_metrics.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace pyfolio {
namespace visualization {

/**
 * @brief Configuration for plot appearance and output
 */
struct PlotConfig {
    std::string title               = "";
    std::string xlabel              = "";
    std::string ylabel              = "";
    std::pair<int, int> figsize     = {12, 8};
    bool grid                       = true;
    bool legend                     = true;
    bool save_plot                  = false;
    std::string save_path           = "";
    int dpi                         = 150;
    std::vector<std::string> colors = {"#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd"};
    std::string format              = "png";  // png, svg, html
};

/**
 * @brief Data structure for plot data
 */
struct PlotData {
    std::vector<DateTime> timestamps;
    std::vector<double> values;
    std::string label;
    std::string color;
    std::string style = "line";  // line, scatter, bar
};

/**
 * @brief Simple plotting interface that can generate multiple output formats
 *
 * This class provides plotting functionality without requiring matplotlib.
 * It can generate:
 * - HTML plots using embedded JavaScript (plotly.js/d3.js)
 * - SVG plots
 * - CSV data for external plotting
 * - Simple ASCII plots for console output
 */
class PlotEngine {
  public:
    PlotEngine()  = default;
    ~PlotEngine() = default;

    /**
     * @brief Create a line plot
     */
    Result<std::string> create_line_plot(const std::vector<PlotData>& series, const PlotConfig& config = PlotConfig{});

    /**
     * @brief Create a bar plot
     */
    Result<std::string> create_bar_plot(const std::vector<std::string>& labels, const std::vector<double>& values,
                                        const PlotConfig& config = PlotConfig{});

    /**
     * @brief Create a heatmap
     */
    Result<std::string> create_heatmap(const std::vector<std::string>& row_labels,
                                       const std::vector<std::string>& col_labels,
                                       const std::vector<std::vector<double>>& data,
                                       const PlotConfig& config = PlotConfig{});

    /**
     * @brief Create a histogram
     */
    Result<std::string> create_histogram(const std::vector<double>& data, int bins = 50,
                                         const PlotConfig& config = PlotConfig{});

  private:
    /**
     * @brief Generate HTML plot using plotly.js
     */
    std::string generate_html_plot(const std::vector<PlotData>& series, const PlotConfig& config,
                                   const std::string& plot_type = "line");

    /**
     * @brief Generate SVG plot
     */
    std::string generate_svg_plot(const std::vector<PlotData>& series, const PlotConfig& config);

    /**
     * @brief Save plot to file
     */
    Result<void> save_plot(const std::string& content, const std::string& path);

  public:
    /**
     * @brief Convert DateTime to string for plotting
     */
    std::string datetime_to_string(const DateTime& dt);

    // generate_svg_plot is already declared earlier
};

/**
 * @brief High-level plotting functions for financial data visualization
 */
namespace plots {

/**
 * @brief Plot cumulative returns
 */
Result<std::string> plot_cumulative_returns(const TimeSeries<Return>& returns,
                                            const std::optional<TimeSeries<Return>>& benchmark = std::nullopt,
                                            const PlotConfig& config                           = PlotConfig{});

/**
 * @brief Plot drawdown chart
 */
Result<std::string> plot_drawdown(const TimeSeries<Return>& returns, const PlotConfig& config = PlotConfig{});

/**
 * @brief Plot rolling volatility
 */
Result<std::string> plot_rolling_volatility(const TimeSeries<Return>& returns, size_t window = 60,
                                            const PlotConfig& config = PlotConfig{});

/**
 * @brief Plot rolling Sharpe ratio
 */
Result<std::string> plot_rolling_sharpe(const TimeSeries<Return>& returns, size_t window = 60,
                                        double risk_free_rate = 0.0, const PlotConfig& config = PlotConfig{});

/**
 * @brief Plot returns distribution histogram
 */
Result<std::string> plot_returns_distribution(const TimeSeries<Return>& returns, int bins = 50,
                                              const PlotConfig& config = PlotConfig{});

/**
 * @brief Plot monthly returns heatmap
 */
Result<std::string> plot_monthly_returns_heatmap(const TimeSeries<Return>& returns,
                                                 const PlotConfig& config = PlotConfig{});

/**
 * @brief Plot annual returns bar chart
 */
Result<std::string> plot_annual_returns(const TimeSeries<Return>& returns,
                                        const std::optional<TimeSeries<Return>>& benchmark = std::nullopt,
                                        const PlotConfig& config                           = PlotConfig{});

/**
 * @brief Plot correlation matrix heatmap
 */
Result<std::string> plot_correlation_matrix(const std::vector<TimeSeries<Return>>& return_series,
                                            const std::vector<std::string>& labels,
                                            const PlotConfig& config = PlotConfig{});

/**
 * @brief Create a comprehensive performance dashboard
 */
Result<std::string> create_performance_dashboard(const TimeSeries<Return>& returns,
                                                 const std::optional<TimeSeries<Return>>& benchmark = std::nullopt,
                                                 const std::string& output_path = "performance_dashboard.html");

}  // namespace plots

/**
 * @brief Utility functions for data preparation
 */
namespace utils {

/**
 * @brief Convert TimeSeries to PlotData
 */
PlotData timeseries_to_plotdata(const TimeSeries<Return>& ts, const std::string& label = "",
                                const std::string& color = "#1f77b4");

/**
 * @brief Calculate cumulative returns for plotting
 */
std::vector<double> calculate_cumulative_returns(const TimeSeries<Return>& returns);

/**
 * @brief Calculate monthly returns matrix for heatmap
 */
std::pair<std::vector<std::string>, std::vector<std::vector<double>>> calculate_monthly_returns_matrix(
    const TimeSeries<Return>& returns);

/**
 * @brief Calculate annual returns
 */
std::pair<std::vector<std::string>, std::vector<double>> calculate_annual_returns(const TimeSeries<Return>& returns);

/**
 * @brief Calculate correlation matrix
 */
std::vector<std::vector<double>> calculate_correlation_matrix(const std::vector<TimeSeries<Return>>& return_series);

}  // namespace utils

// Implementation of key functions

inline PlotData utils::timeseries_to_plotdata(const TimeSeries<Return>& ts, const std::string& label,
                                              const std::string& color) {
    PlotData data;
    data.timestamps = ts.timestamps();
    data.values     = ts.values();
    data.label      = label;
    data.color      = color;
    return data;
}

inline std::vector<double> utils::calculate_cumulative_returns(const TimeSeries<Return>& returns) {
    std::vector<double> cum_returns;
    cum_returns.reserve(returns.size());

    double cum_prod = 1.0;
    for (size_t i = 0; i < returns.size(); ++i) {
        cum_prod *= (1.0 + returns[i]);
        cum_returns.push_back(cum_prod - 1.0);  // Convert to percentage
    }

    return cum_returns;
}

inline std::pair<std::vector<std::string>, std::vector<double>> utils::calculate_annual_returns(
    const TimeSeries<Return>& returns) {
    std::unordered_map<int, std::vector<double>> yearly_returns;

    const auto& timestamps = returns.timestamps();
    const auto& values     = returns.values();

    for (size_t i = 0; i < timestamps.size(); ++i) {
        int year = timestamps[i].year();
        yearly_returns[year].push_back(values[i]);
    }

    std::vector<std::string> years;
    std::vector<double> annual_rets;

    for (const auto& [year, rets] : yearly_returns) {
        years.push_back(std::to_string(year));

        // Calculate annual return as compound growth
        double annual_ret = 1.0;
        for (double ret : rets) {
            annual_ret *= (1.0 + ret);
        }
        annual_rets.push_back(annual_ret - 1.0);
    }

    return {years, annual_rets};
}

inline std::string PlotEngine::generate_html_plot(const std::vector<PlotData>& series, const PlotConfig& config,
                                                  const std::string& plot_type) {
    std::stringstream html;

    html << R"(<!DOCTYPE html>
<html>
<head>
    <script src="https://cdn.plot.ly/plotly-latest.min.js"></script>
    <title>)"
         << config.title << R"(</title>
</head>
<body>
    <div id="plot" style="width:)"
         << config.figsize.first * 50 << R"(px;height:)" << config.figsize.second * 50 << R"(px;"></div>
    <script>
        var data = [)";

    for (size_t i = 0; i < series.size(); ++i) {
        if (i > 0)
            html << ",";

        html << "{\n";
        html << "  x: [";
        for (size_t j = 0; j < series[i].timestamps.size(); ++j) {
            if (j > 0)
                html << ",";
            html << "'" << datetime_to_string(series[i].timestamps[j]) << "'";
        }
        html << "],\n";

        html << "  y: [";
        for (size_t j = 0; j < series[i].values.size(); ++j) {
            if (j > 0)
                html << ",";
            html << series[i].values[j];
        }
        html << "],\n";

        html << "  type: '" << (plot_type == "line" ? "scatter" : plot_type) << "',\n";
        if (plot_type == "line") {
            html << "  mode: 'lines',\n";
        }
        html << "  name: '" << series[i].label << "',\n";
        html << "  line: { color: '" << series[i].color << "' }\n";
        html << "}";
    }

    html << R"(];
        
        var layout = {
            title: ')"
         << config.title << R"(',
            xaxis: { title: ')"
         << config.xlabel << R"(' },
            yaxis: { title: ')"
         << config.ylabel << R"(' },
            showlegend: )"
         << (config.legend ? "true" : "false") << R"(,
            grid: )"
         << (config.grid ? "true" : "false") << R"(
        };
        
        Plotly.newPlot('plot', data, layout);
    </script>
</body>
</html>)";

    return html.str();
}

// datetime_to_string implementation moved to later in file

inline Result<std::string> PlotEngine::create_line_plot(const std::vector<PlotData>& series, const PlotConfig& config) {
    if (series.empty()) {
        return Result<std::string>::error(ErrorCode::InvalidInput, "No data provided for plotting");
    }

    std::string content;

    if (config.format == "html") {
        content = generate_html_plot(series, config, "line");
    } else if (config.format == "svg") {
        content = generate_svg_plot(series, config);
    } else {
        return Result<std::string>::error(ErrorCode::InvalidInput, "Unsupported plot format");
    }

    if (config.save_plot && !config.save_path.empty()) {
        auto save_result = save_plot(content, config.save_path);
        if (save_result.is_error()) {
            return Result<std::string>(save_result.error());
        }
    }

    return Result<std::string>::success(content);
}

inline Result<void> PlotEngine::save_plot(const std::string& content, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return Result<void>::error(ErrorCode::FileNotFound, "Cannot open file for writing: " + path);
    }

    file << content;
    file.close();

    return Result<void>::success();
}

inline Result<std::string> plots::plot_cumulative_returns(const TimeSeries<Return>& returns,
                                                          const std::optional<TimeSeries<Return>>& benchmark,
                                                          const PlotConfig& config) {
    PlotEngine engine;
    std::vector<PlotData> series;

    // Add strategy returns
    auto plot_data   = utils::timeseries_to_plotdata(returns, "Strategy", "#1f77b4");
    plot_data.values = utils::calculate_cumulative_returns(returns);
    series.push_back(plot_data);

    // Add benchmark if provided
    if (benchmark.has_value()) {
        auto bench_data   = utils::timeseries_to_plotdata(benchmark.value(), "Benchmark", "#ff7f0e");
        bench_data.values = utils::calculate_cumulative_returns(benchmark.value());
        series.push_back(bench_data);
    }

    PlotConfig plot_config = config;
    if (plot_config.title.empty()) {
        plot_config.title = "Cumulative Returns";
    }
    if (plot_config.ylabel.empty()) {
        plot_config.ylabel = "Cumulative Return";
    }
    if (plot_config.xlabel.empty()) {
        plot_config.xlabel = "Date";
    }

    return engine.create_line_plot(series, plot_config);
}

inline Result<std::string> plots::plot_drawdown(const TimeSeries<Return>& returns, const PlotConfig& config) {
    PlotEngine engine;

    // Calculate drawdown
    auto drawdown_result = performance::calculate_drawdowns(returns);
    if (drawdown_result.is_error()) {
        return Result<std::string>(drawdown_result.error());
    }

    auto drawdown_series = drawdown_result.value();

    std::vector<PlotData> series;
    auto plot_data = utils::timeseries_to_plotdata(returns, "Drawdown", "#d62728");
    plot_data.values.clear();
    plot_data.timestamps = drawdown_series.timestamps();

    // Convert drawdown to negative percentages for plotting
    for (size_t i = 0; i < drawdown_series.size(); ++i) {
        plot_data.values.push_back(-drawdown_series[i] * 100.0);  // Negative percentage
    }

    series.push_back(plot_data);

    PlotConfig plot_config = config;
    if (plot_config.title.empty()) {
        plot_config.title = "Drawdown";
    }
    if (plot_config.ylabel.empty()) {
        plot_config.ylabel = "Drawdown (%)";
    }
    if (plot_config.xlabel.empty()) {
        plot_config.xlabel = "Date";
    }

    return engine.create_line_plot(series, plot_config);
}

inline Result<std::string> plots::create_performance_dashboard(const TimeSeries<Return>& returns,
                                                               const std::optional<TimeSeries<Return>>& benchmark,
                                                               const std::string& output_path) {
    std::stringstream dashboard;

    dashboard << R"(<!DOCTYPE html>
<html>
<head>
    <title>Performance Dashboard</title>
    <script src="https://cdn.plot.ly/plotly-latest.min.js"></script>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .plot-container { margin: 20px 0; }
        .metrics-table { border-collapse: collapse; width: 100%; margin: 20px 0; }
        .metrics-table th, .metrics-table td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        .metrics-table th { background-color: #f2f2f2; }
    </style>
</head>
<body>
    <h1>Portfolio Performance Dashboard</h1>
    
    <h2>Summary Statistics</h2>
    <table class="metrics-table">
        <tr><th>Metric</th><th>Value</th></tr>)";

    // Calculate and add basic metrics
    if (returns.size() > 0) {
        auto total_return  = utils::calculate_cumulative_returns(returns).back();
        auto annual_return = std::pow(1.0 + total_return, 252.0 / returns.size()) - 1.0;

        dashboard << "<tr><td>Total Return</td><td>" << (total_return * 100) << "%</td></tr>";
        dashboard << "<tr><td>Annualized Return</td><td>" << (annual_return * 100) << "%</td></tr>";
    }

    dashboard << R"(    </table>
    
    <div class="plot-container">
        <h2>Cumulative Returns</h2>
        <div id="cumulative-plot" style="width:100%;height:400px;"></div>
    </div>
    
    <div class="plot-container">
        <h2>Drawdown</h2>
        <div id="drawdown-plot" style="width:100%;height:400px;"></div>
    </div>
    
    <script>
        // Add plotting code here
        var cumData = [{
            x: [)";

    // Add cumulative returns data
    const auto& timestamps = returns.timestamps();
    auto cum_returns       = utils::calculate_cumulative_returns(returns);

    for (size_t i = 0; i < timestamps.size(); ++i) {
        if (i > 0)
            dashboard << ",";
        dashboard << "'" << PlotEngine().datetime_to_string(timestamps[i]) << "'";
    }

    dashboard << "],\n            y: [";

    for (size_t i = 0; i < cum_returns.size(); ++i) {
        if (i > 0)
            dashboard << ",";
        dashboard << cum_returns[i] * 100;  // Convert to percentage
    }

    dashboard << R"(],
            type: 'scatter',
            mode: 'lines',
            name: 'Strategy',
            line: { color: '#1f77b4' }
        }];
        
        var cumLayout = {
            title: 'Cumulative Returns',
            xaxis: { title: 'Date' },
            yaxis: { title: 'Return (%)' }
        };
        
        Plotly.newPlot('cumulative-plot', cumData, cumLayout);
        
        // Add more plots as needed
    </script>
</body>
</html>)";

    // Save to file
    std::ofstream file(output_path);
    if (!file.is_open()) {
        return Result<std::string>::error(ErrorCode::FileNotFound, "Cannot create dashboard file");
    }

    std::string content = dashboard.str();
    file << content;
    file.close();

    return Result<std::string>::success(content);
}

// ======================================================================
// INLINE IMPLEMENTATIONS
// ======================================================================

inline std::string PlotEngine::datetime_to_string(const DateTime& dt) {
    std::ostringstream oss;
    oss << dt.year() << "-" << std::setfill('0') << std::setw(2) << dt.month() << "-" << std::setfill('0')
        << std::setw(2) << dt.day();
    return oss.str();
}

inline std::string PlotEngine::generate_svg_plot(const std::vector<PlotData>& series, const PlotConfig& config) {
    // Simple SVG generation for basic line plots
    std::ostringstream svg;

    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<svg width=\"" << config.figsize.first * 80 << "\" height=\"" << config.figsize.second * 80 << "\" "
        << "xmlns=\"http://www.w3.org/2000/svg\">\n"
        << "  <rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n"
        << "  <g transform=\"translate(60,20)\">\n"
        << "    <text x=\"" << config.figsize.first * 40 << "\" y=\"20\" text-anchor=\"middle\" "
        << "font-family=\"Arial\" font-size=\"16\" font-weight=\"bold\">" << config.title << "</text>\n"
        << "    \n"
        << "    <!-- Plot area -->\n"
        << "    <rect x=\"0\" y=\"40\" width=\"" << config.figsize.first * 70 << "\" height=\""
        << config.figsize.second * 60 << "\" "
        << "fill=\"none\" stroke=\"black\" stroke-width=\"1\"/>\n"
        << "    \n"
        << "    <!-- Simple line plot -->\n";

    if (!series.empty()) {
        const auto& first_series = series[0];
        if (!first_series.values.empty()) {
            svg << "\n    <polyline fill=\"none\" stroke=\"" << (series[0].color.empty() ? "#1f77b4" : series[0].color)
                << "\" stroke-width=\"2\" points=\"";

            // Scale data to fit plot area
            double min_val = *std::min_element(first_series.values.begin(), first_series.values.end());
            double max_val = *std::max_element(first_series.values.begin(), first_series.values.end());
            double range   = max_val - min_val;
            if (range == 0)
                range = 1.0;

            double plot_width  = config.figsize.first * 70.0;
            double plot_height = config.figsize.second * 60.0;

            for (size_t i = 0; i < first_series.values.size(); ++i) {
                double x = (static_cast<double>(i) / (first_series.values.size() - 1)) * plot_width;
                double y = plot_height - ((first_series.values[i] - min_val) / range) * plot_height + 40;

                if (i > 0)
                    svg << " ";
                svg << x << "," << y;
            }
            svg << R"("/>)";
        }
    }

    svg << R"(
  </g>
</svg>)";

    return svg.str();
}

// Plot function implementations

inline Result<std::string> plots::plot_annual_returns(const TimeSeries<Return>& returns,
                                                      const std::optional<TimeSeries<Return>>& benchmark,
                                                      const PlotConfig& config) {
    // Simple implementation - group returns by year and calculate annual returns
    std::unordered_map<int, std::vector<Return>> yearly_returns;

    auto timestamps = returns.timestamps();
    auto values     = returns.values();

    for (size_t i = 0; i < timestamps.size(); ++i) {
        yearly_returns[timestamps[i].year()].push_back(values[i]);
    }

    std::vector<PlotData> series;
    PlotData data;
    data.label = "Annual Returns";
    data.color = "#1f77b4";

    for (const auto& [year, year_returns] : yearly_returns) {
        double annual_return = 1.0;
        for (double ret : year_returns) {
            annual_return *= (1.0 + ret);
        }
        annual_return -= 1.0;

        data.timestamps.push_back(DateTime(year, 12, 31));
        data.values.push_back(annual_return);
    }

    series.push_back(data);

    PlotEngine engine;
    return engine.create_line_plot(series, config);
}

inline Result<std::string> plots::plot_rolling_sharpe(const TimeSeries<Return>& returns, size_t window,
                                                      double risk_free_rate, const PlotConfig& config) {
    // Calculate rolling Sharpe ratio
    auto rolling_sharpe = performance::calculate_rolling_sharpe(returns, window, risk_free_rate);

    auto data = utils::timeseries_to_plotdata(rolling_sharpe, "Rolling Sharpe", "#2ca02c");

    PlotEngine engine;
    return engine.create_line_plot({data}, config);
}

inline Result<std::string> plots::plot_rolling_volatility(const TimeSeries<Return>& returns, size_t window,
                                                          const PlotConfig& config) {
    // Calculate rolling volatility
    auto rolling_vol = performance::calculate_rolling_volatility(returns, window);

    auto data = utils::timeseries_to_plotdata(rolling_vol, "Rolling Volatility", "#ff7f0e");

    PlotEngine engine;
    return engine.create_line_plot({data}, config);
}

inline Result<std::string> plots::plot_returns_distribution(const TimeSeries<Return>& returns, int bins,
                                                            const PlotConfig& config) {
    // Simple histogram implementation
    auto values = returns.values();
    if (values.empty()) {
        return Result<std::string>::error(ErrorCode::InsufficientData, "No returns data");
    }

    double min_val   = *std::min_element(values.begin(), values.end());
    double max_val   = *std::max_element(values.begin(), values.end());
    double bin_width = (max_val - min_val) / bins;

    std::vector<int> histogram(bins, 0);
    for (double val : values) {
        int bin = static_cast<int>((val - min_val) / bin_width);
        if (bin >= bins)
            bin = bins - 1;
        if (bin >= 0)
            histogram[bin]++;
    }

    // Convert to plot data
    PlotData data;
    data.label = "Returns Distribution";
    data.color = "#d62728";
    data.style = "bar";

    for (int i = 0; i < bins; ++i) {
        data.values.push_back(static_cast<double>(histogram[i]));
        data.timestamps.push_back(DateTime(2023, 1, 1 + i));  // Dummy dates for x-axis
    }

    PlotEngine engine;
    return engine.create_line_plot({data}, config);
}

}  // namespace visualization
}  // namespace pyfolio