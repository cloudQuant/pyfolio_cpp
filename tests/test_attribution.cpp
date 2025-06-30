#include <gtest/gtest.h>
#include <pyfolio/attribution/attribution.h>

using namespace pyfolio;
using namespace pyfolio::attribution;

class AttributionTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Create sample factor exposures
        portfolio_exposures = pyfolio::attribution::FactorExposures{
            1.2,   // market_beta
            0.3,   // size_factor
            -0.1,  // value_factor
            0.2,   // momentum_factor
            0.05,  // quality_factor
            -0.15  // low_volatility_factor
        };

        benchmark_exposures = pyfolio::attribution::FactorExposures{
            1.0,  // market_beta
            0.0,  // size_factor
            0.0,  // value_factor
            0.0,  // momentum_factor
            0.0,  // quality_factor
            0.0   // low_volatility_factor
        };

        // Create sample factor returns
        factor_returns = pyfolio::attribution::FactorReturns{
            0.008,   // market_return
            0.002,   // size_return
            -0.001,  // value_return
            0.003,   // momentum_return
            0.001,   // quality_return
            -0.002   // low_volatility_return
        };

        // Create sample Brinson attribution data
        DateTime base_date = DateTime::parse("2024-01-15").value();

        // Portfolio weights
        portfolio_weights = {
            {"Technology", 0.40},
            {"Healthcare", 0.25},
            {"Financials", 0.20},
            {  "Consumer", 0.15}
        };

        // Benchmark weights
        benchmark_weights = {
            {"Technology", 0.30},
            {"Healthcare", 0.25},
            {"Financials", 0.25},
            {  "Consumer", 0.20}
        };

        // Sector returns
        sector_returns = {
            {"Technology",  0.025},
            {"Healthcare",  0.015},
            {"Financials", -0.010},
            {  "Consumer",  0.008}
        };

        benchmark_return = 0.012;  // 1.2% benchmark return
    }

    pyfolio::attribution::FactorExposures portfolio_exposures;
    pyfolio::attribution::FactorExposures benchmark_exposures;
    pyfolio::attribution::FactorReturns factor_returns;

    std::map<std::string, double> portfolio_weights;
    std::map<std::string, double> benchmark_weights;
    std::map<std::string, double> sector_returns;
    double benchmark_return;
};

TEST_F(AttributionTest, FactorAttributionBasic) {
    pyfolio::attribution::AttributionAnalyzer analyzer;

    auto attribution = analyzer.analyze_factor_attribution(portfolio_exposures, benchmark_exposures, factor_returns);
    ASSERT_TRUE(attribution.is_ok());

    auto result = attribution.value();

    // The method returns the excess return (portfolio - benchmark)
    double expected_portfolio = portfolio_exposures.market_beta * factor_returns.market_return +
                                portfolio_exposures.size_factor * factor_returns.size_return +
                                portfolio_exposures.value_factor * factor_returns.value_return +
                                portfolio_exposures.momentum_factor * factor_returns.momentum_return +
                                portfolio_exposures.quality_factor * factor_returns.quality_return +
                                portfolio_exposures.low_volatility_factor * factor_returns.low_volatility_return;

    double expected_benchmark = benchmark_exposures.market_beta * factor_returns.market_return +
                                benchmark_exposures.size_factor * factor_returns.size_return +
                                benchmark_exposures.value_factor * factor_returns.value_return +
                                benchmark_exposures.momentum_factor * factor_returns.momentum_return +
                                benchmark_exposures.quality_factor * factor_returns.quality_return +
                                benchmark_exposures.low_volatility_factor * factor_returns.low_volatility_return;

    EXPECT_NEAR(result, expected_portfolio - expected_benchmark, 1e-10);
}

