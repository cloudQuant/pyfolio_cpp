#include <iomanip>
#include <iostream>
#include <pyfolio/attribution/attribution.h>
#include <pyfolio/positions/holdings.h>
#include <pyfolio/pyfolio.h>
#include <pyfolio/transactions/transaction.h>

int main() {
    std::cout << "Pyfolio_cpp Attribution Analysis Example\n";
    std::cout << "=======================================\n\n";

    try {
        // Create sample transactions for attribution analysis
        pyfolio::transactions::TransactionSeries transactions;

        auto base_date = pyfolio::DateTime::parse("2024-01-02").value();

        // Build a diversified portfolio
        transactions.add_transaction(
            {"AAPL", 100, 150.0, base_date, pyfolio::transactions::TransactionType::Buy, "USD", 1.0});  // Technology
        transactions.add_transaction(
            {"MSFT", 50, 300.0, base_date, pyfolio::transactions::TransactionType::Buy, "USD", 1.5});  // Technology
        transactions.add_transaction({"JPM", 80, 125.0, base_date.add_days(1),
                                      pyfolio::transactions::TransactionType::Buy, "USD", 1.2});  // Financial
        transactions.add_transaction({"JNJ", 60, 160.0, base_date.add_days(1),
                                      pyfolio::transactions::TransactionType::Buy, "USD", 1.0});  // Healthcare
        transactions.add_transaction({"XOM", 75, 100.0, base_date.add_days(2),
                                      pyfolio::transactions::TransactionType::Buy, "USD", 1.1});  // Energy

        std::cout << "Created " << transactions.size() << " sample transactions\n\n";

        // Create sector mapping
        std::map<pyfolio::Symbol, std::string> sector_mapping = {
            {"AAPL", "Technology"},
            {"MSFT", "Technology"},
            { "JPM",  "Financial"},
            { "JNJ", "Healthcare"},
            { "XOM",     "Energy"}
        };

        // Create benchmark weights (equal weight benchmark for simplicity)
        std::map<pyfolio::Symbol, double> benchmark_weights = {
            {"AAPL", 0.20},
            {"MSFT", 0.20},
            { "JPM", 0.20},
            { "JNJ", 0.20},
            { "XOM", 0.20}
        };

        // Create security returns for the period
        std::map<pyfolio::Symbol, double> security_returns = {
            {"AAPL",  0.05}, // 5% return
            {"MSFT",  0.03}, // 3% return
            { "JPM",  0.02}, // 2% return
            { "JNJ",  0.01}, // 1% return
            { "XOM", -0.02}  // -2% return
        };

        // Create price data for holdings
        std::map<pyfolio::Symbol, pyfolio::PriceSeries> price_data;

        for (const auto& [symbol, initial_return] : security_returns) {
            std::vector<pyfolio::DateTime> dates;
            std::vector<pyfolio::Price> prices;

            // Create price series showing the return
            dates.push_back(base_date);
            dates.push_back(base_date.add_days(30));

            double initial_price = 0.0;
            if (symbol == "AAPL")
                initial_price = 150.0;
            else if (symbol == "MSFT")
                initial_price = 300.0;
            else if (symbol == "JPM")
                initial_price = 125.0;
            else if (symbol == "JNJ")
                initial_price = 160.0;
            else if (symbol == "XOM")
                initial_price = 100.0;

            prices.push_back(initial_price);
            prices.push_back(initial_price * (1.0 + initial_return));

            price_data[symbol] = pyfolio::PriceSeries{dates, prices, symbol};
        }

        // Build holdings from transactions
        auto holdings_result = pyfolio::positions::HoldingsSeries::build_from_transactions(
            transactions, price_data, 50000.0);  // $50k initial cash

        if (!holdings_result.is_ok()) {
            std::cerr << "Error building holdings: " << holdings_result.error().to_string() << "\n";
            return 1;
        }

        const auto& holdings_series = holdings_result.value();

        if (holdings_series.size() < 2) {
            std::cerr << "Need at least 2 holdings snapshots for attribution\n";
            return 1;
        }

        // Initialize attribution analyzer
        pyfolio::attribution::BrinsonAttribution attribution_analyzer;
        attribution_analyzer.set_sector_mapping(sector_mapping);

        std::cout << "Attribution Analysis Results:\n";
        std::cout << "============================\n\n";

        // Calculate period attribution
        const auto& start_holdings = holdings_series.front();
        const auto& end_holdings   = holdings_series.back();

        auto attribution_result = attribution_analyzer.calculate_period_attribution(
            start_holdings, end_holdings, benchmark_weights, security_returns);

        if (attribution_result.is_ok()) {
            const auto& attr = attribution_result.value();

            std::cout << "Overall Attribution Results:\n";
            std::cout << "Portfolio Return: " << std::setprecision(2) << std::fixed << (attr.portfolio_return * 100.0)
                      << "%\n";
            std::cout << "Benchmark Return: " << std::setprecision(2) << std::fixed << (attr.benchmark_return * 100.0)
                      << "%\n";
            std::cout << "Active Return: " << std::setprecision(2) << std::fixed << (attr.active_return * 100.0)
                      << "%\n\n";

            std::cout << "Attribution Effects:\n";
            std::cout << "Allocation Effect: " << std::setprecision(2) << std::fixed << (attr.allocation_effect * 100.0)
                      << "%\n";
            std::cout << "Selection Effect: " << std::setprecision(2) << std::fixed << (attr.selection_effect * 100.0)
                      << "%\n";
            std::cout << "Interaction Effect: " << std::setprecision(2) << std::fixed
                      << (attr.interaction_effect * 100.0) << "%\n";
            std::cout << "Total Effect: " << std::setprecision(2) << std::fixed << (attr.total_effect * 100.0)
                      << "%\n\n";

            std::cout << "Attribution Consistency: " << (attr.is_consistent() ? "PASS" : "FAIL") << "\n\n";
        } else {
            std::cerr << "Error calculating attribution: " << attribution_result.error().to_string() << "\n";
        }

        // Calculate sector-level attribution
        auto sector_attribution_result =
            attribution_analyzer.calculate_sector_attribution(start_holdings, benchmark_weights, security_returns);

        if (sector_attribution_result.is_ok()) {
            const auto& sector_results = sector_attribution_result.value();

            std::cout << "Sector Attribution Analysis:\n";
            std::cout << std::left << std::setw(12) << "Sector" << std::right << std::setw(8) << "Port Wt" << std::right
                      << std::setw(9) << "Bench Wt" << std::right << std::setw(9) << "Port Ret" << std::right
                      << std::setw(10) << "Bench Ret" << std::right << std::setw(8) << "Alloc" << std::right
                      << std::setw(8) << "Select" << std::right << std::setw(9) << "Interact" << std::right
                      << std::setw(8) << "Total" << "\n";
            std::cout << std::string(80, '-') << "\n";

            for (const auto& sector : sector_results) {
                std::cout << std::left << std::setw(12) << sector.sector << std::right << std::setw(7)
                          << std::setprecision(1) << std::fixed << (sector.portfolio_weight * 100.0) << "%"
                          << std::right << std::setw(8) << std::setprecision(1) << std::fixed
                          << (sector.benchmark_weight * 100.0) << "%" << std::right << std::setw(8)
                          << std::setprecision(1) << std::fixed << (sector.portfolio_return * 100.0) << "%"
                          << std::right << std::setw(9) << std::setprecision(1) << std::fixed
                          << (sector.benchmark_return * 100.0) << "%" << std::right << std::setw(7)
                          << std::setprecision(2) << std::fixed << (sector.allocation_effect * 100.0) << "%"
                          << std::right << std::setw(7) << std::setprecision(2) << std::fixed
                          << (sector.selection_effect * 100.0) << "%" << std::right << std::setw(8)
                          << std::setprecision(2) << std::fixed << (sector.interaction_effect * 100.0) << "%"
                          << std::right << std::setw(7) << std::setprecision(2) << std::fixed
                          << (sector.total_contribution * 100.0) << "%\n";
            }
            std::cout << "\n";
        } else {
            std::cerr << "Error calculating sector attribution: " << sector_attribution_result.error().to_string()
                      << "\n";
        }

        // Alpha/Beta analysis
        std::cout << "Alpha/Beta Analysis:\n";
        std::cout << "===================\n";

        pyfolio::attribution::AlphaBetaAnalysis alpha_beta_analyzer;

        // Create return series for analysis
        std::vector<double> monthly_port_returns  = {0.02, -0.01, 0.03,  0.01, -0.02, 0.04,
                                                     0.01, 0.02,  -0.01, 0.03, 0.01,  0.02};
        std::vector<double> monthly_bench_returns = {0.015, -0.005, 0.025,  0.005, -0.015, 0.035,
                                                     0.005, 0.015,  -0.005, 0.025, 0.005,  0.015};

        // Create monthly dates for the return series
        std::vector<pyfolio::DateTime> monthly_dates;
        for (int i = 0; i < 12; ++i) {
            monthly_dates.push_back(base_date.add_months(i));
        }

        // Create TimeSeries objects
        pyfolio::ReturnSeries portfolio_returns(monthly_dates, monthly_port_returns, "Portfolio");
        pyfolio::ReturnSeries benchmark_returns(monthly_dates, monthly_bench_returns, "Benchmark");

        auto alpha_beta_result = alpha_beta_analyzer.calculate(portfolio_returns, benchmark_returns,
                                                               0.02 / 12.0);  // 2% annual risk-free rate

        if (alpha_beta_result.is_ok()) {
            const auto& ab_result = alpha_beta_result.value();

            std::cout << "Alpha (monthly): " << std::setprecision(4) << std::fixed << (ab_result.alpha * 100.0)
                      << "%\n";
            std::cout << "Alpha (annualized): " << std::setprecision(2) << std::fixed
                      << (ab_result.alpha * 12.0 * 100.0) << "%\n";
            std::cout << "Beta: " << std::setprecision(3) << std::fixed << ab_result.beta << "\n";
            std::cout << "R-squared: " << std::setprecision(3) << std::fixed << ab_result.r_squared << "\n";
            std::cout << "Tracking Error (monthly): " << std::setprecision(2) << std::fixed
                      << (ab_result.tracking_error * 100.0) << "%\n";
            std::cout << "Tracking Error (annualized): " << std::setprecision(2) << std::fixed
                      << (ab_result.tracking_error * std::sqrt(12.0) * 100.0) << "%\n";
            std::cout << "Information Ratio: " << std::setprecision(3) << std::fixed << ab_result.information_ratio
                      << "\n";
            std::cout << "Systematic Risk: " << std::setprecision(2) << std::fixed
                      << (ab_result.systematic_risk * 100.0) << "%\n";
            std::cout << "Specific Risk: " << std::setprecision(2) << std::fixed << (ab_result.specific_risk * 100.0)
                      << "%\n\n";
        } else {
            std::cerr << "Error calculating alpha/beta: " << alpha_beta_result.error().to_string() << "\n";
        }

        std::cout << "Attribution analysis completed successfully!\n";

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}