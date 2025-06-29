#include <gtest/gtest.h>
#include <pyfolio/analytics/bayesian.h>

using namespace pyfolio;

class BayesianAnalysisTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Create sample return data
        DateTime base_date = DateTime::parse("2024-01-01").value();

        // Generate 252 days of returns (1 year)
        std::mt19937 gen(42);
        std::normal_distribution<> dist(0.0008, 0.015);  // ~20% annual return, ~24% volatility

        for (int i = 0; i < 252; ++i) {
            DateTime current_date = base_date.add_days(i);
            if (current_date.is_weekday()) {
                dates.push_back(current_date);
                returns.push_back(dist(gen));
            }
        }

        returns_ts = TimeSeries<Return>(dates, returns);

        // Create benchmark returns
        std::normal_distribution<> bench_dist(0.0005, 0.012);
        for (size_t i = 0; i < returns.size(); ++i) {
            benchmark_returns.push_back(bench_dist(gen));
        }

        benchmark_ts = TimeSeries<Return>(dates, benchmark_returns);
    }

    std::vector<DateTime> dates;
    std::vector<Return> returns;
    std::vector<Return> benchmark_returns;
    TimeSeries<Return> returns_ts;
    TimeSeries<Return> benchmark_ts;
};

/*TEST_F(BayesianAnalysisTest, BayesianSharpeRatio) {
    pyfolio::analytics::BayesianAnalyzer analyzer;

    auto bayesian_sharpe = analyzer.bayesian_sharpe_ratio(returns_ts, 0.02, 10000, 42);
    ASSERT_TRUE(bayesian_sharpe.is_ok());

    auto result = bayesian_sharpe.value();

    // Check posterior statistics
    EXPECT_TRUE(std::isfinite(result.posterior_mean));
    EXPECT_GT(result.posterior_std, 0.0);
    EXPECT_TRUE(std::isfinite(result.posterior_median));

    // Credible intervals
    EXPECT_LT(result.credible_interval_95.lower, result.posterior_mean);
    EXPECT_GT(result.credible_interval_95.upper, result.posterior_mean);
    EXPECT_LT(result.credible_interval_95.lower, result.credible_interval_95.upper);

    EXPECT_LT(result.credible_interval_68.lower, result.credible_interval_68.upper);
    EXPECT_GE(result.credible_interval_95.lower, result.credible_interval_68.lower);
    EXPECT_LE(result.credible_interval_95.upper, result.credible_interval_68.upper);

    // Probability assessments
    EXPECT_GE(result.prob_positive, 0.0);
    EXPECT_LE(result.prob_positive, 1.0);
    EXPECT_GE(result.prob_greater_than_benchmark, 0.0);
    EXPECT_LE(result.prob_greater_than_benchmark, 1.0);

    // MCMC diagnostics
    EXPECT_GT(result.effective_sample_size, 100); // Should have reasonable ESS
    EXPECT_LT(result.r_hat, 1.1); // Should converge
}

*/

/*TEST_F(BayesianAnalysisTest, BayesianAlphaBeta) {
    pyfolio::analytics::BayesianAnalyzer analyzer;

    auto bayesian_ab = analyzer.bayesian_alpha_beta(returns_ts, benchmark_ts, 0.02, 10000, 42);
    ASSERT_TRUE(bayesian_ab.is_ok());

    auto result = bayesian_ab.value();

    // Alpha posterior
    EXPECT_TRUE(std::isfinite(result.alpha_posterior.mean));
    EXPECT_GT(result.alpha_posterior.std, 0.0);
    EXPECT_LT(result.alpha_posterior.credible_interval_95.lower,
              result.alpha_posterior.credible_interval_95.upper);

    // Beta posterior
    EXPECT_TRUE(std::isfinite(result.beta_posterior.mean));
    EXPECT_GT(result.beta_posterior.std, 0.0);
    EXPECT_LT(result.beta_posterior.credible_interval_95.lower,
              result.beta_posterior.credible_interval_95.upper);

    // Beta should be positive and reasonable for equity strategies
    EXPECT_GT(result.beta_posterior.mean, -2.0);
    EXPECT_LT(result.beta_posterior.mean, 3.0);

    // Correlation should be reasonable
    EXPECT_GE(result.alpha_beta_correlation, -1.0);
    EXPECT_LE(result.alpha_beta_correlation, 1.0);

    // Model fit
    EXPECT_GE(result.r_squared_posterior.mean, 0.0);
    EXPECT_LE(result.r_squared_posterior.mean, 1.0);
    EXPECT_GT(result.residual_volatility, 0.0);
}

TEST_F(BayesianAnalysisTest, BayesianVaR) {
    pyfolio::analytics::BayesianAnalyzer analyzer;

    auto bayesian_var = analyzer.bayesian_var(returns_ts, 0.05);
    ASSERT_TRUE(bayesian_var.is_ok());

    auto result = bayesian_var.value();

    // VaR should be negative
    EXPECT_LT(result.var_posterior.mean, 0.0);
    EXPECT_GT(result.var_posterior.std, 0.0);
    EXPECT_LT(result.var_posterior.credible_interval_95.lower, 0.0);
    EXPECT_LT(result.var_posterior.credible_interval_95.upper, 0.0);

    // CVaR should be more negative than VaR
    EXPECT_LT(result.cvar_posterior.mean, result.var_posterior.mean);
    EXPECT_GT(result.cvar_posterior.std, 0.0);

    // Exceedance probability should be close to confidence level
    EXPECT_GT(result.exceedance_probability, 0.03); // Should be around 5%
    EXPECT_LT(result.exceedance_probability, 0.07);

    // Model parameters
    EXPECT_GT(result.tail_index, 0.0); // Tail index should be positive
    EXPECT_LT(result.tail_index, 10.0); // Reasonable upper bound
}

*/