TEST_F(AttributionTest, BrinsonAttribution) {
    pyfolio::attribution::AttributionAnalyzer analyzer;

    auto brinson = analyzer.analyze_sector_attribution(portfolio_weights, benchmark_weights, sector_returns, benchmark_return);
    ASSERT_TRUE(brinson.is_ok());

    auto result = brinson.value();

    // Verify we get sector attribution results
    EXPECT_GT(result.size(), 0);

    // Check that each sector has valid attribution
    for (const auto& sector_attr : result) {
        EXPECT_FALSE(sector_attr.sector.empty());
        // Basic consistency check: total = allocation + selection + interaction
        double calculated_total =
            sector_attr.allocation_effect + sector_attr.selection_effect + sector_attr.interaction_effect;
        EXPECT_NEAR(sector_attr.total_contribution, calculated_total, 1e-10);
    }

    // Calculate expected values manually for validation
    double portfolio_return   = 0.0;
    double allocation_effect  = 0.0;
    double selection_effect   = 0.0;
    double interaction_effect = 0.0;

    for (const auto& [sector, port_weight] : portfolio_weights) {
        double bench_weight = benchmark_weights.at(sector);
        double sector_ret   = sector_returns.at(sector);

        portfolio_return += port_weight * sector_ret;
        allocation_effect += (port_weight - bench_weight) * (benchmark_return);
        selection_effect += bench_weight * (sector_ret - benchmark_return);
        interaction_effect += (port_weight - bench_weight) * (sector_ret - benchmark_return);
    }

    // Sum up the effects from all sectors
    double total_allocation = 0.0, total_selection = 0.0, total_interaction = 0.0;
    for (const auto& sector_attr : result) {
        total_allocation += sector_attr.allocation_effect;
        total_selection += sector_attr.selection_effect;
        total_interaction += sector_attr.interaction_effect;
    }

    EXPECT_NEAR(total_allocation, allocation_effect, 1e-6);
    EXPECT_NEAR(total_selection, selection_effect, 1e-6);
    EXPECT_NEAR(total_interaction, interaction_effect, 1e-6);
}

TEST_F(AttributionTest, AlphaBetaDecomposition) {
    pyfolio::attribution::AlphaBetaAnalysis analyzer;

    // Create sample return series
    std::vector<DateTime> dates;
    std::vector<Return> portfolio_returns;
    std::vector<Return> benchmark_returns;

    DateTime base_date = DateTime::parse("2024-01-01").value();
    for (int i = 0; i < 100; ++i) {
        dates.push_back(base_date.add_days(i));
        portfolio_returns.push_back(0.001 + 0.015 * sin(i * 0.1));  // Simulated returns
        benchmark_returns.push_back(0.0008 + 0.012 * sin(i * 0.1));
    }

    TimeSeries<Return> port_ts(dates, portfolio_returns);
    TimeSeries<Return> bench_ts(dates, benchmark_returns);

    auto alpha_beta = analyzer.calculate(port_ts, bench_ts, 0.02);
    ASSERT_TRUE(alpha_beta.is_ok());

    auto result = alpha_beta.value();

    // Verify results are reasonable
    EXPECT_TRUE(std::isfinite(result.alpha));
    EXPECT_TRUE(std::isfinite(result.beta));
    EXPECT_GE(result.r_squared, 0.0);
    EXPECT_LE(result.r_squared, 1.0 + 1e-10); // Allow for numerical precision
    EXPECT_TRUE(std::isfinite(result.tracking_error));
    EXPECT_GT(result.tracking_error, 0.0);
    EXPECT_TRUE(std::isfinite(result.information_ratio));
}

TEST_F(AttributionTest, SectorAttributionAnalysis) {
    pyfolio::attribution::AttributionAnalyzer analyzer;

    auto sector_attribution = analyzer.analyze_sector_attribution(portfolio_weights, benchmark_weights, sector_returns, benchmark_return);
    ASSERT_TRUE(sector_attribution.is_ok());

    auto result = sector_attribution.value();

    // Should have attribution for each sector
    EXPECT_EQ(result.size(), portfolio_weights.size());

    double total_contribution = 0.0;
    for (const auto& contribution : result) {
        total_contribution += contribution.total_contribution;

        // Each sector should have reasonable values
        EXPECT_TRUE(std::isfinite(contribution.allocation_effect));
        EXPECT_TRUE(std::isfinite(contribution.selection_effect));
        EXPECT_TRUE(std::isfinite(contribution.total_contribution));

        // Total should equal allocation + selection + interaction
        double expected_total =
            contribution.allocation_effect + contribution.selection_effect + contribution.interaction_effect;
        EXPECT_NEAR(contribution.total_contribution, expected_total, 1e-10);
    }

    // Total contribution should be finite
    EXPECT_TRUE(std::isfinite(total_contribution));
}

