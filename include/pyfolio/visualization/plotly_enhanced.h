#pragma once

/**
 * @file plotly_enhanced.h
 * @brief Enhanced Plotly visualization with interactive features
 * @version 1.0.0
 * @date 2024
 *
 * This file provides advanced Plotly.js integration with interactive dashboards,
 * real-time updates, and professional financial chart components.
 */

#include "../analytics/performance_metrics.h"
#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../core/types.h"
#include "../positions/positions.h"
#include "plotting.h"
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace pyfolio::visualization {

using json = nlohmann::json;

/**
 * @brief Advanced configuration for interactive plots
 */
struct InteractivePlotConfig : public PlotConfig {
    bool enable_zoom          = true;
    bool enable_pan           = true;
    bool enable_hover         = true;
    bool enable_crossfilter   = false;
    bool enable_rangeslider   = true;
    bool enable_rangeselector = true;
    std::string theme         = "plotly_white";  // plotly, plotly_white, plotly_dark, ggplot2, seaborn
    bool responsive           = true;
    json custom_config;

    // Animation settings
    bool enable_animation  = false;
    int animation_duration = 500;

    // Layout customization
    json margin = {
        {"l", 60},
        {"r", 60},
        {"t", 80},
        {"b", 60}
    };
    std::string font_family = "Arial, sans-serif";
    int font_size           = 12;
};

/**
 * @brief Enhanced Plotly chart types
 */
enum class ChartType {
    Line,
    Candlestick,
    OHLC,
    Heatmap,
    Scatter,
    Bar,
    Histogram,
    Box,
    Violin,
    Surface,
    Waterfall,
    Treemap,
    Sunburst
};

/**
 * @brief Data structure for OHLC/Candlestick charts
 */
struct OHLCData {
    std::vector<DateTime> timestamps;
    std::vector<double> open;
    std::vector<double> high;
    std::vector<double> low;
    std::vector<double> close;
    std::vector<double> volume;
    std::string name;
};

/**
 * @brief Advanced Plotly visualization engine
 */
class PlotlyEngine {
  private:
    InteractivePlotConfig default_config_;

  public:
    PlotlyEngine() = default;
    explicit PlotlyEngine(const InteractivePlotConfig& config) : default_config_(config) {}

    /**
     * @brief Create interactive time series chart
     */
    Result<std::string> create_time_series_chart(const std::vector<TimeSeries<double>>& series,
                                                 const std::vector<std::string>& labels,
                                                 const InteractivePlotConfig& config = {});

    /**
     * @brief Create candlestick chart
     */
    Result<std::string> create_candlestick_chart(const OHLCData& ohlc_data, const InteractivePlotConfig& config = {});

    /**
     * @brief Create correlation heatmap
     */
    Result<std::string> create_correlation_heatmap(const std::vector<std::vector<double>>& correlation_matrix,
                                                   const std::vector<std::string>& labels,
                                                   const InteractivePlotConfig& config = {});

    /**
     * @brief Create 3D surface plot
     */
    Result<std::string> create_3d_surface(const std::vector<std::vector<double>>& z_data,
                                          const std::vector<double>& x_data, const std::vector<double>& y_data,
                                          const InteractivePlotConfig& config = {});

    /**
     * @brief Create treemap visualization
     */
    Result<std::string> create_treemap(const std::vector<std::string>& labels, const std::vector<std::string>& parents,
                                       const std::vector<double>& values, const InteractivePlotConfig& config = {});

    /**
     * @brief Create waterfall chart
     */
    Result<std::string> create_waterfall_chart(const std::vector<std::string>& labels,
                                               const std::vector<double>& values,
                                               const InteractivePlotConfig& config = {});

    /**
     * @brief Create subplots dashboard
     */
    Result<std::string> create_subplots_dashboard(const std::vector<json>& subplot_configs, int rows, int cols,
                                                  const InteractivePlotConfig& config = {});

  public:
    /**
     * @brief Generate Plotly.js JSON configuration
     */
    json generate_plotly_config(const InteractivePlotConfig& config);

    /**
     * @brief Generate layout configuration
     */
    json generate_layout(const InteractivePlotConfig& config, const std::string& title = "");

    /**
     * @brief Generate HTML wrapper with Plotly.js
     */
    std::string generate_html_wrapper(const json& data, const json& layout, const json& config,
                                      const std::string& div_id = "plotly-chart");

    /**
     * @brief Convert DateTime to ISO string for Plotly
     */
    std::string datetime_to_plotly_string(const DateTime& dt);
};

/**
 * @brief Interactive financial charts
 */
namespace interactive {

/**
 * @brief Create comprehensive portfolio dashboard
 */
Result<std::string> create_portfolio_dashboard(const TimeSeries<Return>& returns,
                                               const positions::PortfolioHoldings& holdings,
                                               const std::optional<TimeSeries<Return>>& benchmark = std::nullopt,
                                               const std::string& output_path = "portfolio_dashboard.html");

/**
 * @brief Create risk metrics dashboard
 */
Result<std::string> create_risk_dashboard(const TimeSeries<Return>& returns,
                                          const analytics::PerformanceMetrics& metrics,
                                          const std::string& output_path = "risk_dashboard.html");

/**
 * @brief Create real-time updating chart
 */
class RealTimeChart {
  private:
    std::string chart_id_;
    json current_data_;
    json layout_;

  public:
    explicit RealTimeChart(const std::string& chart_id) : chart_id_(chart_id) {}

    /**
     * @brief Initialize chart
     */
    Result<std::string> initialize(const TimeSeries<double>& initial_data, const InteractivePlotConfig& config = {});

    /**
     * @brief Add new data point
     */
    std::string add_data_point(const DateTime& timestamp, double value);

    /**
     * @brief Update multiple series
     */
    std::string update_series(const std::map<std::string, std::pair<DateTime, double>>& updates);

    /**
     * @brief Generate JavaScript update code
     */
    std::string generate_update_script();
};

/**
 * @brief Create advanced performance attribution chart
 */
Result<std::string> create_attribution_waterfall(const std::map<std::string, double>& attribution_factors,
                                                 const InteractivePlotConfig& config = {});

/**
 * @brief Create rolling metrics comparison chart
 */
Result<std::string> create_rolling_metrics_chart(const TimeSeries<Return>& returns,
                                                 const std::vector<size_t>& windows  = {30, 60, 252},
                                                 const InteractivePlotConfig& config = {});

/**
 * @brief Create sector allocation pie chart
 */
Result<std::string> create_sector_allocation_chart(const std::map<std::string, double>& sector_weights,
                                                   const InteractivePlotConfig& config = {});

/**
 * @brief Create drawdown underwater chart
 */
Result<std::string> create_underwater_chart(const TimeSeries<Return>& returns,
                                            const InteractivePlotConfig& config = {});

}  // namespace interactive

/**
 * @brief Web-based dashboard components
 */
namespace dashboard {

/**
 * @brief Multi-page dashboard generator
 */
class DashboardBuilder {
  private:
    std::map<std::string, std::string> pages_;
    std::string navigation_html_;
    InteractivePlotConfig global_config_;

  public:
    explicit DashboardBuilder(const InteractivePlotConfig& config = {}) : global_config_(config) {}

    /**
     * @brief Add page to dashboard
     */
    void add_page(const std::string& page_id, const std::string& title, const std::string& content);

    /**
     * @brief Add performance overview page
     */
    Result<void> add_performance_page(const TimeSeries<Return>& returns, const analytics::PerformanceMetrics& metrics);

    /**
     * @brief Add risk analysis page
     */
    Result<void> add_risk_page(const TimeSeries<Return>& returns,
                               const std::optional<TimeSeries<Return>>& benchmark = std::nullopt);

    /**
     * @brief Add portfolio composition page
     */
    Result<void> add_holdings_page(const positions::PortfolioHoldings& holdings);

    /**
     * @brief Generate complete dashboard HTML
     */
    Result<std::string> build(const std::string& title = "Portfolio Dashboard");

    /**
     * @brief Save dashboard to file
     */
    Result<void> save(const std::string& filename);

  private:
    std::string generate_navigation();
    std::string generate_css();
    std::string generate_javascript();
};

/**
 * @brief Live data integration
 */
class LiveDashboard {
  private:
    std::string websocket_endpoint_;
    int update_interval_ms_;

  public:
    LiveDashboard(const std::string& endpoint, int interval_ms = 1000)
        : websocket_endpoint_(endpoint), update_interval_ms_(interval_ms) {}

    /**
     * @brief Generate WebSocket client code
     */
    std::string generate_websocket_client();

    /**
     * @brief Create live updating portfolio dashboard
     */
    Result<std::string> create_live_dashboard(const std::string& initial_data_endpoint,
                                              const InteractivePlotConfig& config = {});
};

}  // namespace dashboard

// ============================================================================
// IMPLEMENTATION
// ============================================================================

inline json PlotlyEngine::generate_plotly_config(const InteractivePlotConfig& config) {
    json plotly_config = {
        {        "displayModeBar",              true},
        {           "displaylogo",             false},
        {"modeBarButtonsToRemove",     json::array()},
        {            "responsive", config.responsive}
    };

    if (!config.enable_zoom) {
        plotly_config["modeBarButtonsToRemove"].push_back("zoom2d");
        plotly_config["modeBarButtonsToRemove"].push_back("zoomIn2d");
        plotly_config["modeBarButtonsToRemove"].push_back("zoomOut2d");
    }

    if (!config.enable_pan) {
        plotly_config["modeBarButtonsToRemove"].push_back("pan2d");
    }

    // Merge custom config
    if (!config.custom_config.empty()) {
        plotly_config.update(config.custom_config);
    }

    return plotly_config;
}

inline json PlotlyEngine::generate_layout(const InteractivePlotConfig& config, const std::string& title) {
    json layout = {
        { "template",                                                         config.theme},
        {   "margin",                                                        config.margin},
        {     "font",         {{"family", config.font_family}, {"size", config.font_size}}},
        {"hovermode", config.enable_hover ? std::string("x unified") : std::string("none")}
    };

    if (!title.empty()) {
        layout["title"] = {
            {   "text",    title},
            {      "x",      0.5},
            {"xanchor", "center"}
        };
    }

    if (config.enable_rangeslider) {
        layout["xaxis"] = {
            {"rangeslider", {{"visible", true}}},
            {       "type",              "date"}
        };
    }

    if (config.enable_rangeselector) {
        layout["xaxis"]["rangeselector"] = {
            {"buttons", json::array({{{"count", 7}, {"label", "7D"}, {"step", "day"}, {"stepmode", "backward"}},
                                     {{"count", 30}, {"label", "1M"}, {"step", "day"}, {"stepmode", "backward"}},
                                     {{"count", 90}, {"label", "3M"}, {"step", "day"}, {"stepmode", "backward"}},
                                     {{"count", 1}, {"label", "1Y"}, {"step", "year"}, {"stepmode", "backward"}},
                                     {{"step", "all"}}})}
        };
    }

    return layout;
}

inline std::string PlotlyEngine::generate_html_wrapper(const json& data, const json& layout, const json& config,
                                                       const std::string& div_id) {
    std::stringstream html;

    html << R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Interactive Chart</title>
    <script src="https://cdn.plot.ly/plotly-2.26.0.min.js"></script>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #f8f9fa;
        }
        .chart-container {
            background: white;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            padding: 20px;
            margin: 20px 0;
        }
        .header {
            text-align: center;
            color: #333;
            margin-bottom: 30px;
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>Portfolio Analytics Dashboard</h1>
    </div>
    
    <div class="chart-container">
        <div id=")"
         << div_id << R"(" style="width:100%;height:600px;"></div>
    </div>
    
    <script>
        var plotData = )"
         << data.dump() << R"(;
        var plotLayout = )"
         << layout.dump() << R"(;
        var plotConfig = )"
         << config.dump() << R"(;
        
        Plotly.newPlot(')"
         << div_id << R"(', plotData, plotLayout, plotConfig);
        
        // Make chart responsive
        window.addEventListener('resize', function() {
            Plotly.Plots.resize(')"
         << div_id << R"(');
        });
    </script>
</body>
</html>)";

    return html.str();
}

