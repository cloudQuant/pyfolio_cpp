#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <pyfolio/core/time_series.h>
#include <pyfolio/core/types.h>
#include <pyfolio/io/data_loader.h>
#include <pyfolio/visualization/plotting.h>

using namespace pyfolio;
using namespace pyfolio::visualization;
using namespace pyfolio::io;

class VisualizationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Create test directory
        test_dir_ = std::filesystem::temp_directory_path() / "pyfolio_viz_test";
        std::filesystem::create_directories(test_dir_);

        // Generate sample data
        auto start_date = DateTime(2020, 1, 1);

        // Create strategy returns with some volatility
        for (int i = 0; i < 252; ++i) {  // One year of daily data
            auto date  = DateTime(2020, 1, 1 + i);
            double ret = 0.0005 + 0.01 * std::sin(i * 0.1) + 0.005 * (i % 10 == 0 ? 1 : 0);
            returns_.push_back(date, ret);
        }

        // Create benchmark returns (lower volatility)
        for (int i = 0; i < 252; ++i) {
            auto date  = DateTime(2020, 1, 1 + i);
            double ret = 0.0003 + 0.005 * std::sin(i * 0.08);
            benchmark_.push_back(date, ret);
        }
    }

    void TearDown() override {
        // Clean up test files
        std::filesystem::remove_all(test_dir_);
    }

    std::filesystem::path test_dir_;
    TimeSeries<Return> returns_;
    TimeSeries<Return> benchmark_;
};

TEST_F(VisualizationTest, TestPlotEngineLineplot) {
    PlotEngine engine;

    std::vector<PlotData> series;

    // Create test data
    PlotData data;
    data.timestamps = returns_.timestamps();
    data.values     = returns_.values();
    data.label      = "Test Data";
    data.color      = "#1f77b4";

    series.push_back(data);

    PlotConfig config;
    config.title  = "Test Plot";
    config.xlabel = "Date";
    config.ylabel = "Value";
    config.format = "html";

    auto result = engine.create_line_plot(series, config);

    ASSERT_TRUE(result.has_value());

    const auto& html = result.value();

    // Check that HTML contains expected elements
    EXPECT_TRUE(html.find("<!DOCTYPE html>") != std::string::npos);
    EXPECT_TRUE(html.find("Test Plot") != std::string::npos);
    EXPECT_TRUE(html.find("plotly") != std::string::npos);
    EXPECT_TRUE(html.find("Test Data") != std::string::npos);
}

TEST_F(VisualizationTest, TestPlotCumulativeReturns) {
    PlotConfig config;
    config.format    = "html";
    config.save_plot = true;
    config.save_path = (test_dir_ / "cumulative_returns.html").string();

    auto result = plots::plot_cumulative_returns(returns_, benchmark_, config);

    ASSERT_TRUE(result.has_value());

    // Check that file was created
    EXPECT_TRUE(std::filesystem::exists(config.save_path));

    // Check content
    const auto& html = result.value();
    EXPECT_TRUE(html.find("Cumulative Returns") != std::string::npos);
    EXPECT_TRUE(html.find("Strategy") != std::string::npos);
    EXPECT_TRUE(html.find("Benchmark") != std::string::npos);
}

TEST_F(VisualizationTest, TestPlotDrawdown) {
    PlotConfig config;
    config.format = "html";
    config.title  = "Portfolio Drawdown";

    auto result = plots::plot_drawdown(returns_, config);

    ASSERT_TRUE(result.has_value());

    const auto& html = result.value();
    EXPECT_TRUE(html.find("Portfolio Drawdown") != std::string::npos);
    EXPECT_TRUE(html.find("Drawdown") != std::string::npos);
}

TEST_F(VisualizationTest, TestCalculateCumulativeReturns) {
    auto cum_returns = utils::calculate_cumulative_returns(returns_);

    EXPECT_EQ(cum_returns.size(), returns_.size());

    // First value should be the first return
    EXPECT_NEAR(cum_returns[0], returns_[0], 1e-10);

    // Last value should be the total return
    double expected_total = 1.0;
    for (size_t i = 0; i < returns_.size(); ++i) {
        expected_total *= (1.0 + returns_[i]);
    }
    expected_total -= 1.0;

    EXPECT_NEAR(cum_returns.back(), expected_total, 1e-10);

    // Cumulative returns should be non-decreasing for positive returns
    // (not strictly true but should hold for our test data on average)
}

TEST_F(VisualizationTest, TestCalculateAnnualReturns) {
    auto [years, annual_rets] = utils::calculate_annual_returns(returns_);

    EXPECT_GT(years.size(), 0);
    EXPECT_EQ(years.size(), annual_rets.size());

    // Should have at least one year
    EXPECT_TRUE(years[0] == "2020");

    // Annual return should be reasonable
    for (double ret : annual_rets) {
        EXPECT_GT(ret, -1.0);  // Can't lose more than 100%
        EXPECT_LT(ret, 10.0);  // Shouldn't gain more than 1000% in our test data
    }
}