/*TEST_F(AttributionTest, MultiPeriodAttribution) {
    pyfolio::attribution::AttributionAnalyzer analyzer;

    // Create multiple periods of attribution data
    std::vector<DateTime> dates = {
        DateTime::parse("2024-01-01").value(),
        DateTime::parse("2024-01-02").value(),
        DateTime::parse("2024-01-03").value(),
        DateTime::parse("2024-01-04").value(),
        DateTime::parse("2024-01-05").value()
    };

    std::vector<std::map<std::string, double>> daily_portfolio_weights(5, portfolio_weights);
    std::vector<std::map<std::string, double>> daily_benchmark_weights(5, benchmark_weights);
    std::vector<std::map<std::string, double>> daily_returns(5, sector_returns);
    std::vector<double> daily_benchmark_returns = {0.012, 0.008, -0.005, 0.015, 0.010};

    auto multi_period = analyzer.multi_period_attribution(
        dates, daily_portfolio_weights, daily_benchmark_weights,
        daily_returns, daily_benchmark_returns);
    ASSERT_TRUE(multi_period.is_ok());

    auto result = multi_period.value();

    EXPECT_EQ(result.daily_attributions.size(), dates.size());

    // Verify each daily attribution
    for (const auto& daily_attr : result.daily_attributions) {
        EXPECT_TRUE(std::isfinite(daily_attr.total_active_return));
        EXPECT_TRUE(std::isfinite(daily_attr.allocation_effect));
        EXPECT_TRUE(std::isfinite(daily_attr.selection_effect));
    }

    // Cumulative attribution should be reasonable
    EXPECT_TRUE(std::isfinite(result.cumulative_attribution.total_active_return));
    EXPECT_TRUE(std::isfinite(result.cumulative_attribution.allocation_effect));
    EXPECT_TRUE(std::isfinite(result.cumulative_attribution.selection_effect));
}

*/

/*TEST_F(AttributionTest, RiskAttributionAnalysis) {
    pyfolio::attribution::AttributionAnalyzer analyzer;

    auto risk_attribution = analyzer.risk_attribution(
        portfolio_exposures, benchmark_exposures, factor_returns);
    ASSERT_TRUE(risk_attribution.is_ok());

    auto result = risk_attribution.value();

    // Active exposures should be portfolio minus benchmark
    EXPECT_NEAR(result.active_exposures.market_beta,
                portfolio_exposures.market_beta - benchmark_exposures.market_beta, 1e-10);
    EXPECT_NEAR(result.active_exposures.size_factor,
                portfolio_exposures.size_factor - benchmark_exposures.size_factor, 1e-10);

    // Risk contributions should be reasonable
    EXPECT_TRUE(std::isfinite(result.total_active_risk));
    EXPECT_GE(result.total_active_risk, 0.0);

    for (const auto& [factor, contribution] : result.factor_risk_contributions) {
        EXPECT_TRUE(std::isfinite(contribution));
    }
}

*/

/*TEST_F(AttributionTest, AttributionReporting) {
    pyfolio::attribution::AttributionAnalyzer analyzer;

    // Generate comprehensive attribution report
    auto brinson = analyzer.brinson_attribution(
        portfolio_weights, benchmark_weights, sector_returns, benchmark_return);
    ASSERT_TRUE(brinson.is_ok());

    auto factor_attr = analyzer.factor_attribution(portfolio_exposures, factor_returns);
    ASSERT_TRUE(factor_attr.is_ok());

    auto report = analyzer.generate_attribution_report(brinson.value(), factor_attr.value());
    ASSERT_TRUE(report.is_ok());

    auto result = report.value();

    // Report should contain summary statistics
    EXPECT_TRUE(std::isfinite(result.total_return_attribution));
    EXPECT_TRUE(std::isfinite(result.total_risk_attribution));
    EXPECT_GT(result.attribution_summary.size(), 0);

    // Report should have detailed breakdowns
    EXPECT_GT(result.detailed_breakdown.size(), 0);
}

*/