inline std::string PlotlyEngine::datetime_to_plotly_string(const DateTime& dt) {
    return dt.to_string("%Y-%m-%d");
}

inline Result<std::string> PlotlyEngine::create_time_series_chart(const std::vector<TimeSeries<double>>& series,
                                                                  const std::vector<std::string>& labels,
                                                                  const InteractivePlotConfig& config) {
    if (series.empty()) {
        return Result<std::string>::error(ErrorCode::InvalidInput, "No data series provided");
    }

    if (labels.size() != series.size()) {
        return Result<std::string>::error(ErrorCode::InvalidInput, "Labels count must match series count");
    }

    json data                       = json::array();
    std::vector<std::string> colors = config.colors;
    if (colors.empty()) {
        colors = {"#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b", "#e377c2"};
    }

    for (size_t i = 0; i < series.size(); ++i) {
        const auto& ts = series[i];

        json trace = {
            {"type",                              "scatter"},
            {"mode",                                "lines"},
            {"name",                              labels[i]},
            {   "x",                          json::array()},
            {   "y",                          json::array()},
            {"line", {{"color", colors[i % colors.size()]}}}
        };

        const auto& timestamps = ts.timestamps();
        const auto& values     = ts.values();

        for (size_t j = 0; j < timestamps.size(); ++j) {
            trace["x"].push_back(datetime_to_plotly_string(timestamps[j]));
            trace["y"].push_back(values[j]);
        }

        if (config.enable_hover) {
            trace["hovertemplate"] = labels[i] + ": %{y:.4f}<br>%{x}<extra></extra>";
        }

        data.push_back(trace);
    }

    auto layout              = generate_layout(config, config.title);
    layout["xaxis"]["title"] = config.xlabel;
    layout["yaxis"]["title"] = config.ylabel;

    auto plotly_config = generate_plotly_config(config);

    return Result<std::string>::success(generate_html_wrapper(data, layout, plotly_config));
}