/*TEST_F(BayesianAnalysisTest, ModelComparison) {
    pyfolio::analytics::BayesianAnalyzer analyzer;

    // Compare different models for return distribution
    std::vector<ReturnDistributionModel> models = {
        ReturnDistributionModel::Normal,
        ReturnDistributionModel::StudentT,
        ReturnDistributionModel::SkewNormal,
        ReturnDistributionModel::GED
    };

    auto comparison = analyzer.model_comparison(returns_ts, models, 5000, 42);
    ASSERT_TRUE(comparison.is_ok());

    auto result = comparison.value();

    EXPECT_EQ(result.model_probabilities.size(), models.size());
    EXPECT_EQ(result.information_criteria.size(), models.size());
    EXPECT_EQ(result.log_likelihoods.size(), models.size());

    // Model probabilities should sum to 1
    double total_prob = 0.0;
    for (const auto& prob : result.model_probabilities) {
        EXPECT_GE(prob, 0.0);
        EXPECT_LE(prob, 1.0);
        total_prob += prob;
    }
    EXPECT_NEAR(total_prob, 1.0, 1e-6);

    // Should identify best model
    EXPECT_GE(result.best_model_index, 0);
    EXPECT_LT(result.best_model_index, static_cast<int>(models.size()));

    // Information criteria should be finite
    for (const auto& ic : result.information_criteria) {
        EXPECT_TRUE(std::isfinite(ic));
    }
}

*/

/*TEST_F(BayesianAnalysisTest, UncertaintyQuantification) {
    pyfolio::analytics::BayesianAnalyzer analyzer;

    auto uncertainty = analyzer.uncertainty_quantification(returns_ts, 10000, 42);
    ASSERT_TRUE(uncertainty.is_ok());

    auto result = uncertainty.value();

    // Parameter uncertainty
    EXPECT_GT(result.parameter_uncertainty.return_uncertainty, 0.0);
    EXPECT_GT(result.parameter_uncertainty.volatility_uncertainty, 0.0);
    EXPECT_TRUE(std::isfinite(result.parameter_uncertainty.skewness_uncertainty));
    EXPECT_TRUE(std::isfinite(result.parameter_uncertainty.kurtosis_uncertainty));

    // Model uncertainty
    EXPECT_GE(result.model_uncertainty.distribution_uncertainty, 0.0);
    EXPECT_LE(result.model_uncertainty.distribution_uncertainty, 1.0);
    EXPECT_GE(result.model_uncertainty.parameter_sensitivity, 0.0);

    // Prediction uncertainty
    EXPECT_GT(result.prediction_uncertainty.one_day_ahead.std, 0.0);
    EXPECT_GT(result.prediction_uncertainty.one_week_ahead.std,
              result.prediction_uncertainty.one_day_ahead.std);
    EXPECT_GT(result.prediction_uncertainty.one_month_ahead.std,
              result.prediction_uncertainty.one_week_ahead.std);

    // All predictions should have reasonable bounds
    for (const auto& pred : {result.prediction_uncertainty.one_day_ahead,
                            result.prediction_uncertainty.one_week_ahead,
                            result.prediction_uncertainty.one_month_ahead}) {
        EXPECT_TRUE(std::isfinite(pred.mean));
        EXPECT_GT(pred.std, 0.0);
        EXPECT_LT(pred.credible_interval_95.lower, pred.credible_interval_95.upper);
    }
}

*/

