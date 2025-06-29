#include <gtest/gtest.h>
#include <pyfolio/risk/var.h>
// #include <pyfolio/risk/risk_metrics.h>
#include <pyfolio/core/time_series.h>

using namespace pyfolio;

class RiskAnalysisTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Create sample return data with realistic characteristics
        DateTime base_date = DateTime::parse("2024-01-01").value();

        // Generate 252 days of returns (1 year)
        std::mt19937 gen(42);                                   // Fixed seed for reproducibility
        std::normal_distribution<> normal_dist(0.0005, 0.015);  // ~12.6% annual return, ~24% volatility

        for (int i = 0; i < 252; ++i) {
            DateTime current_date = base_date.add_days(i);
            if (current_date.is_weekday()) {
                dates.push_back(current_date);

                // Add some fat tails and skewness to make it more realistic
                double base_return = normal_dist(gen);
                if (i % 20 == 0) {  // Occasional large moves
                    base_return *= 3.0;
                }
                returns.push_back(base_return);
            }
        }

        returns_ts = TimeSeries<Return>(dates, returns);

        // Create portfolio data for portfolio VaR
        symbols           = {"AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"};
        portfolio_weights = {0.3, 0.25, 0.2, 0.15, 0.1};

        // Generate correlated returns for portfolio
        setupPortfolioReturns();
    }

    void setupPortfolioReturns() {
        std::mt19937 gen(43);
        std::normal_distribution<> dist(0.0, 1.0);

        // Create correlation matrix (simplified)
        portfolio_returns.clear();
        for (size_t i = 0; i < symbols.size(); ++i) {
            std::vector<Return> asset_returns;
            for (size_t j = 0; j < dates.size(); ++j) {
                // Simple correlation model
                double corr_factor   = 0.3 * returns[j];  // Market correlation
                double idiosyncratic = 0.02 * dist(gen);  // Asset-specific risk
                asset_returns.push_back(corr_factor + idiosyncratic);
            }
            portfolio_returns.push_back(TimeSeries<Return>(dates, asset_returns));
        }
    }

    std::vector<DateTime> dates;
    std::vector<Return> returns;
    TimeSeries<Return> returns_ts;

    std::vector<std::string> symbols;
    std::vector<double> portfolio_weights;
    std::vector<TimeSeries<Return>> portfolio_returns;
};

TEST_F(RiskAnalysisTest, HistoricalVaR) {
    pyfolio::risk::VaRCalculator var_calc;

    // Test different confidence levels
    std::vector<double> confidence_levels = {0.01, 0.05, 0.10};

    for (double confidence : confidence_levels) {
        auto var_result = var_calc.calculate_historical_var(returns_ts, confidence);
        ASSERT_TRUE(var_result.is_ok()) << "Failed for confidence level: " << confidence;

        double var_value = var_result.value().var_estimate;

        // VaR should be negative (loss)
        EXPECT_LT(var_value, 0.0);

        // VaR should be reasonable (not more than 50% daily loss)
        EXPECT_GT(var_value, -0.5);

        // Higher confidence should give more negative (worse) VaR
        if (confidence == 0.01) {
            auto var_05 = var_calc.calculate_historical_var(returns_ts, 0.05);
            ASSERT_TRUE(var_05.is_ok());
            EXPECT_LT(var_value, var_05.value().var_estimate);  // 1% VaR should be worse than 5% VaR
        }
    }
}

TEST_F(RiskAnalysisTest, ParametricVaR) {
    pyfolio::risk::VaRCalculator var_calc;

    auto var_result = var_calc.calculate_parametric_var(returns_ts, 0.05);
    ASSERT_TRUE(var_result.is_ok());

    double var_value = var_result.value().var_estimate;

    // Basic sanity checks
    EXPECT_LT(var_value, 0.0);
    EXPECT_GT(var_value, -0.5);

    // Compare with historical VaR - should be in similar range
    auto hist_var = var_calc.calculate_historical_var(returns_ts, 0.05);
    ASSERT_TRUE(hist_var.is_ok());

    double hist_value = hist_var.value().var_estimate;

    // Should be within reasonable range of each other
    EXPECT_LT(std::abs(var_value - hist_value), std::abs(hist_value) * 0.5);
}

