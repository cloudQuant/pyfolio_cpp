/**
 * @file plotly_enhanced.cpp
 * @brief Implementation of enhanced Plotly visualization features
 * @version 1.0.0
 * @date 2024
 */

#include "../../include/pyfolio/visualization/plotly_enhanced.h"
#include "../../include/pyfolio/analytics/statistics.h"
#include "../../include/pyfolio/performance/drawdown.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace pyfolio::visualization::interactive {

Result<std::string> create_portfolio_dashboard(const TimeSeries<Return>& returns,
                                               const positions::PortfolioHoldings& holdings,
                                               const std::optional<TimeSeries<Return>>& benchmark,
                                               const std::string& output_path) {
    PlotlyEngine engine;
    InteractivePlotConfig config;
    config.theme                = "plotly_white";
    config.enable_rangeslider   = true;
    config.enable_rangeselector = true;

    std::stringstream dashboard;

    dashboard << R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Portfolio Dashboard</title>
    <script src="https://cdn.plot.ly/plotly-2.26.0.min.js"></script>
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.1.3/dist/css/bootstrap.min.css" rel="stylesheet">
    <style>
        body { 
            background-color: #f8f9fa; 
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        }
        .dashboard-header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 2rem 0;
            margin-bottom: 2rem;
        }
        .metric-card {
            background: white;
            border-radius: 10px;
            padding: 1.5rem;
            margin-bottom: 1rem;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            transition: transform 0.2s;
        }
        .metric-card:hover {
            transform: translateY(-2px);
        }
        .metric-value {
            font-size: 2rem;
            font-weight: bold;
            color: #2c3e50;
        }
        .metric-label {
            color: #7f8c8d;
            font-size: 0.9rem;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        .chart-container {
            background: white;
            border-radius: 10px;
            padding: 1.5rem;
            margin-bottom: 2rem;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        .positive { color: #27ae60; }
        .negative { color: #e74c3c; }
    </style>
</head>
<body>
    <div class="dashboard-header">
        <div class="container">
            <h1 class="text-center mb-0">Portfolio Performance Dashboard</h1>
            <p class="text-center mb-0 opacity-75">Comprehensive Analytics & Risk Metrics</p>
        </div>
    </div>
    
    <div class="container-fluid">
        <div class="row">
            <!-- Key Metrics Cards -->
            <div class="col-md-3">
                <div class="metric-card">
                    <div class="metric-label">Total Value</div>
                    <div class="metric-value">)"
              << std::fixed << std::setprecision(2) << holdings.total_value() << R"(</div>
                </div>
            </div>
            <div class="col-md-3">
                <div class="metric-card">
                    <div class="metric-label">Cash Balance</div>
                    <div class="metric-value">)"
              << holdings.cash_balance() << R"(</div>
                </div>
            </div>
            <div class="col-md-3">
                <div class="metric-card">
                    <div class="metric-label">Positions</div>
                    <div class="metric-value">)"
              << holdings.holdings().size() << R"(</div>
                </div>
            </div>
            <div class="col-md-3">
                <div class="metric-card">
                    <div class="metric-label">Last Updated</div>
                    <div class="metric-value" style="font-size: 1.2rem;">)"
              << holdings.timestamp().to_string() << R"(</div>
                </div>
            </div>
        </div>
        
        <div class="row">
            <!-- Cumulative Returns Chart -->
            <div class="col-12">
                <div class="chart-container">
                    <h4>Cumulative Returns</h4>
                    <div id="cumulative-returns" style="width:100%;height:400px;"></div>
                </div>
            </div>
        </div>
        
        <div class="row">
            <!-- Portfolio Composition -->
            <div class="col-md-6">
                <div class="chart-container">
                    <h4>Portfolio Composition</h4>
                    <div id="portfolio-pie" style="width:100%;height:400px;"></div>
                </div>
            </div>
            
            <!-- Performance Metrics -->
            <div class="col-md-6">
                <div class="chart-container">
                    <h4>Holdings Details</h4>
                    <div class="table-responsive">
                        <table class="table table-striped">
                            <thead>
                                <tr>
                                    <th>Symbol</th>
                                    <th>Shares</th>
                                    <th>Market Value</th>
                                    <th>Weight</th>
                                    <th>P&L</th>
                                </tr>
                            </thead>
                            <tbody>)";

    // Add holdings table data
    for (const auto& [symbol, holding] : holdings.holdings()) {
        dashboard << "<tr>";
        dashboard << "<td><strong>" << symbol << "</strong></td>";
        dashboard << "<td>" << std::fixed << std::setprecision(0) << holding.shares << "</td>";
        dashboard << "<td>$" << std::fixed << std::setprecision(2) << holding.market_value << "</td>";
        dashboard << "<td>" << std::fixed << std::setprecision(1) << (holding.weight * 100) << "%</td>";

        std::string pnl_class = holding.unrealized_pnl >= 0 ? "positive" : "negative";
        dashboard << "<td class=\"" << pnl_class << "\">$" << std::fixed << std::setprecision(2)
                  << holding.unrealized_pnl << "</td>";
        dashboard << "</tr>";
    }

    dashboard << R"(                            </tbody>
                        </table>
                    </div>
                </div>
            </div>
        </div>
    </div>
    
    <script>
        // Cumulative Returns Chart
        var cumData = [{
            x: [)";

    // Add cumulative returns data
    const auto& timestamps = returns.timestamps();
    std::vector<double> cum_returns;
    double cum_prod = 1.0;

    for (size_t i = 0; i < returns.size(); ++i) {
        if (i > 0)
            dashboard << ",";
        dashboard << "'" << timestamps[i].to_string() << "'";

        cum_prod *= (1.0 + returns[i]);
        cum_returns.push_back((cum_prod - 1.0) * 100);
    }

    dashboard << "],\n            y: [";

    for (size_t i = 0; i < cum_returns.size(); ++i) {
        if (i > 0)
            dashboard << ",";
        dashboard << cum_returns[i];
    }

    dashboard << R"(],
            type: 'scatter',
            mode: 'lines',
            name: 'Portfolio',
            line: { color: '#667eea', width: 3 },
            hovertemplate: 'Return: %{y:.2f}%<br>Date: %{x}<extra></extra>'
        }];
        
        var cumLayout = {
            title: { text: 'Portfolio Performance Over Time', x: 0.5 },
            xaxis: { 
                title: 'Date',
                rangeslider: { visible: true },
                rangeselector: {
                    buttons: [
                        { count: 7, label: '7D', step: 'day', stepmode: 'backward' },
                        { count: 30, label: '1M', step: 'day', stepmode: 'backward' },
                        { count: 90, label: '3M', step: 'day', stepmode: 'backward' },
                        { count: 1, label: '1Y', step: 'year', stepmode: 'backward' },
                        { step: 'all' }
                    ]
                }
            },
            yaxis: { title: 'Cumulative Return (%)' },
            template: 'plotly_white',
            hovermode: 'x unified'
        };
        
        Plotly.newPlot('cumulative-returns', cumData, cumLayout, {responsive: true});
        
        // Portfolio Composition Pie Chart
        var pieLabels = [)";

    // Add pie chart data
    std::vector<std::string> pie_labels;
    std::vector<double> pie_values;

    for (const auto& [symbol, holding] : holdings.holdings()) {
        pie_labels.push_back(symbol);
        pie_values.push_back(holding.market_value);
    }

    // Add cash if significant
    if (holdings.cash_balance() > holdings.total_value() * 0.01) {
        pie_labels.push_back("Cash");
        pie_values.push_back(holdings.cash_balance());
    }

    for (size_t i = 0; i < pie_labels.size(); ++i) {
        if (i > 0)
            dashboard << ",";
        dashboard << "'" << pie_labels[i] << "'";
    }

    dashboard << "];\n        var pieValues = [";

    for (size_t i = 0; i < pie_values.size(); ++i) {
        if (i > 0)
            dashboard << ",";
        dashboard << pie_values[i];
    }

    dashboard << R"(];
        
        var pieData = [{
            type: 'pie',
            labels: pieLabels,
            values: pieValues,
            hole: 0.3,
            hovertemplate: '%{label}<br>Value: $%{value:,.0f}<br>Share: %{percent}<extra></extra>',
            textinfo: 'label+percent',
            textposition: 'outside',
            marker: {
                colors: ['#667eea', '#764ba2', '#f093fb', '#f5576c', '#4facfe', '#00f2fe']
            }
        }];
        
        var pieLayout = {
            title: { text: 'Asset Allocation', x: 0.5 },
            template: 'plotly_white',
            showlegend: true,
            legend: { orientation: 'v', x: 1.05, y: 0.5 }
        };
        
        Plotly.newPlot('portfolio-pie', pieData, pieLayout, {responsive: true});
        
        // Make charts responsive
        window.addEventListener('resize', function() {
            Plotly.Plots.resize('cumulative-returns');
            Plotly.Plots.resize('portfolio-pie');
        });
    </script>
    
    <script src="https://cdn.jsdelivr.net/npm/bootstrap@5.1.3/dist/js/bootstrap.bundle.min.js"></script>
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