/*TEST_F(BayesianAnalysisTest, ProbabilisticForecasting) {
    pyfolio::analytics::BayesianAnalyzer analyzer;

    auto forecast = analyzer.probabilistic_forecasting(returns_ts, 21, 10000, 42); // 21-day forecast
    ASSERT_TRUE(forecast.is_ok());

    auto result = forecast.value();

    EXPECT_EQ(result.forecast_horizons.size(), 21);
    EXPECT_EQ(result.return_forecasts.size(), 21);
    EXPECT_EQ(result.volatility_forecasts.size(), 21);
    EXPECT_EQ(result.var_forecasts.size(), 21);

    // Check forecast properties
    for (size_t i = 0; i < result.forecast_horizons.size(); ++i) {
        EXPECT_EQ(result.forecast_horizons[i], i + 1);

        // Return forecasts
        EXPECT_TRUE(std::isfinite(result.return_forecasts[i].mean));
        EXPECT_GT(result.return_forecasts[i].std, 0.0);

        // Volatility forecasts
        EXPECT_GT(result.volatility_forecasts[i].mean, 0.0);
        EXPECT_GT(result.volatility_forecasts[i].std, 0.0);

        // VaR forecasts
        EXPECT_LT(result.var_forecasts[i].mean, 0.0);
        EXPECT_GT(result.var_forecasts[i].std, 0.0);

        // Uncertainty should generally increase with horizon
        if (i > 0) {
            EXPECT_GE(result.return_forecasts[i].std,
                     result.return_forecasts[i-1].std * 0.9); // Allow small decreases
        }
    }

    // Forecast paths
    EXPECT_EQ(result.forecast_paths.size(), 1000); // Default number of paths
    for (const auto& path : result.forecast_paths) {
        EXPECT_EQ(path.size(), 21);
        for (double ret : path) {
            EXPECT_TRUE(std::isfinite(ret));
            EXPECT_GT(ret, -1.0); // No complete losses
            EXPECT_LT(ret, 1.0);  // No 100%+ daily returns
        }
    }
}

*/

/*TEST_F(BayesianAnalysisTest, RobustnessAnalysis) {
    pyfolio::analytics::BayesianAnalyzer analyzer;

    // Test robustness to different priors
    std::vector<PriorSpecification> priors = {
        {PriorType::Uninformative, {}, {}},
        {PriorType::WeaklyInformative, {0.0005, 0.015}, {0.0002, 0.005}}, // Conservative
        {PriorType::StronglyInformative, {0.001, 0.020}, {0.0001, 0.002}}  // Aggressive
    };

    auto robustness = analyzer.robustness_analysis(returns_ts, priors, 5000, 42);
    ASSERT_TRUE(robustness.is_ok());

    auto result = robustness.value();

    EXPECT_EQ(result.prior_sensitivity.size(), priors.size());

    for (const auto& sensitivity : result.prior_sensitivity) {
        EXPECT_TRUE(std::isfinite(sensitivity.sharpe_ratio_mean));
        EXPECT_GT(sensitivity.sharpe_ratio_std, 0.0);
        EXPECT_TRUE(std::isfinite(sensitivity.return_mean));
        EXPECT_GT(sensitivity.volatility_mean, 0.0);
    }

    // Cross-prior statistics
    EXPECT_GT(result.cross_prior_correlation, 0.5); // Should be reasonably correlated
    EXPECT_LE(result.cross_prior_correlation, 1.0);
    EXPECT_GT(result.prior_robustness_score, 0.0);
    EXPECT_LE(result.prior_robustness_score, 1.0);
}

*/

/*TEST_F(BayesianAnalysisTest, MCMCDiagnostics) {
    pyfolio::analytics::BayesianAnalyzer analyzer;

    auto sharpe_result = analyzer.bayesian_sharpe_ratio(returns_ts, 0.02, 20000, 42);
    ASSERT_TRUE(sharpe_result.is_ok());

    auto result = sharpe_result.value();

    // Check MCMC convergence diagnostics
    EXPECT_LT(result.r_hat, 1.1);           // Gelman-Rubin statistic
    EXPECT_GT(result.effective_sample_size, 1000); // Reasonable ESS

    // If we have autocorrelation info
    if (result.autocorrelation.has_value()) {
        EXPECT_GE(result.autocorrelation.value(), 0.0);
        EXPECT_LT(result.autocorrelation.value(), 1.0);
    }

    // Chain should have mixed well
    EXPECT_TRUE(result.chains_converged);
}

*/

