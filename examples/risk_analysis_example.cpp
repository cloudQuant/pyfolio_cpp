#include <iomanip>
#include <iostream>
#include <pyfolio/positions/holdings.h>
#include <pyfolio/pyfolio.h>
#include <pyfolio/risk/factor_exposure.h>
#include <pyfolio/risk/var.h>
#include <pyfolio/transactions/transaction.h>
#include <random>

int main() {
    std::cout << "Pyfolio_cpp Risk Analysis Example\n";
    std::cout << "=================================\n\n";

    try {
        // Create sample return data for multiple assets
        auto base_date = pyfolio::DateTime::parse("2024-01-01").value();

        // Generate realistic return series with different risk characteristics
        std::mt19937 rng(42);  // Fixed seed for reproducibility

        auto generate_returns = [&](double mean_return, double volatility, double skew = 0.0, size_t periods = 252) {
            std::vector<double> returns;
            std::vector<pyfolio::DateTime> dates;

            std::normal_distribution<double> normal_dist(mean_return / 252.0, volatility / std::sqrt(252.0));

            for (size_t i = 0; i < periods; ++i) {
                double ret = normal_dist(rng);
                // Add some skewness
                if (skew != 0.0) {
                    ret += skew * std::pow(ret, 3);
                }
                returns.push_back(ret);
                dates.push_back(base_date.add_days(static_cast<int>(i)));
            }

            return std::make_pair(dates, returns);
        };

        // Create asset return series with different risk profiles
        auto [spy_dates, spy_returns] =
            generate_returns(0.10, 0.16, -0.3);  // S&P 500: 10% return, 16% vol, negative skew
        auto [iwm_dates, iwm_returns] = generate_returns(0.12, 0.20, -0.5);  // Small cap: higher return, higher vol
        auto [tlt_dates, tlt_returns] = generate_returns(0.03, 0.08, 0.0);   // Bonds: low return, low vol
        auto [gld_dates, gld_returns] =
            generate_returns(0.05, 0.18, 0.2);  // Gold: moderate return, high vol, positive skew
        auto [vix_dates, vix_returns] = generate_returns(0.00, 0.80, 1.0);  // VIX: high vol, positive skew

        std::map<pyfolio::Symbol, pyfolio::ReturnSeries> asset_returns = {
            {"SPY", pyfolio::ReturnSeries{spy_dates, spy_returns, "SPY"}},
            {"IWM", pyfolio::ReturnSeries{iwm_dates, iwm_returns, "IWM"}},
            {"TLT", pyfolio::ReturnSeries{tlt_dates, tlt_returns, "TLT"}},
            {"GLD", pyfolio::ReturnSeries{gld_dates, gld_returns, "GLD"}},
            {"VIX", pyfolio::ReturnSeries{vix_dates, vix_returns, "VIX"}}
        };

        // Define portfolio weights (balanced portfolio)
        std::map<pyfolio::Symbol, double> portfolio_weights = {
            {"SPY", 0.40},
            {"IWM", 0.20},
            {"TLT", 0.25},
            {"GLD", 0.10},
            {"VIX", 0.05}
        };

        std::cout << "Portfolio Composition:\n";
        for (const auto& [symbol, weight] : portfolio_weights) {
            std::cout << "  " << symbol << ": " << std::setprecision(1) << std::fixed << (weight * 100.0) << "%\n";
        }
        std::cout << "\n";

        // Initialize VaR calculator
        pyfolio::risk::VaRCalculator var_calculator(42);  // Fixed seed

        // Manual portfolio return calculation for demonstration
        std::vector<double> portfolio_returns;
        std::vector<pyfolio::DateTime> portfolio_dates;

        for (size_t i = 0; i < spy_returns.size(); ++i) {
            double port_ret =
                portfolio_weights.at("SPY") * spy_returns[i] + portfolio_weights.at("IWM") * iwm_returns[i] +
                portfolio_weights.at("TLT") * tlt_returns[i] + portfolio_weights.at("GLD") * gld_returns[i] +
                portfolio_weights.at("VIX") * vix_returns[i];
            portfolio_returns.push_back(port_ret);
            portfolio_dates.push_back(spy_dates[i]);
        }

        pyfolio::ReturnSeries portfolio_series{portfolio_dates, portfolio_returns, "Portfolio"};

        std::cout << "VaR Analysis Results:\n";
        std::cout << "====================\n";

        // 1. Historical VaR
        std::cout << "\n1. Historical VaR:\n";
        auto hist_var_result = var_calculator.calculate_historical_var(portfolio_series, 0.95);
        if (hist_var_result.is_ok()) {
            const auto& result = hist_var_result.value();
            std::cout << "  95% VaR: " << std::setprecision(2) << std::fixed << result.var_percentage()
                      << "% of portfolio value\n";
            std::cout << "  95% CVaR: " << std::setprecision(2) << std::fixed << result.cvar_percentage()
                      << "% of portfolio value\n";
            std::cout << "  Portfolio Volatility: " << std::setprecision(2) << std::fixed
                      << (result.portfolio_volatility * 100.0) << "% (daily)\n";
            std::cout << "  Skewness: " << std::setprecision(3) << std::fixed << result.skewness << "\n";
            std::cout << "  Kurtosis: " << std::setprecision(3) << std::fixed << result.kurtosis << "\n";
            std::cout << "  Max Drawdown: " << std::setprecision(2) << std::fixed << (result.max_drawdown * 100.0)
                      << "%\n";

            // Scale to different horizons
            auto weekly_var  = result.scale_to_horizon(pyfolio::risk::VaRHorizon::Weekly);
            auto monthly_var = result.scale_to_horizon(pyfolio::risk::VaRHorizon::Monthly);

            std::cout << "  Weekly VaR: " << std::setprecision(2) << std::fixed << weekly_var.var_percentage() << "%\n";
            std::cout << "  Monthly VaR: " << std::setprecision(2) << std::fixed << monthly_var.var_percentage()
                      << "%\n";
        }

        // 2. Parametric VaR
        std::cout << "\n2. Parametric VaR (Normal Distribution):\n";
        auto param_var_result = var_calculator.calculate_parametric_var(portfolio_series, 0.95);
        if (param_var_result.is_ok()) {
            const auto& result = param_var_result.value();
            std::cout << "  95% VaR: " << std::setprecision(2) << std::fixed << result.var_percentage()
                      << "% of portfolio value\n";
            std::cout << "  95% CVaR: " << std::setprecision(2) << std::fixed << result.cvar_percentage()
                      << "% of portfolio value\n";
        }

        // 3. Cornish-Fisher VaR (adjusts for skewness and kurtosis)
        std::cout << "\n3. Cornish-Fisher VaR (Skewness/Kurtosis Adjusted):\n";
        auto cf_var_result = var_calculator.calculate_cornish_fisher_var(portfolio_series, 0.95);
        if (cf_var_result.is_ok()) {
            const auto& result = cf_var_result.value();
            std::cout << "  95% VaR: " << std::setprecision(2) << std::fixed << result.var_percentage()
                      << "% of portfolio value\n";
            std::cout << "  95% CVaR: " << std::setprecision(2) << std::fixed << result.cvar_percentage()
                      << "% of portfolio value\n";
        }

        // 4. Monte Carlo VaR
        std::cout << "\n4. Monte Carlo VaR (10,000 simulations):\n";
        auto mc_var_result =
            var_calculator.calculate_monte_carlo_var(portfolio_series, 0.95, pyfolio::risk::VaRHorizon::Daily, 10000);
        if (mc_var_result.is_ok()) {
            const auto& result = mc_var_result.value();
            std::cout << "  95% VaR: " << std::setprecision(2) << std::fixed << result.var_percentage()
                      << "% of portfolio value\n";
            std::cout << "  95% CVaR: " << std::setprecision(2) << std::fixed << result.cvar_percentage()
                      << "% of portfolio value\n";
        }

        // 5. Marginal VaR Analysis
        std::cout << "\n5. Marginal VaR Analysis:\n";
        auto marginal_var_result = var_calculator.calculate_marginal_var(asset_returns, portfolio_weights, 0.95);
        if (marginal_var_result.is_ok()) {
            const auto& result = marginal_var_result.value();
            std::cout << "  Total Portfolio VaR: " << std::setprecision(2) << std::fixed
                      << std::abs(result.total_var * 100.0) << "%\n\n";

            std::cout << "  Asset Contributions:\n";
            std::cout << "  " << std::left << std::setw(8) << "Asset" << std::right << std::setw(12) << "Marginal VaR"
                      << std::right << std::setw(14) << "Component VaR" << std::right << std::setw(18)
                      << "% Contribution" << "\n";
            std::cout << "  " << std::string(50, '-') << "\n";

            for (const auto& [symbol, weight] : portfolio_weights) {
                std::cout << "  " << std::left << std::setw(8) << symbol << std::right << std::setw(11)
                          << std::setprecision(4) << std::fixed << result.get_marginal_var(symbol) << "%" << std::right
                          << std::setw(13) << std::setprecision(4) << std::fixed << result.get_component_var(symbol)
                          << "%" << std::right << std::setw(17) << std::setprecision(1) << std::fixed
                          << result.get_percentage_contribution(symbol) << "%\n";
            }
        }

        // 6. Stress Testing
        std::cout << "\n6. Stress Testing:\n";
        auto stress_scenarios = pyfolio::risk::stress_scenarios::get_common_scenarios();
        auto stress_results   = var_calculator.stress_test(asset_returns, portfolio_weights, stress_scenarios, 0.95);

        if (stress_results.is_ok()) {
            const auto& results = stress_results.value();

            std::cout << "  Scenario Analysis (95% VaR):\n";
            std::cout << "  " << std::left << std::setw(25) << "Scenario" << std::right << std::setw(12) << "VaR"
                      << std::right << std::setw(12) << "CVaR" << "\n";
            std::cout << "  " << std::string(50, '-') << "\n";

            for (const auto& [scenario_name, var_result] : results) {
                std::cout << "  " << std::left << std::setw(25) << scenario_name << std::right << std::setw(11)
                          << std::setprecision(2) << std::fixed << var_result.var_percentage() << "%" << std::right
                          << std::setw(11) << std::setprecision(2) << std::fixed << var_result.cvar_percentage()
                          << "%\n";
            }
        }

        // 7. VaR Comparison Table
        std::cout << "\n7. VaR Method Comparison (95% Confidence Level):\n";
        std::cout << "  " << std::left << std::setw(20) << "Method" << std::right << std::setw(12) << "VaR"
                  << std::right << std::setw(12) << "CVaR" << "\n";
        std::cout << "  " << std::string(45, '-') << "\n";

        if (hist_var_result.is_ok()) {
            std::cout << "  " << std::left << std::setw(20) << "Historical" << std::right << std::setw(11)
                      << std::setprecision(2) << std::fixed << hist_var_result.value().var_percentage() << "%"
                      << std::right << std::setw(11) << std::setprecision(2) << std::fixed
                      << hist_var_result.value().cvar_percentage() << "%\n";
        }

        if (param_var_result.is_ok()) {
            std::cout << "  " << std::left << std::setw(20) << "Parametric" << std::right << std::setw(11)
                      << std::setprecision(2) << std::fixed << param_var_result.value().var_percentage() << "%"
                      << std::right << std::setw(11) << std::setprecision(2) << std::fixed
                      << param_var_result.value().cvar_percentage() << "%\n";
        }

        if (cf_var_result.is_ok()) {
            std::cout << "  " << std::left << std::setw(20) << "Cornish-Fisher" << std::right << std::setw(11)
                      << std::setprecision(2) << std::fixed << cf_var_result.value().var_percentage() << "%"
                      << std::right << std::setw(11) << std::setprecision(2) << std::fixed
                      << cf_var_result.value().cvar_percentage() << "%\n";
        }

        if (mc_var_result.is_ok()) {
            std::cout << "  " << std::left << std::setw(20) << "Monte Carlo" << std::right << std::setw(11)
                      << std::setprecision(2) << std::fixed << mc_var_result.value().var_percentage() << "%"
                      << std::right << std::setw(11) << std::setprecision(2) << std::fixed
                      << mc_var_result.value().cvar_percentage() << "%\n";
        }

        // 8. Risk Metrics Summary
        if (hist_var_result.is_ok()) {
            const auto& result = hist_var_result.value();
            std::cout << "\n8. Risk Metrics Summary:\n";
            std::cout << "  Daily Statistics:\n";
            std::cout << "    Portfolio Volatility: " << std::setprecision(2) << std::fixed
                      << (result.portfolio_volatility * 100.0) << "% per day\n";
            std::cout << "    Annualized Volatility: " << std::setprecision(2) << std::fixed
                      << (result.portfolio_volatility * std::sqrt(252.0) * 100.0) << "% per year\n";
            std::cout << "    Skewness: " << std::setprecision(3) << std::fixed << result.skewness;
            if (result.skewness < 0)
                std::cout << " (negative tail risk)";
            else if (result.skewness > 0)
                std::cout << " (positive tail risk)";
            std::cout << "\n";
            std::cout << "    Excess Kurtosis: " << std::setprecision(3) << std::fixed << (result.kurtosis - 3.0);
            if (result.kurtosis > 3.0)
                std::cout << " (fat tails)";
            std::cout << "\n";

            std::cout << "\n  Risk Interpretation:\n";
            double daily_var_pct = result.var_percentage();
            if (daily_var_pct < 1.0) {
                std::cout << "    Low risk portfolio\n";
            } else if (daily_var_pct < 2.0) {
                std::cout << "    Moderate risk portfolio\n";
            } else {
                std::cout << "    High risk portfolio\n";
            }

            std::cout << "    There is a 5% chance of losing more than " << std::setprecision(2) << std::fixed
                      << daily_var_pct << "% in a single day\n";
            std::cout << "    When losses exceed VaR, expected loss is " << std::setprecision(2) << std::fixed
                      << result.cvar_percentage() << "% (CVaR)\n";
        }

        std::cout << "\nRisk analysis completed successfully!\n";

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}