TEST_F(RiskAnalysisTest, MonteCarloVaR) {
    pyfolio::risk::VaRCalculator var_calc;

    auto var_result = var_calc.calculate_monte_carlo_var(returns_ts, 0.05, pyfolio::risk::VaRHorizon::Daily, 10000);
    ASSERT_TRUE(var_result.is_ok());

    double var_value = var_result.value().var_estimate;

    // Basic sanity checks
    EXPECT_LT(var_value, 0.0);
    EXPECT_GT(var_value, -0.5);

    // Test reproducibility with same seed
    auto var_result2 = var_calc.calculate_monte_carlo_var(returns_ts, 0.05, pyfolio::risk::VaRHorizon::Daily, 10000);
    ASSERT_TRUE(var_result2.is_ok());

    // Should be very close (allowing for small Monte Carlo variation)
    EXPECT_NEAR(var_value, var_result2.value().var_estimate, std::abs(var_value) * 0.1);
}

TEST_F(RiskAnalysisTest, CornishFisherVaR) {
    pyfolio::risk::VaRCalculator var_calc;

    auto var_result = var_calc.calculate_cornish_fisher_var(returns_ts, 0.05);
    ASSERT_TRUE(var_result.is_ok());

    double var_value = var_result.value().var_estimate;

    // Basic sanity checks
    EXPECT_LT(var_value, 0.0);
    EXPECT_GT(var_value, -0.5);

    // Should account for higher moments (skewness/kurtosis)
    auto parametric_var = var_calc.calculate_parametric_var(returns_ts, 0.05);
    ASSERT_TRUE(parametric_var.is_ok());

    // CF-VaR should be different from parametric (unless returns are perfectly normal)
    EXPECT_NE(var_value, parametric_var.value().var_estimate);
}

TEST_F(RiskAnalysisTest, ConditionalVaR) {
    pyfolio::risk::VaRCalculator var_calc;

    auto cvar_result = var_calc.calculate_historical_var(returns_ts, 0.05);
    ASSERT_TRUE(cvar_result.is_ok());

    double cvar_value = cvar_result.value().cvar_estimate;

    // CVaR should be more negative than VaR
    auto var_result = var_calc.calculate_historical_var(returns_ts, 0.05);
    ASSERT_TRUE(var_result.is_ok());

    EXPECT_LT(cvar_value, var_result.value().var_estimate);

    // Basic sanity checks
    EXPECT_LT(cvar_value, 0.0);
    EXPECT_GT(cvar_value, -1.0);
}

/*TEST_F(RiskAnalysisTest, PortfolioVaR) {
    pyfolio::risk::VaRCalculator var_calc;

    auto portfolio_var = var_calc.calculate_marginal_var(portfolio_returns, portfolio_weights, 0.05);
    ASSERT_TRUE(portfolio_var.is_ok());

    auto result = portfolio_var.value();

    // Portfolio VaR should be reasonable
    EXPECT_LT(result.portfolio_var, 0.0);
    EXPECT_GT(result.portfolio_var, -0.5);

    // Component VaRs
    EXPECT_EQ(result.component_vars.size(), symbols.size());

    double total_component_var = 0.0;
    for (const auto& component_var : result.component_vars) {
        total_component_var += component_var;
    }

    // Components should approximately sum to portfolio VaR (diversification effects)
    EXPECT_NEAR(total_component_var, result.portfolio_var, std::abs(result.portfolio_var) * 0.2);

    // Marginal VaRs should be reasonable
    EXPECT_EQ(result.marginal_vars.size(), symbols.size());
    for (const auto& marginal_var : result.marginal_vars) {
        EXPECT_TRUE(std::isfinite(marginal_var));
    }
}*/

/*TEST_F(RiskAnalysisTest, StressTesting) {
    pyfolio::risk::VaRCalculator var_calc;

    // Define stress scenarios
    std::vector<pyfolio::risk::StressTestScenario> scenarios;

    pyfolio::risk::StressTestScenario crash_scenario;
    crash_scenario.name = "Market Crash";
    crash_scenario.shock_factors["AAPL"] = 0.85;  // 15% decline
    crash_scenario.probability = 0.1;
    scenarios.push_back(crash_scenario);

    pyfolio::risk::StressTestScenario volatility_scenario;
    volatility_scenario.name = "High Volatility";
    volatility_scenario.shock_factors["AAPL"] = 1.5;  // 50% increase in volatility effect
    volatility_scenario.probability = 0.2;
    scenarios.push_back(volatility_scenario);

    // Create portfolio data for stress test
    std::map<Symbol, ReturnSeries> asset_returns;
    asset_returns["AAPL"] = returns_ts;
    std::map<Symbol, double> weights;
    weights["AAPL"] = 1.0;

    auto stress_result = var_calc.stress_test(asset_returns, weights, scenarios);
    ASSERT_TRUE(stress_result.is_ok());

    auto results = stress_result.value();
    EXPECT_EQ(results.size(), scenarios.size());

    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        const auto& scenario = scenarios[i];

        EXPECT_EQ(result.scenario_name, scenario.name);
        EXPECT_TRUE(std::isfinite(result.stressed_return));
        EXPECT_TRUE(std::isfinite(result.stressed_volatility));
        EXPECT_LT(result.stressed_var, 0.0);

        // Stressed VaR should be finite
        EXPECT_TRUE(std::isfinite(result.stressed_var));

        // Compare with normal VaR
        auto normal_var = var_calc.calculate_historical_var(returns_ts, 0.05);
        ASSERT_TRUE(normal_var.is_ok());
        // Stress test should generally change VaR
        EXPECT_NE(result.stressed_var, normal_var.value().var_estimate);
    }
}

*/