Result<std::string> create_risk_dashboard(const TimeSeries<Return>& returns,
                                          const analytics::PerformanceMetrics& metrics,
                                          const std::string& output_path) {
    std::stringstream dashboard;

    dashboard << R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Risk Analysis Dashboard</title>
    <script src="https://cdn.plot.ly/plotly-2.26.0.min.js"></script>
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.1.3/dist/css/bootstrap.min.css" rel="stylesheet">
    <style>
        body { background-color: #f8f9fa; }
        .risk-header {
            background: linear-gradient(135deg, #ff6b6b 0%, #ee5a24 100%);
            color: white;
            padding: 2rem 0;
            margin-bottom: 2rem;
        }
        .risk-card {
            background: white;
            border-radius: 10px;
            padding: 1.5rem;
            margin-bottom: 1rem;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            border-left: 4px solid #ff6b6b;
        }
        .risk-value {
            font-size: 1.8rem;
            font-weight: bold;
        }
        .risk-good { color: #27ae60; border-left-color: #27ae60; }
        .risk-warning { color: #f39c12; border-left-color: #f39c12; }
        .risk-danger { color: #e74c3c; border-left-color: #e74c3c; }
        .chart-container {
            background: white;
            border-radius: 10px;
            padding: 1.5rem;
            margin-bottom: 2rem;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
    </style>
</head>
<body>
    <div class="risk-header">
        <div class="container">
            <h1 class="text-center mb-0">Risk Analysis Dashboard</h1>
            <p class="text-center mb-0 opacity-75">Comprehensive Risk Metrics & Stress Testing</p>
        </div>
    </div>
    
    <div class="container-fluid">
        <div class="row">
            <!-- Risk Metrics -->
            <div class="col-md-4">
                <div class="risk-card )"
              << (metrics.sharpe_ratio > 1.0 ? "risk-good"
                                             : (metrics.sharpe_ratio > 0.5 ? "risk-warning" : "risk-danger"))
              << R"(">
                    <div class="small text-muted">SHARPE RATIO</div>
                    <div class="risk-value">)"
              << std::fixed << std::setprecision(3) << metrics.sharpe_ratio << R"(</div>
                </div>
            </div>
            <div class="col-md-4">
                <div class="risk-card )"
              << (metrics.max_drawdown < 0.1 ? "risk-good"
                                             : (metrics.max_drawdown < 0.2 ? "risk-warning" : "risk-danger"))
              << R"(">
                    <div class="small text-muted">MAX DRAWDOWN</div>
                    <div class="risk-value">)"
              << std::fixed << std::setprecision(2) << (metrics.max_drawdown * 100) << R"(%</div>
                </div>
            </div>
            <div class="col-md-4">
                <div class="risk-card )"
              << (metrics.var_95 > -0.05 ? "risk-good" : (metrics.var_95 > -0.1 ? "risk-warning" : "risk-danger"))
              << R"(">
                    <div class="small text-muted">VALUE AT RISK (95%)</div>
                    <div class="risk-value">)"
              << std::fixed << std::setprecision(2) << (metrics.var_95 * 100) << R"(%</div>
                </div>
            </div>
        </div>
        
        <div class="row">
            <!-- Returns Distribution -->
            <div class="col-md-6">
                <div class="chart-container">
                    <h4>Returns Distribution</h4>
                    <div id="returns-histogram" style="width:100%;height:400px;"></div>
                </div>
            </div>
            
            <!-- Risk Metrics Radar -->
            <div class="col-md-6">
                <div class="chart-container">
                    <h4>Risk Profile</h4>
                    <div id="risk-radar" style="width:100%;height:400px;"></div>
                </div>
            </div>
        </div>
        
        <div class="row">
            <!-- Drawdown Chart -->
            <div class="col-12">
                <div class="chart-container">
                    <h4>Underwater Plot (Drawdowns)</h4>
                    <div id="drawdown-chart" style="width:100%;height:300px;"></div>
                </div>
            </div>
        </div>
    </div>
    
    <script>
        // Returns Distribution Histogram
        var returns = [)";

    // Add returns data for histogram
    const auto& return_values = returns.values();
    for (size_t i = 0; i < return_values.size(); ++i) {
        if (i > 0)
            dashboard << ",";
        dashboard << (return_values[i] * 100);  // Convert to percentage
    }

    dashboard << R"(];
        
        var histData = [{
            x: returns,
            type: 'histogram',
            nbinsx: 50,
            marker: { color: '#ff6b6b', opacity: 0.7 },
            name: 'Returns'
        }];
        
        var histLayout = {
            title: 'Daily Returns Distribution',
            xaxis: { title: 'Daily Return (%)' },
            yaxis: { title: 'Frequency' },
            template: 'plotly_white'
        };
        
        Plotly.newPlot('returns-histogram', histData, histLayout, {responsive: true});
        
        // Risk Radar Chart
        var radarData = [{
            type: 'scatterpolar',
            r: [)"
              << std::min(metrics.sharpe_ratio / 2.0, 1.0) << "," << std::min((1.0 - metrics.max_drawdown), 1.0) << ","
              << std::min((1.0 + metrics.var_95 * 10), 1.0) << "," << std::min(metrics.annual_return / 0.2, 1.0) << ","
              << std::min((1.0 - metrics.annual_volatility), 1.0) << R"(],
            theta: ['Sharpe Ratio', 'Drawdown Control', 'VaR Score', 'Returns', 'Volatility Control'],
            fill: 'toself',
            marker: { color: '#ff6b6b' },
            line: { color: '#c0392b' },
            name: 'Risk Profile'
        }];
        
        var radarLayout = {
            polar: {
                radialaxis: {
                    visible: true,
                    range: [0, 1]
                }
            },
            template: 'plotly_white'
        };
        
        Plotly.newPlot('risk-radar', radarData, radarLayout, {responsive: true});
        
        // Drawdown Underwater Plot
        var drawdownData = [{
            x: [)";

    // Calculate and add drawdown data
    const auto& timestamps = returns.timestamps();
    std::vector<double> cumulative_max;
    std::vector<double> drawdowns;

    double peak       = 0.0;
    double cumulative = 0.0;

    for (size_t i = 0; i < timestamps.size(); ++i) {
        if (i > 0)
            dashboard << ",";
        dashboard << "'" << timestamps[i].to_string() << "'";

        cumulative += return_values[i];
        peak = std::max(peak, cumulative);
        drawdowns.push_back((cumulative - peak) * 100);  // Convert to percentage
    }

    dashboard << "],\n            y: [";

    for (size_t i = 0; i < drawdowns.size(); ++i) {
        if (i > 0)
            dashboard << ",";
        dashboard << drawdowns[i];
    }

    dashboard << R"(],
            type: 'scatter',
            mode: 'lines',
            fill: 'tonexty',
            fillcolor: 'rgba(255, 107, 107, 0.3)',
            line: { color: '#ff6b6b', width: 2 },
            name: 'Drawdown',
            hovertemplate: 'Drawdown: %{y:.2f}%<br>Date: %{x}<extra></extra>'
        }];
        
        var drawdownLayout = {
            title: 'Portfolio Drawdowns Over Time',
            xaxis: { title: 'Date' },
            yaxis: { title: 'Drawdown (%)' },
            template: 'plotly_white',
            hovermode: 'x unified'
        };
        
        Plotly.newPlot('drawdown-chart', drawdownData, drawdownLayout, {responsive: true});
        
        // Make charts responsive
        window.addEventListener('resize', function() {
            Plotly.Plots.resize('returns-histogram');
            Plotly.Plots.resize('risk-radar');
            Plotly.Plots.resize('drawdown-chart');
        });
    </script>
    
    <script src="https://cdn.jsdelivr.net/npm/bootstrap@5.1.3/dist/js/bootstrap.bundle.min.js"></script>
</body>
</html>)";

    // Save to file
    std::ofstream file(output_path);
    if (!file.is_open()) {
        return Result<std::string>::error(ErrorCode::FileNotFound, "Cannot create risk dashboard file");
    }

    std::string content = dashboard.str();
    file << content;
    file.close();

    return Result<std::string>::success(content);
}

Result<std::string> create_attribution_waterfall(const std::map<std::string, double>& attribution_factors,
                                                 const InteractivePlotConfig& config) {
    if (attribution_factors.empty()) {
        return Result<std::string>::error(ErrorCode::InvalidInput, "No attribution factors provided");
    }

    PlotlyEngine engine;

    json data = json::array();

    // Prepare waterfall data
    std::vector<std::string> labels;
    std::vector<double> values;
    std::vector<std::string> colors;

    double cumulative = 0.0;

    for (const auto& [factor, value] : attribution_factors) {
        labels.push_back(factor);
        values.push_back(value);
        cumulative += value;

        // Color coding
        if (value > 0) {
            colors.push_back("#27ae60");
        } else {
            colors.push_back("#e74c3c");
        }
    }

    // Add total
    labels.push_back("Total");
    values.push_back(cumulative);
    colors.push_back("#3498db");

    json waterfall = {
        {         "type",                                    "waterfall"},
        {            "x",                                         labels},
        {            "y",                                         values},
        {    "connector",             {{"line", {{"color", "#636363"}}}}},
        {   "increasing",           {{"marker", {{"color", "#27ae60"}}}}},
        {   "decreasing",           {{"marker", {{"color", "#e74c3c"}}}}},
        {       "totals",           {{"marker", {{"color", "#3498db"}}}}},
        {"hovertemplate", "%{x}<br>Attribution: %{y:.3f}<extra></extra>"}
    };

    data.push_back(waterfall);

    auto layout              = engine.generate_layout(config, "Performance Attribution Waterfall");
    layout["xaxis"]["title"] = "Attribution Factors";
    layout["yaxis"]["title"] = "Contribution to Return";

    auto plotly_config = engine.generate_plotly_config(config);

    return Result<std::string>::success(engine.generate_html_wrapper(data, layout, plotly_config));
}

Result<std::string> RealTimeChart::initialize(const TimeSeries<double>& initial_data,
                                              const InteractivePlotConfig& config) {
    current_data_ = json::array();
    layout_       = json::object();

    // Create initial data structure
    json trace = {
        {"type",              "scatter"},
        {"mode",                "lines"},
        {"name",            "Live Data"},
        {   "x",          json::array()},
        {   "y",          json::array()},
        {"line", {{"color", "#1f77b4"}}}
    };

    const auto& timestamps = initial_data.timestamps();
    const auto& values     = initial_data.values();

    for (size_t i = 0; i < timestamps.size(); ++i) {
        trace["x"].push_back(timestamps[i].to_string());
        trace["y"].push_back(values[i]);
    }

    current_data_.push_back(trace);

    // Create layout
    layout_ = {
        {   "title", config.title.empty() ? "Real-Time Chart" : config.title},
        {   "xaxis",                                     {{"title", "Time"}}},
        {   "yaxis",                                    {{"title", "Value"}}},
        {"template",                                            config.theme}
    };

    // Generate HTML with WebSocket integration
    std::stringstream html;
    html << R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>Real-Time Chart</title>
    <script src="https://cdn.plot.ly/plotly-2.26.0.min.js"></script>
</head>
<body>
    <div id=")"
         << chart_id_ << R"(" style="width:100%;height:600px;"></div>
    
    <script>
        var data = )"
         << current_data_.dump() << R"(;
        var layout = )"
         << layout_.dump() << R"(;
        
        Plotly.newPlot(')"
         << chart_id_ << R"(', data, layout);
        
        // WebSocket connection for live updates (example)
        function connectWebSocket() {
            // var ws = new WebSocket('ws://localhost:8080/live-data');
            // ws.onmessage = function(event) {
            //     var update = JSON.parse(event.data);
            //     Plotly.extendTraces(')"
         << chart_id_ << R"(', {
            //         x: [[update.timestamp]],
            //         y: [[update.value]]
            //     }, [0]);
            // };
        }
        
        // Simulate live updates
        function simulateUpdates() {
            setInterval(function() {
                var now = new Date();
                var value = Math.random() * 100;
                
                Plotly.extendTraces(')"
         << chart_id_ << R"(', {
                    x: [[now.toISOString().split('T')[0]]],
                    y: [[value]]
                }, [0]);
            }, 1000);
        }
        
        // Start simulation
        simulateUpdates();
    </script>
</body>
</html>)";

    return Result<std::string>::success(html.str());
}

}  // namespace pyfolio::visualization::interactive

