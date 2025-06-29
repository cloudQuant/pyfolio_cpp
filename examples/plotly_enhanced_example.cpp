/**
 * @file plotly_enhanced_example.cpp
 * @brief Example demonstrating enhanced Plotly visualization features
 * @version 1.0.0
 * @date 2024
 *
 * This example showcases the advanced interactive visualization capabilities
 * of the pyfolio_cpp library using Plotly.js integration.
 */

#include "pyfolio/analytics/performance_metrics.h"
#include "pyfolio/analytics/statistics.h"
#include "pyfolio/core/time_series.h"
#include "pyfolio/positions/positions.h"
#include "pyfolio/visualization/plotly_enhanced.h"
#include <filesystem>
#include <iostream>
#include <random>

using namespace pyfolio;
using namespace pyfolio::visualization;
using namespace pyfolio::visualization::interactive;

/**
 * @brief Generate sample market data for demonstration
 */
std::pair<TimeSeries<Return>, TimeSeries<Return>> generate_sample_data() {
    std::vector<DateTime> dates;
    std::vector<double> strategy_returns;
    std::vector<double> benchmark_returns;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> strategy_dist(0.0008, 0.015);   // 0.08% daily return, 1.5% volatility
    std::normal_distribution<double> benchmark_dist(0.0005, 0.012);  // 0.05% daily return, 1.2% volatility

    // Generate 2 years of daily data
    for (int i = 0; i < 504; ++i) {
        DateTime base_date(2022, 1, 1);
        dates.push_back(base_date.add_days(i));
        strategy_returns.push_back(strategy_dist(gen));
        benchmark_returns.push_back(benchmark_dist(gen));
    }

    auto strategy_ts  = TimeSeries<Return>::create(dates, strategy_returns).value();
    auto benchmark_ts = TimeSeries<Return>::create(dates, benchmark_returns).value();

    return {strategy_ts, benchmark_ts};
}

/**
 * @brief Create sample portfolio holdings
 */
positions::PortfolioHoldings create_sample_portfolio() {
    DateTime base_date(2024, 1, 15);
    positions::PortfolioHoldings holdings(base_date, 25000.0);

    // Tech sector
    holdings.update_holding("AAPL", 150.0, 180.0, 185.0);
    holdings.update_holding("GOOGL", 80.0, 2750.0, 2800.0);
    holdings.update_holding("MSFT", 200.0, 375.0, 380.0);
    holdings.update_holding("NVDA", 100.0, 750.0, 780.0);

    // Finance sector
    holdings.update_holding("JPM", 300.0, 145.0, 148.0);
    holdings.update_holding("BAC", 500.0, 32.0, 33.0);
    holdings.update_holding("GS", 75.0, 380.0, 385.0);

    // Healthcare sector
    holdings.update_holding("JNJ", 200.0, 158.0, 160.0);
    holdings.update_holding("PFE", 400.0, 28.0, 29.0);

    return holdings;
}

/**
 * @brief Generate OHLC data for candlestick chart
 */
OHLCData generate_ohlc_data() {
    OHLCData ohlc_data;
    ohlc_data.name = "Sample Stock";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> price_change(-2.0, 2.0);
    std::uniform_real_distribution<double> volume_factor(0.8, 1.2);

    double base_price = 100.0;

    for (int i = 0; i < 60; ++i) {  // 2 months of daily data
        DateTime base_date(2024, 1, 1);
        ohlc_data.timestamps.push_back(base_date.add_days(i));

        double open   = base_price;
        double change = price_change(gen);
        double close  = open + change;
        double high   = std::max(open, close) + std::abs(price_change(gen) * 0.5);
        double low    = std::min(open, close) - std::abs(price_change(gen) * 0.5);
        double volume = 1000000 * volume_factor(gen);

        ohlc_data.open.push_back(open);
        ohlc_data.high.push_back(high);
        ohlc_data.low.push_back(low);
        ohlc_data.close.push_back(close);
        ohlc_data.volume.push_back(volume);

        base_price = close;
    }

    return ohlc_data;
}