/*TEST_F(RiskAnalysisTest, RiskMetricsCalculation) {
    RiskMetrics risk_metrics;

    auto metrics = risk_metrics.calculate_comprehensive_risk_metrics(returns_ts);
    ASSERT_TRUE(metrics.is_ok());

    auto result = metrics.value();

    // Validate all risk metrics
    EXPECT_GT(result.volatility, 0.0);
    EXPECT_LT(result.volatility, 1.0); // Less than 100% daily vol

    EXPECT_LT(result.var_95, 0.0);
    EXPECT_LT(result.var_99, 0.0);
    EXPECT_LT(result.var_99, result.var_95); // 99% VaR should be worse

    EXPECT_LT(result.cvar_95, result.var_95);
    EXPECT_LT(result.cvar_99, result.var_99);

    EXPECT_LE(result.max_drawdown, 0.0);
    EXPECT_GE(result.max_drawdown, -1.0);

    // Skewness and kurtosis should be finite
    EXPECT_TRUE(std::isfinite(result.skewness));
    EXPECT_TRUE(std::isfinite(result.kurtosis));

    // Expected shortfall
    EXPECT_LT(result.expected_shortfall, 0.0);
}

*/

/*TEST_F(RiskAnalysisTest, RollingRiskMetrics) {
    RiskMetrics risk_metrics;

    auto rolling_vol = risk_metrics.rolling_volatility(returns_ts, 21); // 21-day rolling
    ASSERT_TRUE(rolling_vol.is_ok());

    auto vol_series = rolling_vol.value();
    EXPECT_EQ(vol_series.size(), returns.size() - 20); // 21-day window

    // All volatilities should be positive
    for (const auto& vol : vol_series) {
        EXPECT_GT(vol, 0.0);
        EXPECT_LT(vol, 1.0); // Reasonable upper bound
    }

    // Rolling VaR
    auto rolling_var = risk_metrics.rolling_var(returns_ts, 21, 0.05);
    ASSERT_TRUE(rolling_var.is_ok());

    auto var_series = rolling_var.value();
    EXPECT_EQ(var_series.size(), returns.size() - 20);

    // All VaRs should be negative
    for (const auto& var : var_series) {
        EXPECT_LT(var, 0.0);
        EXPECT_GT(var, -0.5); // Reasonable lower bound
    }
}

*/

/*TEST_F(RiskAnalysisTest, CorrelationAnalysis) {
    RiskMetrics risk_metrics;

    // Test correlation between first two assets
    auto correlation = risk_metrics.correlation(portfolio_returns[0], portfolio_returns[1]);
    ASSERT_TRUE(correlation.is_ok());

    double corr_value = correlation.value();
    EXPECT_GE(corr_value, -1.0);
    EXPECT_LE(corr_value, 1.0);

    // Test correlation matrix
    auto corr_matrix = risk_metrics.correlation_matrix(portfolio_returns);
    ASSERT_TRUE(corr_matrix.is_ok());

    auto matrix = corr_matrix.value();
    EXPECT_EQ(matrix.size(), portfolio_returns.size());

    // Diagonal should be 1.0
    for (size_t i = 0; i < matrix.size(); ++i) {
        EXPECT_EQ(matrix[i].size(), portfolio_returns.size());
        EXPECT_NEAR(matrix[i][i], 1.0, 1e-10);

        // Matrix should be symmetric
        for (size_t j = 0; j < matrix.size(); ++j) {
            EXPECT_NEAR(matrix[i][j], matrix[j][i], 1e-10);
            EXPECT_GE(matrix[i][j], -1.0);
            EXPECT_LE(matrix[i][j], 1.0);
        }
    }
}

*/