// Add missing PlotlyEngine implementations
namespace pyfolio::visualization {

Result<std::string> PlotlyEngine::create_treemap(const std::vector<std::string>& labels,
                                                 const std::vector<std::string>& parents,
                                                 const std::vector<double>& values,
                                                 const InteractivePlotConfig& config) {
    if (labels.empty() || labels.size() != parents.size() || labels.size() != values.size()) {
        return Result<std::string>::error(ErrorCode::InvalidInput, "Treemap data arrays must be same size");
    }

    json data = json::array();

    json treemap = {
        {         "type",                                                                 "treemap"},
        {       "labels",                                                                    labels},
        {      "parents",                                                                   parents},
        {       "values",                                                                    values},
        {     "textinfo",                                              "label+value+percent parent"},
        {"hovertemplate", "%{label}<br>Value: %{value}<br>Percent: %{percentParent}<extra></extra>"},
        {     "maxdepth",                                                                         3}
    };

    data.push_back(treemap);

    auto layout        = generate_layout(config, config.title.empty() ? "Treemap Visualization" : config.title);
    auto plotly_config = generate_plotly_config(config);

    return Result<std::string>::success(generate_html_wrapper(data, layout, plotly_config));
}

Result<std::string> PlotlyEngine::create_waterfall_chart(const std::vector<std::string>& labels,
                                                         const std::vector<double>& values,
                                                         const InteractivePlotConfig& config) {
    if (labels.empty() || labels.size() != values.size()) {
        return Result<std::string>::error(ErrorCode::InvalidInput, "Waterfall data arrays must be same size");
    }

    json data = json::array();

    json waterfall = {
        {         "type",                          "waterfall"},
        {            "x",                               labels},
        {            "y",                               values},
        {    "connector",   {{"line", {{"color", "#636363"}}}}},
        {   "increasing", {{"marker", {{"color", "#27ae60"}}}}},
        {   "decreasing", {{"marker", {{"color", "#e74c3c"}}}}},
        {       "totals", {{"marker", {{"color", "#3498db"}}}}},
        {"hovertemplate", "%{x}<br>Value: %{y}<extra></extra>"}
    };

    data.push_back(waterfall);

    auto layout              = generate_layout(config, config.title.empty() ? "Waterfall Chart" : config.title);
    layout["xaxis"]["title"] = "Categories";
    layout["yaxis"]["title"] = "Values";

    auto plotly_config = generate_plotly_config(config);

    return Result<std::string>::success(generate_html_wrapper(data, layout, plotly_config));
}

Result<std::string> PlotlyEngine::create_3d_surface(const std::vector<std::vector<double>>& z_data,
                                                    const std::vector<double>& x_data,
                                                    const std::vector<double>& y_data,
                                                    const InteractivePlotConfig& config) {
    if (z_data.empty() || x_data.empty() || y_data.empty()) {
        return Result<std::string>::error(ErrorCode::InvalidInput, "3D surface data cannot be empty");
    }

    json data = json::array();

    json surface = {
        {         "type",                                      "surface"},
        {            "z",                                         z_data},
        {            "x",                                         x_data},
        {            "y",                                         y_data},
        {   "colorscale",                                      "Viridis"},
        {"hovertemplate", "X: %{x}<br>Y: %{y}<br>Z: %{z}<extra></extra>"}
    };

    data.push_back(surface);

    auto layout     = generate_layout(config, config.title.empty() ? "3D Surface Plot" : config.title);
    layout["scene"] = {
        {"xaxis", {{"title", "X Axis"}}},
        {"yaxis", {{"title", "Y Axis"}}},
        {"zaxis", {{"title", "Z Axis"}}}
    };

    auto plotly_config = generate_plotly_config(config);

    return Result<std::string>::success(generate_html_wrapper(data, layout, plotly_config));
}

Result<std::string> PlotlyEngine::create_subplots_dashboard(const std::vector<json>& subplot_configs, int rows,
                                                            int cols, const InteractivePlotConfig& config) {
    if (subplot_configs.empty()) {
        return Result<std::string>::error(ErrorCode::InvalidInput, "No subplot configurations provided");
    }

    json data = json::array();

    // Combine all subplot data
    for (const auto& subplot : subplot_configs) {
        if (subplot.contains("data")) {
            for (const auto& trace : subplot["data"]) {
                data.push_back(trace);
            }
        }
    }

    auto layout = generate_layout(config, config.title.empty() ? "Dashboard" : config.title);

    // Add subplot configuration
    layout["grid"] = {
        {   "rows",          rows},
        {"columns",          cols},
        {"pattern", "independent"}
    };

    auto plotly_config = generate_plotly_config(config);

    return Result<std::string>::success(generate_html_wrapper(data, layout, plotly_config));
}

}  // namespace pyfolio::visualization