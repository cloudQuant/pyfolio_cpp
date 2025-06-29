#include <iomanip>
#include <iostream>
#include <pyfolio/analytics/bayesian.h>
#include <pyfolio/analytics/regime_detection.h>
#include <pyfolio/pyfolio.h>
#include <random>

int main() {
    std::cout << "Pyfolio_cpp Bayesian & Regime Analysis Example\n";
    std::cout << "=============================================\n\n";

    try {
        // Generate realistic return data with different regimes
        auto base_date = pyfolio::DateTime::parse("2020-01-01").value();
        std::mt19937 rng(42);  // Fixed seed for reproducibility

        auto generate_regime_returns = [&](double mean, double vol, size_t periods) {
            std::vector<double> returns;
            std::vector<pyfolio::DateTime> dates;

            std::normal_distribution<double> normal_dist(mean / 252.0, vol / std::sqrt(252.0));

            for (size_t i = 0; i < periods; ++i) {
                returns.push_back(normal_dist(rng));
                dates.push_back(base_date.add_days(static_cast<int>(returns.size() - 1)));
            }

            return std::make_pair(dates, returns);
        };

        // Create multi-regime return series
        std::vector<double> portfolio_returns;
        std::vector<double> benchmark_returns;
        std::vector<pyfolio::DateTime> all_dates;

        // Regime 1: Bull market (252 days)
        auto [bull_dates, bull_port] = generate_regime_returns(0.15, 0.18, 252);
        auto [_, bull_bench]         = generate_regime_returns(0.12, 0.16, 252);

        // Regime 2: Crisis period (63 days)
        auto [crisis_dates, crisis_port] = generate_regime_returns(-0.30, 0.45, 63);
        auto [__, crisis_bench]          = generate_regime_returns(-0.25, 0.40, 63);

        // Regime 3: Recovery period (126 days)
        auto [recovery_dates, recovery_port] = generate_regime_returns(0.25, 0.30, 126);
        auto [___, recovery_bench]           = generate_regime_returns(0.20, 0.25, 126);

        // Regime 4: Stable period (189 days)
        auto [stable_dates, stable_port] = generate_regime_returns(0.08, 0.12, 189);
        auto [____, stable_bench]        = generate_regime_returns(0.07, 0.11, 189);

        // Combine all regimes
        portfolio_returns.insert(portfolio_returns.end(), bull_port.begin(), bull_port.end());
        portfolio_returns.insert(portfolio_returns.end(), crisis_port.begin(), crisis_port.end());
        portfolio_returns.insert(portfolio_returns.end(), recovery_port.begin(), recovery_port.end());
        portfolio_returns.insert(portfolio_returns.end(), stable_port.begin(), stable_port.end());

        benchmark_returns.insert(benchmark_returns.end(), bull_bench.begin(), bull_bench.end());
        benchmark_returns.insert(benchmark_returns.end(), crisis_bench.begin(), crisis_bench.end());
        benchmark_returns.insert(benchmark_returns.end(), recovery_bench.begin(), recovery_bench.end());
        benchmark_returns.insert(benchmark_returns.end(), stable_bench.begin(), stable_bench.end());

        all_dates.insert(all_dates.end(), bull_dates.begin(), bull_dates.end());
        all_dates.insert(all_dates.end(), crisis_dates.begin(), crisis_dates.end());
        all_dates.insert(all_dates.end(), recovery_dates.begin(), recovery_dates.end());
        all_dates.insert(all_dates.end(), stable_dates.begin(), stable_dates.end());

        // Update dates to be sequential
        for (size_t i = 0; i < all_dates.size(); ++i) {
            all_dates[i] = base_date.add_days(static_cast<int>(i));
        }

        pyfolio::ReturnSeries portfolio_series{all_dates, portfolio_returns, "Portfolio"};
        pyfolio::ReturnSeries benchmark_series{all_dates, benchmark_returns, "Benchmark"};

        std::cout << "Generated multi-regime return series:\n";
        std::cout << "- Bull Market: 252 days (15% annual return, 18% vol)\n";
        std::cout << "- Crisis Period: 63 days (-30% annual return, 45% vol)\n";
        std::cout << "- Recovery Phase: 126 days (25% annual return, 30% vol)\n";
        std::cout << "- Stable Period: 189 days (8% annual return, 12% vol)\n";
        std::cout << "Total observations: " << portfolio_returns.size() << " days\n\n";

        // ========================================
        // 1. BAYESIAN PERFORMANCE ANALYSIS
        // ========================================

        std::cout << "1. BAYESIAN PERFORMANCE ANALYSIS\n";
        std::cout << "================================\n";

        pyfolio::analytics::BayesianAnalyzer bayesian_analyzer(42, 5000, 500);  // 5000 samples, 500 burn-in

        // Set up priors
        auto alpha_prior = pyfolio::analytics::PriorDistribution::normal(0.0, 0.01);  // Skeptical prior
        auto beta_prior  = pyfolio::analytics::PriorDistribution::normal(1.0, 0.25);  // Weak prior around 1

        auto bayesian_result =
            bayesian_analyzer.analyze_performance(portfolio_series, benchmark_series, alpha_prior, beta_prior, 0.02);

        if (bayesian_result.is_ok()) {
            const auto& result = bayesian_result.value();

            std::cout << "Alpha Analysis:\n";
            std::cout << "  Posterior Mean: " << std::setprecision(4) << std::fixed
                      << (result.alpha_mean * 252.0 * 100.0) << "% (annualized)\n";
            std::cout << "  Posterior Std: " << std::setprecision(4) << std::fixed
                      << (result.alpha_std * std::sqrt(252.0) * 100.0) << "% (annualized)\n";
            std::cout << "  95% Credible Interval: [" << std::setprecision(2) << std::fixed
                      << (result.alpha_credible_lower * 252.0 * 100.0) << "%, "
                      << (result.alpha_credible_upper * 252.0 * 100.0) << "%]\n";
            std::cout << "  Probability α > 0: " << std::setprecision(1) << std::fixed
                      << (result.prob_alpha_positive * 100.0) << "%\n";

            std::cout << "\nBeta Analysis:\n";
            std::cout << "  Posterior Mean: " << std::setprecision(3) << std::fixed << result.beta_mean << "\n";
            std::cout << "  Posterior Std: " << std::setprecision(3) << std::fixed << result.beta_std << "\n";
            std::cout << "  95% Credible Interval: [" << std::setprecision(2) << std::fixed
                      << result.beta_credible_lower << ", " << result.beta_credible_upper << "]\n";
            std::cout << "  Probability β > 1: " << std::setprecision(1) << std::fixed
                      << (result.prob_beta_greater_one * 100.0) << "%\n";

            std::cout << "\nSharpe Ratio Analysis:\n";
            std::cout << "  Posterior Mean: " << std::setprecision(2) << std::fixed
                      << (result.sharpe_mean * std::sqrt(252.0)) << " (annualized)\n";
            std::cout << "  Posterior Std: " << std::setprecision(2) << std::fixed
                      << (result.sharpe_std * std::sqrt(252.0)) << " (annualized)\n";
            std::cout << "  95% Credible Interval: [" << std::setprecision(2) << std::fixed
                      << (result.sharpe_credible_lower * std::sqrt(252.0)) << ", "
                      << (result.sharpe_credible_upper * std::sqrt(252.0)) << "]\n";

            // Performance interpretation
            std::cout << "\nPerformance Assessment:\n";
            if (result.is_alpha_significant(0.95)) {
                std::cout << "  ✅ Alpha is significantly positive (95% confidence)\n";
            } else if (result.prob_alpha_positive > 0.75) {
                std::cout << "  ⚠️  Alpha is likely positive but not highly significant\n";
            } else {
                std::cout << "  ❌ No significant outperformance detected\n";
            }

            if (result.prob_beta_greater_one > 0.8) {
                std::cout << "  📈 Portfolio exhibits higher systematic risk than benchmark\n";
            } else if (result.prob_beta_greater_one < 0.2) {
                std::cout << "  📉 Portfolio exhibits lower systematic risk than benchmark\n";
            } else {
                std::cout << "  ➡️  Portfolio beta is close to benchmark\n";
            }
        }

        // ========================================
        // 2. BAYESIAN VaR ANALYSIS
        // ========================================

        std::cout << "\n\n2. BAYESIAN VaR ANALYSIS\n";
        std::cout << "========================\n";

        auto bayesian_var_result = bayesian_analyzer.bayesian_var(portfolio_series, 0.95);

        if (bayesian_var_result.is_ok()) {
            auto [var_mean, var_std] = bayesian_var_result.value();

            std::cout << "95% VaR with Uncertainty:\n";
            std::cout << "  Point Estimate: " << std::setprecision(2) << std::fixed << (std::abs(var_mean) * 100.0)
                      << "% of portfolio value\n";
            std::cout << "  Estimation Uncertainty: ±" << std::setprecision(2) << std::fixed << (var_std * 100.0)
                      << "%\n";
            std::cout << "  95% Confidence Interval: [" << std::setprecision(2) << std::fixed
                      << (std::abs(var_mean - 1.96 * var_std) * 100.0) << "%, "
                      << (std::abs(var_mean + 1.96 * var_std) * 100.0) << "%]\n";

            if (var_std / std::abs(var_mean) > 0.2) {
                std::cout << "  ⚠️  High uncertainty in VaR estimate - consider more data\n";
            } else {
                std::cout << "  ✅ VaR estimate has reasonable precision\n";
            }
        }

        // ========================================
        // 3. REGIME DETECTION ANALYSIS
        // ========================================

        std::cout << "\n\n3. REGIME DETECTION ANALYSIS\n";
        std::cout << "============================\n";

        pyfolio::analytics::RegimeDetector regime_detector(42, 21, 0.025, 0.002);

        auto regime_result = regime_detector.detect_regimes(portfolio_series);

        if (regime_result.is_ok()) {
            const auto& result = regime_result.value();

            std::cout << "Current Market Regime:\n";
            std::cout << "  Type: "
                      << (result.regime_characteristics.empty() ? "Unknown" : result.regime_characteristics[0].name())
                      << "\n";
            std::cout << "  Confidence: " << std::setprecision(1) << std::fixed
                      << (result.current_regime_confidence * 100.0) << "%\n";
            std::cout << "  Duration: " << result.current_regime_duration << " days\n";

            // Regime statistics
            auto regime_stats = result.get_regime_statistics();
            std::cout << "\nRegime Distribution:\n";
            for (const auto& [regime_type, probability] : regime_stats) {
                std::string regime_name;
                switch (regime_type) {
                    case pyfolio::analytics::RegimeType::Bull:
                        regime_name = "Bull Market";
                        break;
                    case pyfolio::analytics::RegimeType::Bear:
                        regime_name = "Bear Market";
                        break;
                    case pyfolio::analytics::RegimeType::Volatile:
                        regime_name = "High Volatility";
                        break;
                    case pyfolio::analytics::RegimeType::Stable:
                        regime_name = "Low Volatility";
                        break;
                    case pyfolio::analytics::RegimeType::Crisis:
                        regime_name = "Crisis";
                        break;
                    case pyfolio::analytics::RegimeType::Recovery:
                        regime_name = "Recovery";
                        break;
                }
                std::cout << "  " << std::left << std::setw(18) << regime_name << std::right << std::setw(6)
                          << std::setprecision(1) << std::fixed << (probability * 100.0) << "%\n";
            }

            // Recent regime changes
            auto recent_changes = result.get_recent_changes(3);
            if (!recent_changes.empty()) {
                std::cout << "\nRecent Regime Changes:\n";
                for (const auto& [date, regime] : recent_changes) {
                    std::string regime_name;
                    switch (regime) {
                        case pyfolio::analytics::RegimeType::Bull:
                            regime_name = "Bull Market";
                            break;
                        case pyfolio::analytics::RegimeType::Bear:
                            regime_name = "Bear Market";
                            break;
                        case pyfolio::analytics::RegimeType::Volatile:
                            regime_name = "High Volatility";
                            break;
                        case pyfolio::analytics::RegimeType::Stable:
                            regime_name = "Low Volatility";
                            break;
                        case pyfolio::analytics::RegimeType::Crisis:
                            regime_name = "Crisis";
                            break;
                        case pyfolio::analytics::RegimeType::Recovery:
                            regime_name = "Recovery";
                            break;
                    }
                    std::cout << "  " << date.to_string() << " → " << regime_name << "\n";
                }
            }

            // Regime characteristics
            if (!result.regime_characteristics.empty()) {
                std::cout << "\nRegime Characteristics:\n";
                std::cout << std::left << std::setw(18) << "Regime" << std::right << std::setw(12) << "Mean Return"
                          << std::right << std::setw(12) << "Volatility" << std::right << std::setw(12) << "Persistence"
                          << std::right << std::setw(12) << "Risk Level" << "\n";
                std::cout << std::string(66, '-') << "\n";

                for (const auto& char_data : result.regime_characteristics) {
                    std::cout << std::left << std::setw(18) << char_data.name() << std::right << std::setw(11)
                              << std::setprecision(1) << std::fixed << (char_data.mean_return * 252.0 * 100.0) << "%"
                              << std::right << std::setw(11) << std::setprecision(1) << std::fixed
                              << (char_data.volatility * std::sqrt(252.0) * 100.0) << "%" << std::right << std::setw(11)
                              << std::setprecision(0) << std::fixed << char_data.persistence << " days" << std::right
                              << std::setw(11) << char_data.risk_level() << "/5\n";
                }
            }
        }

        // ========================================
        // 4. REGIME-BASED RECOMMENDATIONS
        // ========================================

        std::cout << "\n\n4. REGIME-BASED RECOMMENDATIONS\n";
        std::cout << "===============================\n";

        auto recommendations_result = regime_detector.get_regime_recommendations();
        if (recommendations_result.is_ok()) {
            const auto& recommendations = recommendations_result.value();

            if (regime_result.is_ok()) {
                auto current_regime = regime_result.value().current_regime;
                auto rec_it         = recommendations.find(current_regime);

                if (rec_it != recommendations.end()) {
                    std::cout << "Current Regime Strategy:\n";
                    std::cout << "  " << rec_it->second << "\n\n";
                }
            }

            std::cout << "All Regime Strategies:\n";
            for (const auto& [regime, recommendation] : recommendations) {
                std::string regime_name;
                switch (regime) {
                    case pyfolio::analytics::RegimeType::Bull:
                        regime_name = "Bull Market";
                        break;
                    case pyfolio::analytics::RegimeType::Bear:
                        regime_name = "Bear Market";
                        break;
                    case pyfolio::analytics::RegimeType::Volatile:
                        regime_name = "High Volatility";
                        break;
                    case pyfolio::analytics::RegimeType::Stable:
                        regime_name = "Low Volatility";
                        break;
                    case pyfolio::analytics::RegimeType::Crisis:
                        regime_name = "Crisis";
                        break;
                    case pyfolio::analytics::RegimeType::Recovery:
                        regime_name = "Recovery";
                        break;
                }
                std::cout << "  " << std::left << std::setw(18) << regime_name << ": " << recommendation << "\n";
            }
        }

        // ========================================
        // 5. BAYESIAN FORECASTING
        // ========================================

        std::cout << "\n\n5. BAYESIAN FORECASTING\n";
        std::cout << "=======================\n";

        auto volatility_prior = pyfolio::analytics::PriorDistribution::normal(0.15, 0.05);
        auto forecast_result  = bayesian_analyzer.forecast_returns(portfolio_series, 21, volatility_prior);

        if (forecast_result.is_ok()) {
            const auto& result = forecast_result.value();

            std::cout << "21-Day Return Forecast:\n";
            std::cout << "  Model Confidence: " << std::setprecision(1) << std::fixed
                      << (result.model_confidence * 100.0) << "%\n";

            // Show forecasts for key horizons
            std::vector<size_t> horizons    = {0, 4, 9, 20};  // 1, 5, 10, 21 days
            std::vector<std::string> labels = {"1-day", "5-day", "10-day", "21-day"};

            std::cout << "\nForecasts by Horizon:\n";
            std::cout << std::left << std::setw(10) << "Horizon" << std::right << std::setw(12) << "Point Est."
                      << std::right << std::setw(18) << "95% Pred. Interval" << std::right << std::setw(12)
                      << "Uncertainty" << "\n";
            std::cout << std::string(54, '-') << "\n";

            for (size_t i = 0; i < horizons.size(); ++i) {
                size_t h = horizons[i];
                if (h < result.return_forecasts.size()) {
                    std::cout << std::left << std::setw(10) << labels[i] << std::right << std::setw(11)
                              << std::setprecision(2) << std::fixed << (result.get_forecast(h) * 100.0) << "%"
                              << std::right << std::setw(8) << std::setprecision(2) << std::fixed
                              << (result.forecast_lower_95[h] * 100.0) << "%"
                              << " to " << std::setw(6) << std::setprecision(2) << std::fixed
                              << (result.forecast_upper_95[h] * 100.0) << "%" << std::right << std::setw(11)
                              << std::setprecision(2) << std::fixed << (result.forecast_volatility[h] * 100.0) << "%\n";
                }
            }

            // Forecast quality assessment
            double avg_uncertainty = 0.0;
            for (size_t h = 0; h < std::min(size_t(5), result.forecast_volatility.size()); ++h) {
                avg_uncertainty += result.forecast_volatility[h];
            }
            avg_uncertainty /= std::min(size_t(5), result.forecast_volatility.size());

            std::cout << "\nForecast Quality Assessment:\n";
            if (result.model_confidence > 0.8) {
                std::cout << "  ✅ High confidence forecasts\n";
            } else if (result.model_confidence > 0.6) {
                std::cout << "  ⚠️  Moderate confidence forecasts\n";
            } else {
                std::cout << "  ❌ Low confidence forecasts - high uncertainty\n";
            }

            if (avg_uncertainty < 0.02) {
                std::cout << "  📊 Low forecast uncertainty\n";
            } else if (avg_uncertainty < 0.04) {
                std::cout << "  📊 Moderate forecast uncertainty\n";
            } else {
                std::cout << "  📊 High forecast uncertainty\n";
            }
        }

        // ========================================
        // 6. INTEGRATED ANALYSIS SUMMARY
        // ========================================

        std::cout << "\n\n6. INTEGRATED ANALYSIS SUMMARY\n";
        std::cout << "==============================\n";

        std::cout << "Key Insights:\n";

        if (bayesian_result.is_ok()) {
            const auto& bayes_result = bayesian_result.value();
            std::cout << "• Alpha generation: ";
            if (bayes_result.prob_alpha_positive > 0.9) {
                std::cout << "Strong evidence of skill (α > 0 with " << std::setprecision(0) << std::fixed
                          << (bayes_result.prob_alpha_positive * 100.0) << "% confidence)\n";
            } else if (bayes_result.prob_alpha_positive > 0.7) {
                std::cout << "Moderate evidence of skill\n";
            } else {
                std::cout << "Limited evidence of skill\n";
            }

            std::cout << "• Risk profile: ";
            if (bayes_result.beta_mean > 1.2) {
                std::cout << "High beta strategy (β = " << std::setprecision(2) << std::fixed << bayes_result.beta_mean
                          << ")\n";
            } else if (bayes_result.beta_mean < 0.8) {
                std::cout << "Low beta strategy (β = " << std::setprecision(2) << std::fixed << bayes_result.beta_mean
                          << ")\n";
            } else {
                std::cout << "Market-neutral risk profile\n";
            }
        }

        if (regime_result.is_ok()) {
            const auto& regime_res = regime_result.value();
            std::cout << "• Market environment: Currently in "
                      << (regime_res.regime_characteristics.empty() ? "Unknown"
                                                                    : regime_res.regime_characteristics[0].name())
                      << " regime\n";
        }

        if (forecast_result.is_ok()) {
            const auto& forecast_res = forecast_result.value();
            std::cout << "• Short-term outlook: ";
            if (forecast_res.get_forecast(4) > 0.01) {
                std::cout << "Positive expected returns over next 5 days\n";
            } else if (forecast_res.get_forecast(4) < -0.01) {
                std::cout << "Negative expected returns over next 5 days\n";
            } else {
                std::cout << "Neutral expected returns over next 5 days\n";
            }
        }

        std::cout << "\nRecommendations:\n";
        std::cout << "• Use Bayesian uncertainty in position sizing decisions\n";
        std::cout << "• Monitor regime changes for tactical allocation adjustments\n";
        std::cout << "• Consider forecast confidence in risk management\n";
        std::cout << "• Regularly update priors as new data becomes available\n";

        std::cout << "\nBayesian and regime analysis completed successfully!\n";

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}