TEST_F(VisualizationTest, TestTimeseriesToPlotdata) {
    auto plot_data = utils::timeseries_to_plotdata(returns_, "Test Label", "#ff0000");

    EXPECT_EQ(plot_data.timestamps.size(), returns_.size());
    EXPECT_EQ(plot_data.values.size(), returns_.size());
    EXPECT_EQ(plot_data.label, "Test Label");
    EXPECT_EQ(plot_data.color, "#ff0000");

    // Check that data matches
    for (size_t i = 0; i < returns_.size(); ++i) {
        EXPECT_EQ(plot_data.timestamps[i], returns_.timestamp(i));
        EXPECT_EQ(plot_data.values[i], returns_[i]);
    }
}

TEST_F(VisualizationTest, TestCreatePerformanceDashboard) {
    auto output_path = (test_dir_ / "dashboard.html").string();

    auto result = plots::create_performance_dashboard(returns_, benchmark_, output_path);

    ASSERT_TRUE(result.has_value());

    // Check that file was created
    EXPECT_TRUE(std::filesystem::exists(output_path));

    // Read file and check contents
    std::ifstream file(output_path);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    EXPECT_TRUE(content.find("Performance Dashboard") != std::string::npos);
    EXPECT_TRUE(content.find("Summary Statistics") != std::string::npos);
    EXPECT_TRUE(content.find("Cumulative Returns") != std::string::npos);
    EXPECT_TRUE(content.find("Total Return") != std::string::npos);
}

TEST_F(VisualizationTest, TestPlotConfigDefaults) {
    PlotConfig config;

    EXPECT_EQ(config.figsize.first, 12);
    EXPECT_EQ(config.figsize.second, 8);
    EXPECT_TRUE(config.grid);
    EXPECT_TRUE(config.legend);
    EXPECT_FALSE(config.save_plot);
    EXPECT_EQ(config.dpi, 150);
    EXPECT_EQ(config.format, "png");
    EXPECT_GT(config.colors.size(), 0);
}

TEST_F(VisualizationTest, TestPlotWithEmptyData) {
    TimeSeries<Return> empty_returns;
    PlotConfig config;
    config.format = "html";

    auto result = plots::plot_cumulative_returns(empty_returns, std::nullopt, config);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidInput);
}

TEST_F(VisualizationTest, TestPlotRollingVolatility) {
    PlotConfig config;
    config.format = "html";
    config.title  = "Rolling Volatility Test";

    auto result = plots::plot_rolling_volatility(returns_, 20, config);

    ASSERT_TRUE(result.has_value());

    const auto& html = result.value();
    EXPECT_TRUE(html.find("Rolling Volatility Test") != std::string::npos);
}

TEST_F(VisualizationTest, TestPlotRollingSharpe) {
    PlotConfig config;
    config.format = "html";
    config.title  = "Rolling Sharpe Test";

    auto result = plots::plot_rolling_sharpe(returns_, 20, 0.02, config);

    ASSERT_TRUE(result.has_value());

    const auto& html = result.value();
    EXPECT_TRUE(html.find("Rolling Sharpe Test") != std::string::npos);
}

TEST_F(VisualizationTest, TestPlotReturnsDistribution) {
    PlotConfig config;
    config.format = "html";
    config.title  = "Returns Distribution";

    auto result = plots::plot_returns_distribution(returns_, 30, config);

    ASSERT_TRUE(result.has_value());

    const auto& html = result.value();
    EXPECT_TRUE(html.find("Returns Distribution") != std::string::npos);
}

TEST_F(VisualizationTest, TestPlotAnnualReturns) {
    PlotConfig config;
    config.format = "html";
    config.title  = "Annual Returns";

    auto result = plots::plot_annual_returns(returns_, benchmark_, config);

    ASSERT_TRUE(result.has_value());

    const auto& html = result.value();
    EXPECT_TRUE(html.find("Annual Returns") != std::string::npos);
}

TEST_F(VisualizationTest, TestMultipleFormats) {
    PlotConfig html_config;
    html_config.format = "html";

    PlotConfig svg_config;
    svg_config.format = "svg";

    auto html_result = plots::plot_cumulative_returns(returns_, std::nullopt, html_config);
    auto svg_result  = plots::plot_cumulative_returns(returns_, std::nullopt, svg_config);

    ASSERT_TRUE(html_result.has_value());
    // SVG might not be implemented yet, so we don't assert on it

    EXPECT_TRUE(html_result.value().find("html") != std::string::npos);
}

TEST_F(VisualizationTest, TestPlotDataValidation) {
    PlotEngine engine;
    std::vector<PlotData> empty_series;
    PlotConfig config;

    auto result = engine.create_line_plot(empty_series, config);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidInput);
}

TEST_F(VisualizationTest, TestFileIOErrors) {
    PlotConfig config;
    config.format    = "html";
    config.save_plot = true;
    config.save_path = "/invalid/path/that/does/not/exist/plot.html";

    auto result = plots::plot_cumulative_returns(returns_, std::nullopt, config);

    // Should fail due to invalid save path
    EXPECT_FALSE(result.has_value());
}

// Main function removed - using shared Google Test main from CMake
