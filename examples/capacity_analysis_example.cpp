#include <iomanip>
#include <iostream>
#include <pyfolio/capacity/capacity.h>
#include <pyfolio/positions/holdings.h>
#include <pyfolio/pyfolio.h>
#include <pyfolio/transactions/transaction.h>

int main() {
    std::cout << "Pyfolio_cpp Capacity Analysis Example\n";
    std::cout << "=====================================\n\n";

    try {
        // Define portfolio composition
        std::map<pyfolio::Symbol, double> target_weights = {
            {"AAPL", 0.25}, // Large cap tech
            {"MSFT", 0.20}, // Large cap tech
            {"TSLA", 0.15}, // Large cap growth (higher volatility)
            { "JPM", 0.15}, // Large cap financial
            {"NVDA", 0.10}, // Large cap tech (high volatility)
            {"SMCI", 0.08}, // Mid cap tech (lower liquidity)
            {"COIN", 0.05}, // Mid cap crypto (high volatility, low liquidity)
            { "GME", 0.02}  // Small cap meme stock (very low liquidity)
        };

        // Current market prices
        std::map<pyfolio::Symbol, double> current_prices = {
            {"AAPL", 185.00},
            {"MSFT", 380.00},
            {"TSLA", 240.00},
            { "JPM", 155.00},
            {"NVDA", 875.00},
            {"SMCI",  45.00},
            {"COIN",  85.00},
            { "GME",  15.00}
        };

        // Create market microstructure data
        std::map<pyfolio::Symbol, pyfolio::capacity::MarketMicrostructure> market_data;

        // Large cap stocks - high liquidity
        market_data["AAPL"] = pyfolio::capacity::create_market_microstructure(
            "AAPL", 50000000, 2800000000000.0, 185.00, 5.0, 0.25);  // 50M ADV, $2.8T market cap, 5bps spread

        market_data["MSFT"] = pyfolio::capacity::create_market_microstructure("MSFT", 30000000, 2800000000000.0, 380.00,
                                                                              5.0, 0.28);  // 30M ADV, $2.8T market cap

        market_data["TSLA"] = pyfolio::capacity::create_market_microstructure(
            "TSLA", 80000000, 760000000000.0, 240.00, 8.0, 0.45);  // 80M ADV, $760B market cap, higher vol

        market_data["JPM"] = pyfolio::capacity::create_market_microstructure("JPM", 15000000, 450000000000.0, 155.00,
                                                                             6.0, 0.22);  // 15M ADV, $450B market cap

        market_data["NVDA"] = pyfolio::capacity::create_market_microstructure(
            "NVDA", 40000000, 2100000000000.0, 875.00, 7.0, 0.40);  // 40M ADV, $2.1T market cap, high vol

        // Mid cap stocks - moderate liquidity
        market_data["SMCI"] = pyfolio::capacity::create_market_microstructure(
            "SMCI", 8000000, 25000000000.0, 45.00, 15.0, 0.60);  // 8M ADV, $25B market cap, wider spreads

        market_data["COIN"] = pyfolio::capacity::create_market_microstructure(
            "COIN", 12000000, 20000000000.0, 85.00, 20.0, 0.70);  // 12M ADV, $20B market cap, crypto volatility

        // Small cap / meme stock - low liquidity
        market_data["GME"] = pyfolio::capacity::create_market_microstructure(
            "GME", 5000000, 4500000000.0, 15.00, 35.0, 0.80);  // 5M ADV, $4.5B market cap, very volatile

        std::cout << "Market Microstructure Data:\n";
        std::cout << "===========================\n";
        std::cout << std::left << std::setw(8) << "Symbol" << std::right << std::setw(12) << "ADV (M)" << std::right
                  << std::setw(15) << "Market Cap ($B)" << std::right << std::setw(12) << "Spread (bps)" << std::right
                  << std::setw(12) << "Volatility" << "\n";
        std::cout << std::string(60, '-') << "\n";

        for (const auto& [symbol, data] : market_data) {
            std::cout << std::left << std::setw(8) << symbol << std::right << std::setw(11) << std::setprecision(1)
                      << std::fixed << (data.average_daily_volume / 1000000.0) << "M" << std::right << std::setw(14)
                      << std::setprecision(0) << std::fixed << (data.market_cap / 1000000000.0) << "B" << std::right
                      << std::setw(11) << std::setprecision(0) << std::fixed << data.typical_spread_bps << "bps"
                      << std::right << std::setw(11) << std::setprecision(1) << std::fixed << (data.volatility * 100.0)
                      << "%\n";
        }
        std::cout << "\n";

        // Set up capacity analyzer with institutional constraints
        pyfolio::capacity::CapacityConstraints constraints;
        constraints.max_adv_participation  = 0.05;   // Max 5% of ADV (conservative)
        constraints.max_single_day_volume  = 0.02;   // Max 2% of single day volume
        constraints.max_market_cap_percent = 0.005;  // Max 0.5% of market cap
        constraints.max_spread_cost_bps    = 25.0;   // Max 25 bps spread cost
        constraints.max_impact_bps         = 50.0;   // Max 50 bps price impact
        constraints.max_trading_days       = 20;     // Max 20 days to complete trade

        pyfolio::capacity::CapacityAnalyzer analyzer(constraints);
        analyzer.set_market_data(market_data);

        std::cout << "Capacity Constraints:\n";
        std::cout << "====================\n";
        std::cout << "Max ADV Participation: " << (constraints.max_adv_participation * 100.0) << "%\n";
        std::cout << "Max Single Day Volume: " << (constraints.max_single_day_volume * 100.0) << "%\n";
        std::cout << "Max Market Cap %: " << (constraints.max_market_cap_percent * 100.0) << "%\n";
        std::cout << "Max Spread Cost: " << constraints.max_spread_cost_bps << " bps\n";
        std::cout << "Max Impact Cost: " << constraints.max_impact_bps << " bps\n";
        std::cout << "Max Trading Days: " << constraints.max_trading_days << " days\n\n";

        // Test different portfolio sizes
        std::vector<double> portfolio_sizes  = {100000000.0, 500000000.0, 1000000000.0, 5000000000.0, 10000000000.0};
        std::vector<std::string> size_labels = {"$100M", "$500M", "$1B", "$5B", "$10B"};

        std::cout << "Portfolio Capacity Analysis:\n";
        std::cout << "============================\n";

        for (size_t i = 0; i < portfolio_sizes.size(); ++i) {
            double portfolio_size  = portfolio_sizes[i];
            std::string size_label = size_labels[i];

            auto capacity_result = analyzer.analyze_portfolio_capacity(target_weights, portfolio_size, current_prices);

            if (capacity_result.is_ok()) {
                const auto& result = capacity_result.value();

                std::cout << "\n" << size_label << " Portfolio Analysis:\n";
                std::cout << std::string(30, '-') << "\n";
                std::cout << "Total Capacity: $" << std::setprecision(1) << std::fixed
                          << (result.total_portfolio_capacity / 1000000000.0) << "B\n";
                std::cout << "Capacity Utilization: " << std::setprecision(1) << std::fixed
                          << (result.capacity_utilization * 100.0) << "%\n";
                std::cout << "Capacity Headroom: " << std::setprecision(1) << std::fixed
                          << (result.capacity_headroom() * 100.0) << "%\n";
                std::cout << "Total Estimated Costs: $" << std::setprecision(0) << std::fixed
                          << result.total_estimated_costs << "\n";
                std::cout << "Average Trading Days: " << std::setprecision(1) << std::fixed
                          << result.average_trading_days << " days\n";

                if (result.is_near_capacity_limit()) {
                    std::cout << "⚠️  WARNING: Portfolio is near capacity limits!\n";
                }

                if (!result.capacity_constrained_securities.empty()) {
                    std::cout << "Capacity Constrained Securities: ";
                    for (size_t j = 0; j < result.capacity_constrained_securities.size(); ++j) {
                        std::cout << result.capacity_constrained_securities[j];
                        if (j < result.capacity_constrained_securities.size() - 1)
                            std::cout << ", ";
                    }
                    std::cout << "\n";
                }
            }
        }

        // Detailed analysis for $1B portfolio
        std::cout << "\n\nDetailed Security Analysis ($1B Portfolio):\n";
        std::cout << "==========================================\n";

        double analysis_portfolio_size = 1000000000.0;  // $1B
        auto detailed_result =
            analyzer.analyze_portfolio_capacity(target_weights, analysis_portfolio_size, current_prices);

        if (detailed_result.is_ok()) {
            const auto& result = detailed_result.value();

            std::cout << std::left << std::setw(8) << "Symbol" << std::right << std::setw(12) << "Target ($M)"
                      << std::right << std::setw(12) << "Max Pos ($M)" << std::right << std::setw(12)
                      << "Max Daily ($M)" << std::right << std::setw(12) << "Est Cost ($K)" << std::right
                      << std::setw(12) << "Trading Days" << std::right << std::setw(15) << "Constraint" << "\n";
            std::cout << std::string(95, '-') << "\n";

            for (const auto& [symbol, weight] : target_weights) {
                double target_dollars = analysis_portfolio_size * weight;
                auto sec_result       = result.get_security_result(symbol);

                std::string constraint_str;
                switch (sec_result.binding_constraint) {
                    case pyfolio::capacity::LiquidityConstraint::ADVMultiple:
                        constraint_str = "ADV Limit";
                        break;
                    case pyfolio::capacity::LiquidityConstraint::MarketCapPercent:
                        constraint_str = "Market Cap";
                        break;
                    case pyfolio::capacity::LiquidityConstraint::VolumePercent:
                        constraint_str = "Volume %";
                        break;
                    default:
                        constraint_str = "None";
                        break;
                }

                std::cout << std::left << std::setw(8) << symbol << std::right << std::setw(11) << std::setprecision(1)
                          << std::fixed << (target_dollars / 1000000.0) << "M" << std::right << std::setw(11)
                          << std::setprecision(1) << std::fixed << (sec_result.max_position_dollars / 1000000.0) << "M"
                          << std::right << std::setw(11) << std::setprecision(1) << std::fixed
                          << (sec_result.max_daily_trade_dollars / 1000000.0) << "M" << std::right << std::setw(11)
                          << std::setprecision(0) << std::fixed << (sec_result.total_trading_cost / 1000.0) << "K"
                          << std::right << std::setw(11) << std::setprecision(0) << std::fixed
                          << sec_result.estimated_trading_days << " days" << std::right << std::setw(15)
                          << constraint_str << "\n";
            }
        }

        // Calculate turnover capacity
        std::cout << "\n\nTurnover Capacity Analysis:\n";
        std::cout << "===========================\n";

        if (detailed_result.is_ok()) {
            const auto& result = detailed_result.value();

            std::vector<double> turnover_rates = {0.5, 1.0, 2.0, 3.0, 5.0};

            std::cout << std::left << std::setw(15) << "Target Turnover" << std::right << std::setw(20)
                      << "Max Supportable AUM" << "\n";
            std::cout << std::string(35, '-') << "\n";

            for (double turnover : turnover_rates) {
                auto capacity_result =
                    pyfolio::capacity::calculate_turnover_capacity(result, turnover, analysis_portfolio_size);

                if (capacity_result.is_ok()) {
                    double max_aum = capacity_result.value();
                    std::cout << std::left << std::setw(14) << (std::to_string(static_cast<int>(turnover * 100)) + "%")
                              << std::right << std::setw(19) << std::setprecision(1) << std::fixed
                              << (max_aum / 1000000000.0) << "B\n";
                }
            }
        }

        // Simulate trading impact
        std::cout << "\n\nTrading Impact Simulation:\n";
        std::cout << "==========================\n";

        // Create sample transactions for TSLA (high volume stock)
        pyfolio::transactions::TransactionSeries transactions;
        auto base_date = pyfolio::DateTime::parse("2024-01-02").value();

        // Simulate aggressive trading in TSLA
        double tsla_price = current_prices["TSLA"];
        for (int day = 0; day < 10; ++day) {
            // Large daily trades
            transactions.add_transaction({"TSLA", 1000000, tsla_price, base_date.add_days(day),
                                          pyfolio::transactions::TransactionType::Buy, "USD", 1000.0});
        }

        double initial_tsla_capacity = 500000000.0;  // $500M initial capacity
        auto impact_result           = analyzer.simulate_trading_impact("TSLA", transactions, initial_tsla_capacity);

        if (impact_result.is_ok()) {
            const auto& impact_values = impact_result.value();

            std::cout << "TSLA Trading Impact Simulation (10 days of 1M share trades):\n";
            std::cout << "Initial Capacity: $" << std::setprecision(0) << std::fixed
                      << (initial_tsla_capacity / 1000000.0) << "M\n";
            std::cout << "Final Capacity: $" << std::setprecision(0) << std::fixed << (impact_values.back() / 1000000.0)
                      << "M\n";
            std::cout << "Capacity Decay: " << std::setprecision(1) << std::fixed
                      << ((1.0 - impact_values.back() / initial_tsla_capacity) * 100.0) << "%\n";
        }

        // Risk warnings and recommendations
        std::cout << "\n\nCapacity Risk Assessment:\n";
        std::cout << "========================\n";

        if (detailed_result.is_ok()) {
            const auto& result = detailed_result.value();

            std::cout << "Risk Level: ";
            if (result.capacity_utilization > 0.8) {
                std::cout << "🔴 HIGH - Portfolio approaching capacity limits\n";
                std::cout << "Recommendations:\n";
                std::cout << "- Consider reducing position sizes in capacity-constrained securities\n";
                std::cout << "- Implement longer trading timelines\n";
                std::cout << "- Diversify into more liquid alternatives\n";
            } else if (result.capacity_utilization > 0.6) {
                std::cout << "🟡 MEDIUM - Monitor capacity constraints closely\n";
                std::cout << "Recommendations:\n";
                std::cout << "- Plan for extended trading periods\n";
                std::cout << "- Monitor market microstructure changes\n";
            } else {
                std::cout << "🟢 LOW - Sufficient capacity headroom\n";
                std::cout << "Portfolio can scale significantly before hitting capacity constraints\n";
            }

            std::cout << "\nKey Metrics:\n";
            std::cout << "- Total trading costs represent " << std::setprecision(2) << std::fixed
                      << (result.total_estimated_costs / analysis_portfolio_size * 10000.0)
                      << " bps of portfolio value\n";
            std::cout << "- Average implementation timeline: " << std::setprecision(1) << std::fixed
                      << result.average_trading_days << " trading days\n";
            std::cout << "- Capacity headroom: " << std::setprecision(1) << std::fixed
                      << (result.capacity_headroom() * 100.0) << "%\n";
        }

        std::cout << "\nCapacity analysis completed successfully!\n";

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}