/*TEST_F(RiskAnalysisTest, RiskBudgeting) {
    pyfolio::risk::VaRCalculator var_calc;

    auto risk_budget = var_calc.risk_budgeting_analysis(portfolio_returns, portfolio_weights);
    ASSERT_TRUE(risk_budget.is_ok());

    auto result = risk_budget.value();

    // Risk contributions should sum to total risk
    double total_contribution = 0.0;
    for (const auto& contribution : result.risk_contributions) {
        total_contribution += contribution;
        EXPECT_TRUE(std::isfinite(contribution));
    }

    EXPECT_NEAR(total_contribution, result.total_portfolio_risk, 1e-10);

    // Risk budgets should be positive and sum to 1
    double total_budget = 0.0;
    for (const auto& budget : result.risk_budgets) {
        EXPECT_GE(budget, 0.0);
        EXPECT_LE(budget, 1.0);
        total_budget += budget;
    }

    EXPECT_NEAR(total_budget, 1.0, 1e-10);
}

*/

/*TEST_F(RiskAnalysisTest, EmptyDataHandling) {
    pyfolio::risk::VaRCalculator var_calc;
    TimeSeries<Return> empty_ts;

    // All VaR methods should handle empty data gracefully
    auto hist_var = var_calc.calculate_historical_var(empty_ts, 0.05);
    EXPECT_TRUE(hist_var.is_error());

    auto param_var = var_calc.calculate_parametric_var(empty_ts, 0.05);
    EXPECT_TRUE(param_var.is_error());

    auto mc_var = var_calc.calculate_monte_carlo_var(empty_ts, 0.05, pyfolio::risk::VaRHorizon::Daily, 1000);
    EXPECT_TRUE(mc_var.is_error());

    auto cf_var = var_calc.calculate_cornish_fisher_var(empty_ts, 0.05);
    EXPECT_TRUE(cf_var.is_error());
}

*/

/*TEST_F(RiskAnalysisTest, EdgeCaseConfidenceLevels) {
    pyfolio::risk::VaRCalculator var_calc;

    // Test extreme confidence levels
    auto var_001 = var_calc.calculate_historical_var(returns_ts, 0.001); // 99.9%
    EXPECT_TRUE(var_001.is_ok());

    auto var_099 = var_calc.calculate_historical_var(returns_ts, 0.99);   // 1%
    EXPECT_TRUE(var_099.is_ok());

    // 99.9% VaR should be much worse than 1% VaR
    EXPECT_LT(var_001.value(), var_099.value());

    // Test invalid confidence levels
    auto invalid_var1 = var_calc.calculate_historical_var(returns_ts, 0.0);
    EXPECT_TRUE(invalid_var1.is_error());

    auto invalid_var2 = var_calc.calculate_historical_var(returns_ts, 1.0);
    EXPECT_TRUE(invalid_var2.is_error());

    auto invalid_var3 = var_calc.calculate_historical_var(returns_ts, -0.1);
    EXPECT_TRUE(invalid_var3.is_error());
}

*/

/*TEST_F(RiskAnalysisTest, ConsistencyBetweenMethods) {
    pyfolio::risk::VaRCalculator var_calc;

    // All VaR methods should give results in similar range for normal data
    auto hist_var = var_calc.calculate_historical_var(returns_ts, 0.05);
    auto param_var = var_calc.calculate_parametric_var(returns_ts, 0.05);
    auto mc_var = var_calc.calculate_monte_carlo_var(returns_ts, 0.05, pyfolio::risk::VaRHorizon::Daily, 50000);
    auto cf_var = var_calc.calculate_cornish_fisher_var(returns_ts, 0.05);

    ASSERT_TRUE(hist_var.is_ok());
    ASSERT_TRUE(param_var.is_ok());
    ASSERT_TRUE(mc_var.is_ok());
    ASSERT_TRUE(cf_var.is_ok());

    double hist_val = hist_var.value();
    double param_val = param_var.value();
    double mc_val = mc_var.value();
    double cf_val = cf_var.value();

    // All should be negative
    EXPECT_LT(hist_val, 0.0);
    EXPECT_LT(param_val, 0.0);
    EXPECT_LT(mc_val, 0.0);
    EXPECT_LT(cf_val, 0.0);

    // They should be within reasonable range of each other
    double avg_var = (hist_val + param_val + mc_val + cf_val) / 4.0;

    EXPECT_LT(std::abs(hist_val - avg_var), std::abs(avg_var) * 0.3);
    EXPECT_LT(std::abs(param_val - avg_var), std::abs(avg_var) * 0.3);
    EXPECT_LT(std::abs(mc_val - avg_var), std::abs(avg_var) * 0.3);
    EXPECT_LT(std::abs(cf_val - avg_var), std::abs(avg_var) * 0.3);
}*/