inline Result<std::string> PlotlyEngine::create_candlestick_chart(const OHLCData& ohlc_data,
                                                                  const InteractivePlotConfig& config) {
    if (ohlc_data.timestamps.empty()) {
        return Result<std::string>::error(ErrorCode::InvalidInput, "No OHLC data provided");
    }

    json data = json::array();

    // Candlestick trace
    json candlestick = {
        {      "type",                                     "candlestick"},
        {      "name", ohlc_data.name.empty() ? "Price" : ohlc_data.name},
        {         "x",                                     json::array()},
        {      "open",                                     json::array()},
        {      "high",                                     json::array()},
        {       "low",                                     json::array()},
        {     "close",                                     json::array()},
        {"increasing",                {{"line", {{"color", "#26a69a"}}}}},
        {"decreasing",                {{"line", {{"color", "#ef5350"}}}}},
        {     "xaxis",                                               "x"},
        {     "yaxis",                                               "y"}
    };

    for (size_t i = 0; i < ohlc_data.timestamps.size(); ++i) {
        candlestick["x"].push_back(datetime_to_plotly_string(ohlc_data.timestamps[i]));
        candlestick["open"].push_back(ohlc_data.open[i]);
        candlestick["high"].push_back(ohlc_data.high[i]);
        candlestick["low"].push_back(ohlc_data.low[i]);
        candlestick["close"].push_back(ohlc_data.close[i]);
    }

    data.push_back(candlestick);

    // Volume trace if available
    if (!ohlc_data.volume.empty()) {
        json volume_trace = {
            {  "type",                                "bar"},
            {  "name",                             "Volume"},
            {     "x",                        json::array()},
            {     "y",                        json::array()},
            {"marker", {{"color", "rgba(158,202,225,0.5)"}}},
            { "xaxis",                                  "x"},
            { "yaxis",                                 "y2"}
        };

        for (size_t i = 0; i < ohlc_data.timestamps.size(); ++i) {
            volume_trace["x"].push_back(datetime_to_plotly_string(ohlc_data.timestamps[i]));
            volume_trace["y"].push_back(ohlc_data.volume[i]);
        }

        data.push_back(volume_trace);
    }

    auto layout = generate_layout(config, config.title);

    // Dual y-axis for volume
    if (!ohlc_data.volume.empty()) {
        layout["yaxis2"] = {
            {     "title", "Volume"},
            {"overlaying",      "y"},
            {      "side",  "right"},
            {  "showgrid",    false}
        };
    }

    layout["xaxis"]["rangeslider"] = {
        {"visible", false}
    };  // Disable rangeslider for candlestick

    auto plotly_config = generate_plotly_config(config);

    return Result<std::string>::success(generate_html_wrapper(data, layout, plotly_config));
}