/*TEST_F(BayesianAnalysisTest, BayesianPortfolioOptimization) {
    pyfolio::analytics::BayesianAnalyzer analyzer;

    // Create multiple asset returns
    std::vector<TimeSeries<Return>> asset_returns;
    std::vector<std::string> asset_names = {"Asset1", "Asset2", "Asset3"};

    std::mt19937 gen(43);
    for (int asset = 0; asset < 3; ++asset) {
        std::normal_distribution<> dist(0.0005 + asset * 0.0002, 0.015 + asset * 0.002);
        std::vector<Return> asset_rets;

        for (size_t i = 0; i < dates.size(); ++i) {
            asset_rets.push_back(dist(gen));
        }

        asset_returns.emplace_back(dates, asset_rets);
    }

    auto portfolio_opt = analyzer.bayesian_portfolio_optimization(
        asset_returns, asset_names, 0.02, 10000, 42);
    ASSERT_TRUE(portfolio_opt.is_ok());

    auto result = portfolio_opt.value();

    // Optimal weights
    EXPECT_EQ(result.optimal_weights.size(), 3);

    double total_weight = 0.0;
    for (double weight : result.optimal_weights) {
        EXPECT_GE(weight, 0.0); // Assuming long-only
        EXPECT_LE(weight, 1.0);
        total_weight += weight;
    }
    EXPECT_NEAR(total_weight, 1.0, 1e-6);

    // Portfolio metrics
    EXPECT_TRUE(std::isfinite(result.expected_return.mean));
    EXPECT_GT(result.expected_volatility.mean, 0.0);
    EXPECT_TRUE(std::isfinite(result.expected_sharpe.mean));

    // Weight uncertainty
    EXPECT_EQ(result.weight_uncertainty.size(), 3);
    for (const auto& uncertainty : result.weight_uncertainty) {
        EXPECT_GE(uncertainty, 0.0);
        EXPECT_LE(uncertainty, 1.0);
    }
}

*/

/*TEST_F(BayesianAnalysisTest, EmptyDataHandling) {
    pyfolio::analytics::BayesianAnalyzer analyzer;
    TimeSeries<Return> empty_ts;

    // All Bayesian methods should handle empty data gracefully
    auto empty_sharpe = analyzer.bayesian_sharpe_ratio(empty_ts, 0.02, 1000, 42);
    EXPECT_TRUE(empty_sharpe.is_error());

    auto empty_ab = analyzer.bayesian_alpha_beta(empty_ts, empty_ts, 0.02, 1000, 42);
    EXPECT_TRUE(empty_ab.is_error());

    auto empty_var = analyzer.bayesian_var(empty_ts, 0.05, 1000, 42);
    EXPECT_TRUE(empty_var.is_error());
}

*/

/*TEST_F(BayesianAnalysisTest, SmallSampleHandling) {
    pyfolio::analytics::BayesianAnalyzer analyzer;

    // Create very small sample
    std::vector<DateTime> small_dates = {dates[0], dates[1], dates[2]};
    std::vector<Return> small_returns = {returns[0], returns[1], returns[2]};
    TimeSeries<Return> small_ts(small_dates, small_returns);

    // Should handle small samples but with high uncertainty
    auto small_sharpe = analyzer.bayesian_sharpe_ratio(small_ts, 0.02, 1000, 42);
    ASSERT_TRUE(small_sharpe.is_ok());

    auto result = small_sharpe.value();

    // Should have high uncertainty with small sample
    EXPECT_GT(result.posterior_std, 0.1); // High uncertainty
    EXPECT_LT(result.effective_sample_size, 500); // Lower ESS expected
}

*/

/*TEST_F(BayesianAnalysisTest, ReproducibilityWithSeed) {
    pyfolio::analytics::BayesianAnalyzer analyzer;

    // Run same analysis twice with same seed
    auto result1 = analyzer.bayesian_sharpe_ratio(returns_ts, 0.02, 1000, 42);
    auto result2 = analyzer.bayesian_sharpe_ratio(returns_ts, 0.02, 1000, 42);

    ASSERT_TRUE(result1.is_ok());
    ASSERT_TRUE(result2.is_ok());

    // Results should be very similar (allowing for small numerical differences)
    EXPECT_NEAR(result1.value().posterior_mean, result2.value().posterior_mean, 1e-6);
    EXPECT_NEAR(result1.value().posterior_std, result2.value().posterior_std, 1e-6);
}*/