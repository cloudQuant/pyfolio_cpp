#include "pyfolio/analytics/performance_metrics.h"
#include "pyfolio/core/time_series.h"
#include "pyfolio/positions/positions.h"
#include "pyfolio/visualization/plotly_enhanced.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>

using namespace pyfolio;
using namespace pyfolio::visualization;
using namespace pyfolio::visualization::interactive;

class PlotlyEnhancedTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Create sample time series data
        std::vector<DateTime> dates;
        std::vector<double> returns;

        for (int i = 0; i < 100; ++i) {
            DateTime base_date(2023, 1, 1);
            dates.push_back(base_date.add_days(i));
            returns.push_back(0.001 * (i % 10 - 5) + 0.0001 * (rand() % 100 - 50));
        }

        auto ts_result = TimeSeries<Return>::create(dates, returns);
        ASSERT_TRUE(ts_result.is_ok());
        sample_returns_ = ts_result.value();

        // Create sample holdings
        base_date_       = DateTime(2023, 6, 15);
        sample_holdings_ = std::make_unique<positions::PortfolioHoldings>(base_date_, 10000.0);
        sample_holdings_->update_holding("AAPL", 100.0, 150.0, 155.0);
        sample_holdings_->update_holding("GOOGL", 50.0, 2800.0, 2850.0);
        sample_holdings_->update_holding("MSFT", 200.0, 350.0, 360.0);

        // Create sample performance metrics
        sample_metrics_ = analytics::PerformanceMetrics{.total_return      = 0.15,
                                                        .annual_return     = 0.12,
                                                        .annual_volatility = 0.18,
                                                        .sharpe_ratio      = 0.67,
                                                        .sortino_ratio     = 0.85,
                                                        .max_drawdown      = 0.08,
                                                        .calmar_ratio      = 1.5,
                                                        .var_95            = -0.025,
                                                        .beta              = 1.05,
                                                        .alpha             = 0.02,
                                                        .tracking_error    = 0.05,
                                                        .information_ratio = 0.4,
                                                        .omega_ratio       = 1.25,
                                                        .skewness          = -0.15,
                                                        .kurtosis          = 3.2};

        // Set up output directory
        output_dir_ = "/tmp/plotly_test_output/";
        std::filesystem::create_directories(output_dir_);
    }

    void TearDown() override {
        // Clean up test files
        std::filesystem::remove_all(output_dir_);
    }

    TimeSeries<Return> sample_returns_;
    DateTime base_date_;
    std::unique_ptr<positions::PortfolioHoldings> sample_holdings_;
    analytics::PerformanceMetrics sample_metrics_;
    std::string output_dir_;
};

TEST_F(PlotlyEnhancedTest, PlotlyEngineBasicFunctionality) {
    PlotlyEngine engine;

    // Test time series chart creation
    std::vector<TimeSeries<double>> series = {sample_returns_};
    std::vector<std::string> labels        = {"Test Strategy"};

    auto result = engine.create_time_series_chart(series, labels);
    EXPECT_TRUE(result.is_ok());

    std::string html = result.value();
    EXPECT_TRUE(html.find("plotly") != std::string::npos);
    EXPECT_TRUE(html.find("Test Strategy") != std::string::npos);
    EXPECT_TRUE(html.find("<!DOCTYPE html>") != std::string::npos);
}

TEST_F(PlotlyEnhancedTest, CandlestickChartCreation) {
    PlotlyEngine engine;

    // Create sample OHLC data
    OHLCData ohlc_data;
    ohlc_data.name = "Test Asset";

    for (int i = 0; i < 20; ++i) {
        DateTime base_date(2023, 1, 1);
        ohlc_data.timestamps.push_back(base_date.add_days(i));
        double base_price = 100.0 + i * 0.5;
        ohlc_data.open.push_back(base_price);
        ohlc_data.high.push_back(base_price + 2.0);
        ohlc_data.low.push_back(base_price - 1.5);
        ohlc_data.close.push_back(base_price + 0.5);
        ohlc_data.volume.push_back(1000000 + i * 10000);
    }

    auto result = engine.create_candlestick_chart(ohlc_data);
    EXPECT_TRUE(result.is_ok());

    std::string html = result.value();
    EXPECT_TRUE(html.find("candlestick") != std::string::npos);
    EXPECT_TRUE(html.find("Test Asset") != std::string::npos);
    EXPECT_TRUE(html.find("Volume") != std::string::npos);
}

TEST_F(PlotlyEnhancedTest, CorrelationHeatmapCreation) {
    PlotlyEngine engine;

    // Create correlation matrix
    std::vector<std::vector<double>> correlation_matrix = {
        {1.0, 0.7, 0.3},
        {0.7, 1.0, 0.5},
        {0.3, 0.5, 1.0}
    };

    std::vector<std::string> labels = {"Asset1", "Asset2", "Asset3"};

    auto result = engine.create_correlation_heatmap(correlation_matrix, labels);
    EXPECT_TRUE(result.is_ok());

    std::string html = result.value();
    EXPECT_TRUE(html.find("heatmap") != std::string::npos);
    EXPECT_TRUE(html.find("Asset1") != std::string::npos);
    EXPECT_TRUE(html.find("Correlation") != std::string::npos);
}