int main() {
    std::cout << "Enhanced Plotly Visualization Example\n";
    std::cout << "=====================================\n\n";

    // Create output directory
    std::string output_dir = "plotly_enhanced_output/";
    std::filesystem::create_directories(output_dir);

    try {
        // 1. Generate sample data
        std::cout << "1. Generating sample market data...\n";
        auto [strategy_returns, benchmark_returns] = generate_sample_data();
        auto portfolio_holdings                    = create_sample_portfolio();
        auto ohlc_data                             = generate_ohlc_data();

        // 2. Create PlotlyEngine instance
        PlotlyEngine engine;

        // 3. Create time series comparison chart
        std::cout << "2. Creating time series comparison chart...\n";
        std::vector<TimeSeries<double>> return_series = {strategy_returns, benchmark_returns};
        std::vector<std::string> series_labels        = {"Strategy", "Benchmark"};

        InteractivePlotConfig ts_config;
        ts_config.title                = "Strategy vs Benchmark Performance";
        ts_config.xlabel               = "Date";
        ts_config.ylabel               = "Cumulative Return";
        ts_config.theme                = "plotly_white";
        ts_config.enable_rangeslider   = true;
        ts_config.enable_rangeselector = true;

        auto ts_result = engine.create_time_series_chart(return_series, series_labels, ts_config);
        if (ts_result.is_ok()) {
            std::ofstream file(output_dir + "time_series_comparison.html");
            file << ts_result.value();
            file.close();
            std::cout << "   ✓ Time series chart saved to " << output_dir << "time_series_comparison.html\n";
        }

        // 4. Create candlestick chart
        std::cout << "3. Creating candlestick chart...\n";
        InteractivePlotConfig candlestick_config;
        candlestick_config.title = "Stock Price Movement";
        candlestick_config.theme = "plotly_dark";

        auto candlestick_result = engine.create_candlestick_chart(ohlc_data, candlestick_config);
        if (candlestick_result.is_ok()) {
            std::ofstream file(output_dir + "candlestick_chart.html");
            file << candlestick_result.value();
            file.close();
            std::cout << "   ✓ Candlestick chart saved to " << output_dir << "candlestick_chart.html\n";
        }

        // 5. Create correlation heatmap
        std::cout << "4. Creating correlation heatmap...\n";
        std::vector<std::vector<double>> correlation_matrix = {
            {1.00, 0.85, 0.72, 0.68, 0.45, 0.38, 0.42},
            {0.85, 1.00, 0.78, 0.72, 0.42, 0.35, 0.39},
            {0.72, 0.78, 1.00, 0.68, 0.38, 0.32, 0.36},
            {0.68, 0.72, 0.68, 1.00, 0.35, 0.28, 0.33},
            {0.45, 0.42, 0.38, 0.35, 1.00, 0.78, 0.82},
            {0.38, 0.35, 0.32, 0.28, 0.78, 1.00, 0.75},
            {0.42, 0.39, 0.36, 0.33, 0.82, 0.75, 1.00}
        };
        std::vector<std::string> asset_labels = {"AAPL", "GOOGL", "MSFT", "NVDA", "JPM", "BAC", "GS"};

        auto heatmap_result = engine.create_correlation_heatmap(correlation_matrix, asset_labels);
        if (heatmap_result.is_ok()) {
            std::ofstream file(output_dir + "correlation_heatmap.html");
            file << heatmap_result.value();
            file.close();
            std::cout << "   ✓ Correlation heatmap saved to " << output_dir << "correlation_heatmap.html\n";
        }

        // 6. Create portfolio treemap
        std::cout << "5. Creating portfolio treemap...\n";
        std::vector<std::string> treemap_labels  = {"Portfolio", "Technology", "Finance", "Healthcare", "AAPL",
                                                    "GOOGL",     "MSFT",       "NVDA",    "JPM",        "BAC",
                                                    "GS",        "JNJ",        "PFE"};
        std::vector<std::string> treemap_parents = {"",           "Portfolio",  "Portfolio",  "Portfolio", "Technology",
                                                    "Technology", "Technology", "Technology", "Finance",   "Finance",
                                                    "Finance",    "Healthcare", "Healthcare"};
        std::vector<double> treemap_values       = {
            0,     0,     0,    0,     // Sector totals will be calculated
            27750, 22400, 7600, 7800,  // Tech holdings
            4440,  1650,  2887,        // Finance holdings
            3200,  1160                // Healthcare holdings
        };

        auto treemap_result = engine.create_treemap(treemap_labels, treemap_parents, treemap_values);
        if (treemap_result.is_ok()) {
            std::ofstream file(output_dir + "portfolio_treemap.html");
            file << treemap_result.value();
            file.close();
            std::cout << "   ✓ Portfolio treemap saved to " << output_dir << "portfolio_treemap.html\n";
        }

        // 7. Create performance attribution waterfall
        std::cout << "6. Creating performance attribution waterfall...\n";
        std::map<std::string, double> attribution_factors = {
            {   "Asset Selection",  0.028},
            { "Sector Allocation",  0.015},
            {     "Market Timing", -0.008},
            {"Security Selection",  0.019},
            {   "Currency Effect", -0.003},
            { "Transaction Costs", -0.012}
        };

        auto waterfall_result = create_attribution_waterfall(attribution_factors);
        if (waterfall_result.is_ok()) {
            std::ofstream file(output_dir + "attribution_waterfall.html");
            file << waterfall_result.value();
            file.close();
            std::cout << "   ✓ Attribution waterfall saved to " << output_dir << "attribution_waterfall.html\n";
        }

        // 8. Create 3D surface plot for risk analysis
        std::cout << "7. Creating 3D risk surface plot...\n";
        std::vector<std::vector<double>> risk_surface;
        std::vector<double> volatility_range, return_range;

        // Generate risk-return surface
        for (int i = 0; i < 20; ++i) {
            volatility_range.push_back(0.05 + i * 0.01);  // 5% to 24% volatility
            return_range.push_back(0.02 + i * 0.008);     // 2% to 17.2% return
        }

        for (size_t i = 0; i < volatility_range.size(); ++i) {
            std::vector<double> row;
            for (size_t j = 0; j < return_range.size(); ++j) {
                // Calculate Sharpe ratio for each point
                double sharpe = (return_range[j] - 0.02) / volatility_range[i];  // Assuming 2% risk-free rate
                row.push_back(sharpe);
            }
            risk_surface.push_back(row);
        }

        InteractivePlotConfig surface_config;
        surface_config.title = "Risk-Return Surface (Sharpe Ratio)";
        surface_config.theme = "plotly_white";

        auto surface_result = engine.create_3d_surface(risk_surface, volatility_range, return_range, surface_config);
        if (surface_result.is_ok()) {
            std::ofstream file(output_dir + "risk_surface_3d.html");
            file << surface_result.value();
            file.close();
            std::cout << "   ✓ 3D risk surface saved to " << output_dir << "risk_surface_3d.html\n";
        }

        // 9. Create comprehensive portfolio dashboard
        std::cout << "8. Creating comprehensive portfolio dashboard...\n";
        std::string portfolio_dashboard_path = output_dir + "portfolio_dashboard.html";
        auto dashboard_result = create_portfolio_dashboard(strategy_returns, portfolio_holdings, benchmark_returns,
                                                           portfolio_dashboard_path);
        if (dashboard_result.is_ok()) {
            std::cout << "   ✓ Portfolio dashboard saved to " << portfolio_dashboard_path << "\n";
        }

        // 10. Create risk analysis dashboard
        std::cout << "9. Creating risk analysis dashboard...\n";

        // Calculate performance metrics for the dashboard
        analytics::PerformanceMetrics metrics{.total_return      = 0.168,
                                              .annual_return     = 0.124,
                                              .annual_volatility = 0.185,
                                              .sharpe_ratio      = 0.73,
                                              .sortino_ratio     = 0.89,
                                              .max_drawdown      = 0.085,
                                              .calmar_ratio      = 1.46,
                                              .var_95            = -0.028,
                                              .beta              = 1.08,
                                              .alpha             = 0.024,
                                              .tracking_error    = 0.048,
                                              .information_ratio = 0.42,
                                              .omega_ratio       = 1.28,
                                              .skewness          = -0.18,
                                              .kurtosis          = 3.4};

        std::string risk_dashboard_path = output_dir + "risk_dashboard.html";
        auto risk_dashboard_result      = create_risk_dashboard(strategy_returns, metrics, risk_dashboard_path);
        if (risk_dashboard_result.is_ok()) {
            std::cout << "   ✓ Risk dashboard saved to " << risk_dashboard_path << "\n";
        }

        // 11. Create real-time chart template
        std::cout << "10. Creating real-time chart template...\n";
        RealTimeChart real_time_chart("live-portfolio-chart");
        auto rt_result = real_time_chart.initialize(strategy_returns);
        if (rt_result.is_ok()) {
            std::ofstream file(output_dir + "realtime_chart_template.html");
            file << rt_result.value();
            file.close();
            std::cout << "   ✓ Real-time chart template saved to " << output_dir << "realtime_chart_template.html\n";
        }

        // Summary
        std::cout << "\n" << std::string(50, '=') << "\n";
        std::cout << "✅ Enhanced Plotly Visualization Example Complete!\n\n";
        std::cout << "Generated visualizations:\n";
        std::cout << "  📈 Time Series Comparison (Strategy vs Benchmark)\n";
        std::cout << "  🕯️  Candlestick Chart with Volume\n";
        std::cout << "  🔥 Correlation Heatmap\n";
        std::cout << "  🌳 Portfolio Allocation Treemap\n";
        std::cout << "  💧 Performance Attribution Waterfall\n";
        std::cout << "  🎯 3D Risk-Return Surface\n";
        std::cout << "  📊 Comprehensive Portfolio Dashboard\n";
        std::cout << "  ⚠️  Risk Analysis Dashboard\n";
        std::cout << "  🔄 Real-time Chart Template\n\n";

        std::cout << "📁 All files saved to: " << output_dir << "\n";
        std::cout << "🌐 Open any .html file in your browser to view interactive charts\n\n";

        std::cout << "Key features demonstrated:\n";
        std::cout << "  • Interactive zooming and panning\n";
        std::cout << "  • Range slider and selector controls\n";
        std::cout << "  • Multiple chart types and themes\n";
        std::cout << "  • Responsive Bootstrap layouts\n";
        std::cout << "  • Professional financial dashboards\n";
        std::cout << "  • Real-time chart capabilities\n";
        std::cout << "  • Modern web-based visualization\n\n";

    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}