inline Result<std::string> PlotlyEngine::create_correlation_heatmap(
    const std::vector<std::vector<double>>& correlation_matrix, const std::vector<std::string>& labels,
    const InteractivePlotConfig& config) {
    if (correlation_matrix.empty() || labels.empty()) {
        return Result<std::string>::error(ErrorCode::InvalidInput, "Empty correlation matrix or labels");
    }

    json data = json::array();

    json heatmap = {
        {         "type",                                              "heatmap"},
        {            "z",                                     correlation_matrix},
        {            "x",                                                 labels},
        {            "y",                                                 labels},
        {   "colorscale",                                                 "RdBu"},
        {         "zmid",                                                      0},
        {     "colorbar",                             {{"title", "Correlation"}}},
        {"hovertemplate", "%{x} vs %{y}<br>Correlation: %{z:.3f}<extra></extra>"}
    };

    data.push_back(heatmap);

    auto layout              = generate_layout(config, config.title.empty() ? "Correlation Matrix" : config.title);
    layout["xaxis"]["title"] = "";
    layout["yaxis"]["title"] = "";
    layout["width"]          = 600;
    layout["height"]         = 600;

    auto plotly_config = generate_plotly_config(config);

    return Result<std::string>::success(generate_html_wrapper(data, layout, plotly_config));
}

}  // namespace pyfolio::visualization