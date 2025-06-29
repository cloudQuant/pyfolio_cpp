#include <iomanip>
#include <iostream>
#include <pyfolio/positions/allocation.h>
#include <pyfolio/positions/holdings.h>
#include <pyfolio/pyfolio.h>
#include <pyfolio/transactions/round_trips.h>
#include <pyfolio/transactions/trading_costs.h>
#include <pyfolio/transactions/transaction.h>

int main() {
    std::cout << "Pyfolio_cpp Transaction Analysis Example\n";
    std::cout << "========================================\n\n";

    try {
        // Create sample transactions
        pyfolio::transactions::TransactionSeries transactions;

        auto base_date = pyfolio::DateTime::parse("2024-01-02").value();

        // Buy AAPL
        transactions.add_transaction(
            {"AAPL", 100, 150.0, base_date, pyfolio::transactions::TransactionType::Buy, "USD", 1.0, 0.01});

        // Buy MSFT
        transactions.add_transaction(
            {"MSFT", 50, 300.0, base_date.add_days(1), pyfolio::transactions::TransactionType::Buy, "USD", 1.5, 0.01});

        // Sell half AAPL
        transactions.add_transaction({"AAPL", -50, 155.0, base_date.add_days(5),
                                      pyfolio::transactions::TransactionType::Sell, "USD", 1.0, 0.01});

        // Buy more AAPL
        transactions.add_transaction(
            {"AAPL", 25, 152.0, base_date.add_days(10), pyfolio::transactions::TransactionType::Buy, "USD", 0.5, 0.01});

        // Sell remaining AAPL
        transactions.add_transaction({"AAPL", -75, 158.0, base_date.add_days(15),
                                      pyfolio::transactions::TransactionType::Sell, "USD", 1.5, 0.01});

        std::cout << "Created " << transactions.size() << " sample transactions\n\n";

        // Basic transaction analysis
        std::cout << "Transaction Summary:\n";
        std::cout << "Total Value Traded: $" << std::fixed << std::setprecision(2) << transactions.total_value()
                  << "\n";
        std::cout << "Total Commissions: $" << transactions.total_commissions() << "\n";
        std::cout << "Total Slippage: $" << transactions.total_slippage() << "\n";

        auto symbols = transactions.get_symbols();
        std::cout << "Symbols traded: ";
        for (const auto& symbol : symbols) {
            std::cout << symbol << " ";
        }
        std::cout << "\n\n";

        // Round trip analysis
        std::cout << "Round Trip Analysis:\n";
        pyfolio::transactions::RoundTripAnalyzer rt_analyzer;
        auto round_trips_result = rt_analyzer.analyze(transactions);

        if (round_trips_result.is_ok()) {
            const auto& round_trips = round_trips_result.value();
            std::cout << "Found " << round_trips.size() << " round trips\n";

            for (size_t i = 0; i < round_trips.size(); ++i) {
                const auto& trip = round_trips[i];
                std::cout << "Round Trip " << (i + 1) << ":\n";
                std::cout << "  Symbol: " << trip.symbol << "\n";
                std::cout << "  Shares: " << trip.shares << "\n";
                std::cout << "  Open Price: $" << trip.open_price << "\n";
                std::cout << "  Close Price: $" << trip.close_price << "\n";
                std::cout << "  Duration: " << trip.duration_days() << " days\n";
                std::cout << "  P&L: $" << std::setprecision(2) << trip.pnl() << "\n";
                std::cout << "  Return: " << std::setprecision(2) << (trip.return_pct() * 100.0) << "%\n";
                std::cout << "  " << (trip.is_win() ? "WIN" : "LOSS") << "\n\n";
            }

            // Round trip statistics
            auto stats_result = pyfolio::transactions::RoundTripStatistics::calculate(round_trips);
            if (stats_result.is_ok()) {
                const auto& stats = stats_result.value();
                std::cout << "Round Trip Statistics:\n";
                std::cout << "  Total Trips: " << stats.total_trips << "\n";
                std::cout << "  Win Rate: " << std::setprecision(1) << (stats.win_rate * 100.0) << "%\n";
                std::cout << "  Average P&L: $" << std::setprecision(2) << stats.average_pnl << "\n";
                std::cout << "  Average Return: " << std::setprecision(2) << (stats.average_return * 100.0) << "%\n";
                std::cout << "  Average Duration: " << std::setprecision(1) << stats.average_duration_days << " days\n";
                std::cout << "  Profit Factor: " << std::setprecision(2) << stats.profit_factor << "\n";
                std::cout << "  Best Trade: $" << std::setprecision(2) << stats.best_trade_pnl << "\n";
                std::cout << "  Worst Trade: $" << std::setprecision(2) << stats.worst_trade_pnl << "\n\n";
            }
        }

        // Create sample price data for holdings analysis
        std::map<pyfolio::Symbol, pyfolio::PriceSeries> price_data;

        // AAPL price series
        std::vector<pyfolio::DateTime> aapl_dates;
        std::vector<pyfolio::Price> aapl_prices;
        for (int i = 0; i < 20; ++i) {
            aapl_dates.push_back(base_date.add_days(i));
            aapl_prices.push_back(150.0 + i * 0.5);  // Trending up
        }
        price_data["AAPL"] = pyfolio::PriceSeries{aapl_dates, aapl_prices, "AAPL"};

        // MSFT price series
        std::vector<pyfolio::DateTime> msft_dates;
        std::vector<pyfolio::Price> msft_prices;
        for (int i = 0; i < 20; ++i) {
            msft_dates.push_back(base_date.add_days(i));
            msft_prices.push_back(300.0 + i * 0.3);  // Trending up slower
        }
        price_data["MSFT"] = pyfolio::PriceSeries{msft_dates, msft_prices, "MSFT"};

        // Build holdings from transactions
        std::cout << "Holdings Analysis:\n";
        auto holdings_result = pyfolio::positions::HoldingsSeries::build_from_transactions(
            transactions, price_data, 10000.0);  // $10k initial cash

        if (holdings_result.is_ok()) {
            const auto& holdings_series = holdings_result.value();
            std::cout << "Built holdings series with " << holdings_series.size() << " snapshots\n";

            // Show final holdings
            if (!holdings_series.empty()) {
                const auto& final_holdings = holdings_series.back();
                std::cout << "\nFinal Portfolio (as of " << final_holdings.timestamp().to_string() << "):\n";
                std::cout << "Cash: $" << std::setprecision(2) << final_holdings.cash_balance() << "\n";
                std::cout << "Total Value: $" << std::setprecision(2) << final_holdings.total_value() << "\n";

                auto metrics = final_holdings.calculate_metrics();
                std::cout << "Gross Exposure: " << std::setprecision(1) << (metrics.gross_exposure * 100.0) << "%\n";
                std::cout << "Net Exposure: " << std::setprecision(1) << (metrics.net_exposure * 100.0) << "%\n";
                std::cout << "Number of Positions: " << metrics.num_positions << "\n";

                // Show individual holdings
                for (const auto& [symbol, holding] : final_holdings.holdings()) {
                    std::cout << "\n" << symbol << ":\n";
                    std::cout << "  Shares: " << holding.shares << "\n";
                    std::cout << "  Avg Cost: $" << std::setprecision(2) << holding.average_cost << "\n";
                    std::cout << "  Current Price: $" << std::setprecision(2) << holding.current_price << "\n";
                    std::cout << "  Market Value: $" << std::setprecision(2) << holding.market_value << "\n";
                    std::cout << "  Weight: " << std::setprecision(1) << (holding.weight * 100.0) << "%\n";
                    std::cout << "  Unrealized P&L: $" << std::setprecision(2) << holding.unrealized_pnl << "\n";
                    std::cout << "  Return: " << std::setprecision(2) << (holding.return_pct() * 100.0) << "%\n";
                }
            }

            // Portfolio value series
            auto portfolio_value_result = holdings_series.portfolio_value_series();
            if (portfolio_value_result.is_ok()) {
                const auto& pv_series = portfolio_value_result.value();
                std::cout << "\nPortfolio Value Evolution:\n";
                std::cout << "Start: $" << std::setprecision(2) << pv_series.front() << "\n";
                std::cout << "End: $" << std::setprecision(2) << pv_series.back() << "\n";
                std::cout << "Total Return: " << std::setprecision(2)
                          << ((pv_series.back() / pv_series.front() - 1.0) * 100.0) << "%\n";
            }
        } else {
            std::cout << "Error building holdings: " << holdings_result.error().to_string() << "\n";
        }

        // Trading cost analysis
        std::cout << "\nTrading Cost Analysis:\n";
        pyfolio::transactions::TradingCostAnalyzer cost_analyzer;

        // Calculate cost ratio
        double portfolio_value = 25000.0;  // Approximate portfolio value
        auto cost_ratio_result = pyfolio::transactions::calculate_cost_ratio(transactions, portfolio_value);
        if (cost_ratio_result.is_ok()) {
            std::cout << "Total Trading Costs as % of Portfolio: " << std::setprecision(3)
                      << (cost_ratio_result.value() * 100.0) << "%\n";
        }

        // Cost breakdown by symbol
        auto costs_by_symbol_result = pyfolio::transactions::analyze_costs_by_symbol(transactions);
        if (costs_by_symbol_result.is_ok()) {
            const auto& costs = costs_by_symbol_result.value();
            std::cout << "\nCosts by Symbol:\n";
            for (const auto& [symbol, cost_breakdown] : costs) {
                std::cout << symbol << ":\n";
                std::cout << "  Commission: $" << std::setprecision(2) << cost_breakdown.commission << "\n";
                std::cout << "  Slippage: $" << std::setprecision(2) << cost_breakdown.slippage << "\n";
                std::cout << "  Total: $" << std::setprecision(2) << cost_breakdown.total_cost << "\n";
            }
        }

        std::cout << "\nTransaction analysis completed successfully!\n";

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}