/*TEST_F(AttributionTest, EmptyDataHandling) {
    pyfolio::attribution::AttributionAnalyzer analyzer;

    // Test with empty weights
    std::map<std::string, double> empty_weights;
    std::map<std::string, double> empty_returns;

    auto empty_brinson = analyzer.brinson_attribution(
        empty_weights, empty_weights, empty_returns, 0.0);
    EXPECT_TRUE(empty_brinson.is_error());

    // Test with mismatched sectors
    std::map<std::string, double> mismatched_weights = {{"Tech", 1.0}};
    std::map<std::string, double> mismatched_returns = {{"Healthcare", 0.01}};

    auto mismatched = analyzer.brinson_attribution(
        mismatched_weights, benchmark_weights, mismatched_returns, benchmark_return);
    EXPECT_TRUE(mismatched.is_error());
}

*/

/*TEST_F(AttributionTest, AttributionConsistency) {
    pyfolio::attribution::AttributionAnalyzer analyzer;

    // Run same attribution multiple times to verify consistency
    auto brinson1 = analyzer.brinson_attribution(
        portfolio_weights, benchmark_weights, sector_returns, benchmark_return);
    auto brinson2 = analyzer.brinson_attribution(
        portfolio_weights, benchmark_weights, sector_returns, benchmark_return);

    ASSERT_TRUE(brinson1.is_ok());
    ASSERT_TRUE(brinson2.is_ok());

    auto result1 = brinson1.value();
    auto result2 = brinson2.value();

    // Results should be identical
    EXPECT_DOUBLE_EQ(result1.total_active_return, result2.total_active_return);
    EXPECT_DOUBLE_EQ(result1.allocation_effect, result2.allocation_effect);
    EXPECT_DOUBLE_EQ(result1.selection_effect, result2.selection_effect);
    EXPECT_DOUBLE_EQ(result1.interaction_effect, result2.interaction_effect);
}

*/

/*TEST_F(AttributionTest, WeightNormalization) {
    pyfolio::attribution::AttributionAnalyzer analyzer;

    // Test with weights that don't sum to 1.0
    std::map<std::string, double> unnormalized_weights = {
        {"Technology", 0.80},  // 200% leverage
        {"Healthcare", 0.50},
        {"Financials", 0.40},
        {"Consumer", 0.30}
    };

    auto brinson = analyzer.brinson_attribution(
        unnormalized_weights, benchmark_weights, sector_returns, benchmark_return);
    ASSERT_TRUE(brinson.is_ok());

    auto result = brinson.value();

    // Should handle unnormalized weights gracefully
    EXPECT_TRUE(std::isfinite(result.total_active_return));
    EXPECT_TRUE(std::isfinite(result.allocation_effect));
    EXPECT_TRUE(std::isfinite(result.selection_effect));
}

*/

/*TEST_F(AttributionTest, NegativeWeightsHandling) {
    pyfolio::attribution::AttributionAnalyzer analyzer;

    // Test with negative weights (short positions)
    std::map<std::string, double> short_weights = {
        {"Technology", 0.60},
        {"Healthcare", 0.30},
        {"Financials", -0.10},  // Short position
        {"Consumer", 0.20}
    };

    auto brinson = analyzer.brinson_attribution(
        short_weights, benchmark_weights, sector_returns, benchmark_return);
    ASSERT_TRUE(brinson.is_ok());

    auto result = brinson.value();

    // Should handle short positions correctly
    EXPECT_TRUE(std::isfinite(result.total_active_return));
    EXPECT_TRUE(std::isfinite(result.allocation_effect));
    EXPECT_TRUE(std::isfinite(result.selection_effect));
}*/