TEST_F(PlotlyEnhancedTest, TreemapCreation) {
    PlotlyEngine engine;

    std::vector<std::string> labels  = {"Tech", "Finance", "Healthcare", "AAPL", "GOOGL", "JPM", "BAC", "JNJ", "PFE"};
    std::vector<std::string> parents = {"", "", "", "Tech", "Tech", "Finance", "Finance", "Healthcare", "Healthcare"};
    std::vector<double> values       = {0, 0, 0, 15000, 14250, 8000, 7500, 6000, 5500};

    auto result = engine.create_treemap(labels, parents, values);
    EXPECT_TRUE(result.is_ok());

    std::string html = result.value();
    EXPECT_TRUE(html.find("treemap") != std::string::npos);
    EXPECT_TRUE(html.find("Tech") != std::string::npos);
}

TEST_F(PlotlyEnhancedTest, WaterfallChartCreation) {
    PlotlyEngine engine;

    std::vector<std::string> labels = {"Starting Value", "Q1 Performance", "Q2 Performance",
                                       "Q3 Performance", "Q4 Performance", "Final Value"};
    std::vector<double> values      = {100000, 5000, -2000, 8000, -1500, 0};

    auto result = engine.create_waterfall_chart(labels, values);
    EXPECT_TRUE(result.is_ok());

    std::string html = result.value();
    EXPECT_TRUE(html.find("waterfall") != std::string::npos);
    EXPECT_TRUE(html.find("Starting Value") != std::string::npos);
}

TEST_F(PlotlyEnhancedTest, PortfolioeDashboardCreation) {
    std::string output_path = output_dir_ + "portfolio_dashboard.html";

    auto result = create_portfolio_dashboard(sample_returns_, *sample_holdings_, std::nullopt, output_path);
    EXPECT_TRUE(result.is_ok());

    // Check that file was created
    EXPECT_TRUE(std::filesystem::exists(output_path));

    // Check file content
    std::ifstream file(output_path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    EXPECT_TRUE(content.find("Portfolio Performance Dashboard") != std::string::npos);
    EXPECT_TRUE(content.find("AAPL") != std::string::npos);
    EXPECT_TRUE(content.find("GOOGL") != std::string::npos);
    EXPECT_TRUE(content.find("MSFT") != std::string::npos);
    EXPECT_TRUE(content.find("Cumulative Returns") != std::string::npos);
    EXPECT_TRUE(content.find("Portfolio Composition") != std::string::npos);
    EXPECT_TRUE(content.find("bootstrap") != std::string::npos);
}

TEST_F(PlotlyEnhancedTest, RiskDashboardCreation) {
    std::string output_path = output_dir_ + "risk_dashboard.html";

    auto result = create_risk_dashboard(sample_returns_, sample_metrics_, output_path);
    EXPECT_TRUE(result.is_ok());

    // Check that file was created
    EXPECT_TRUE(std::filesystem::exists(output_path));

    // Check file content
    std::ifstream file(output_path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    EXPECT_TRUE(content.find("Risk Analysis Dashboard") != std::string::npos);
    EXPECT_TRUE(content.find("SHARPE RATIO") != std::string::npos);
    EXPECT_TRUE(content.find("MAX DRAWDOWN") != std::string::npos);
    EXPECT_TRUE(content.find("VALUE AT RISK") != std::string::npos);
    EXPECT_TRUE(content.find("Returns Distribution") != std::string::npos);
    EXPECT_TRUE(content.find("Risk Profile") != std::string::npos);
    EXPECT_TRUE(content.find("Underwater Plot") != std::string::npos);
}

TEST_F(PlotlyEnhancedTest, AttributionWaterfallCreation) {
    std::map<std::string, double> attribution_factors = {
        {   "Asset Selection",  0.025},
        { "Sector Allocation",  0.015},
        {     "Market Timing", -0.008},
        {"Security Selection",  0.012},
        {   "Currency Effect", -0.003}
    };

    auto result = create_attribution_waterfall(attribution_factors);
    EXPECT_TRUE(result.is_ok());

    std::string html = result.value();
    EXPECT_TRUE(html.find("waterfall") != std::string::npos);
    EXPECT_TRUE(html.find("Asset Selection") != std::string::npos);
    EXPECT_TRUE(html.find("Total") != std::string::npos);
}

TEST_F(PlotlyEnhancedTest, InteractivePlotConfigCustomization) {
    PlotlyEngine engine;

    InteractivePlotConfig config;
    config.theme                = "plotly_dark";
    config.enable_rangeslider   = false;
    config.enable_rangeselector = false;
    config.title                = "Custom Chart Title";
    config.xlabel               = "Custom X Label";
    config.ylabel               = "Custom Y Label";
    config.font_family          = "Roboto, sans-serif";
    config.font_size            = 14;

    std::vector<TimeSeries<double>> series = {sample_returns_};
    std::vector<std::string> labels        = {"Test Strategy"};

    auto result = engine.create_time_series_chart(series, labels, config);
    EXPECT_TRUE(result.is_ok());

    std::string html = result.value();
    EXPECT_TRUE(html.find("plotly_dark") != std::string::npos);
    EXPECT_TRUE(html.find("Custom Chart Title") != std::string::npos);
    EXPECT_TRUE(html.find("Roboto") != std::string::npos);
}

TEST_F(PlotlyEnhancedTest, RealTimeChartInitialization) {
    RealTimeChart chart("test-chart");

    auto result = chart.initialize(sample_returns_);
    EXPECT_TRUE(result.is_ok());

    std::string html = result.value();
    EXPECT_TRUE(html.find("test-chart") != std::string::npos);
    EXPECT_TRUE(html.find("plotly") != std::string::npos);
}

TEST_F(PlotlyEnhancedTest, ErrorHandling) {
    PlotlyEngine engine;

    // Test empty data
    std::vector<TimeSeries<double>> empty_series;
    std::vector<std::string> empty_labels;

    auto result = engine.create_time_series_chart(empty_series, empty_labels);
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidInput);

    // Test mismatched labels and series count
    std::vector<TimeSeries<double>> series   = {sample_returns_};
    std::vector<std::string> too_many_labels = {"Label1", "Label2"};

    result = engine.create_time_series_chart(series, too_many_labels);
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidInput);

    // Test empty OHLC data
    OHLCData empty_ohlc;
    auto candlestick_result = engine.create_candlestick_chart(empty_ohlc);
    EXPECT_TRUE(candlestick_result.is_error());
    EXPECT_EQ(candlestick_result.error().code, ErrorCode::InvalidInput);

    // Test empty correlation matrix
    std::vector<std::vector<double>> empty_matrix;
    std::vector<std::string> labels = {"A", "B"};
    auto heatmap_result             = engine.create_correlation_heatmap(empty_matrix, labels);
    EXPECT_TRUE(heatmap_result.is_error());
    EXPECT_EQ(heatmap_result.error().code, ErrorCode::InvalidInput);

    // Test empty attribution factors
    std::map<std::string, double> empty_factors;
    auto waterfall_result = create_attribution_waterfall(empty_factors);
    EXPECT_TRUE(waterfall_result.is_error());
    EXPECT_EQ(waterfall_result.error().code, ErrorCode::InvalidInput);
}

TEST_F(PlotlyEnhancedTest, ThreeDSurfacePlot) {
    PlotlyEngine engine;

    // Create sample 3D surface data
    std::vector<std::vector<double>> z_data;
    std::vector<double> x_data, y_data;

    for (int i = 0; i < 10; ++i) {
        x_data.push_back(i);
        y_data.push_back(i);
    }

    for (int i = 0; i < 10; ++i) {
        std::vector<double> row;
        for (int j = 0; j < 10; ++j) {
            row.push_back(std::sin(i * 0.5) * std::cos(j * 0.5));
        }
        z_data.push_back(row);
    }

    auto result = engine.create_3d_surface(z_data, x_data, y_data);
    EXPECT_TRUE(result.is_ok());

    std::string html = result.value();
    EXPECT_TRUE(html.find("surface") != std::string::npos);
}

TEST_F(PlotlyEnhancedTest, ComplexDashboardIntegration) {
    // Test that we can create multiple charts and combine them
    PlotlyEngine engine;

    // Create multiple chart types
    std::vector<TimeSeries<double>> series = {sample_returns_};
    std::vector<std::string> labels        = {"Strategy"};

    auto line_chart = engine.create_time_series_chart(series, labels);
    EXPECT_TRUE(line_chart.is_ok());

    std::vector<std::vector<double>> correlation_matrix = {
        {1.0, 0.5},
        {0.5, 1.0}
    };
    std::vector<std::string> corr_labels = {"Asset1", "Asset2"};
    auto heatmap                         = engine.create_correlation_heatmap(correlation_matrix, corr_labels);
    EXPECT_TRUE(heatmap.is_ok());

    std::vector<std::string> waterfall_labels = {"Start", "Gain", "Loss", "End"};
    std::vector<double> waterfall_values      = {100, 20, -5, 0};
    auto waterfall                            = engine.create_waterfall_chart(waterfall_labels, waterfall_values);
    EXPECT_TRUE(waterfall.is_ok());

    // All charts should be valid HTML
    EXPECT_TRUE(line_chart.value().find("<!DOCTYPE html>") != std::string::npos);
    EXPECT_TRUE(heatmap.value().find("<!DOCTYPE html>") != std::string::npos);
    EXPECT_TRUE(waterfall.value().find("<!DOCTYPE html>") != std::string::npos);
}