#include <iostream>
#include <pyfolio/pyfolio.h>
#include <vector>

int main() {
    std::cout << "Pyfolio_cpp Basic Example\n";
    std::cout << "Version: " << pyfolio::VERSION_STRING << "\n\n";

    try {
        // Create sample price data
        std::vector<pyfolio::DateTime> dates;
        std::vector<pyfolio::Price> prices;

        // Generate 100 days of sample data
        auto start_date   = pyfolio::DateTime::parse("2024-01-01").value();
        double base_price = 100.0;

        for (int i = 0; i < 100; ++i) {
            dates.push_back(start_date.add_days(i));

            // Simple random walk simulation
            double daily_return = (std::rand() % 200 - 100) / 10000.0;  // -1% to +1%
            base_price *= (1.0 + daily_return);
            prices.push_back(base_price);
        }

        // Create price series
        pyfolio::PriceSeries price_series{dates, prices, "SAMPLE_STOCK"};
        std::cout << "Created price series with " << price_series.size() << " data points\n";

        // Calculate returns
        auto returns_result = pyfolio::performance::calculate_returns(price_series);
        if (returns_result.is_error()) {
            std::cerr << "Error calculating returns: " << returns_result.error().to_string() << "\n";
            return 1;
        }

        auto returns = returns_result.value();
        std::cout << "Calculated " << returns.size() << " return observations\n";

        // Calculate performance metrics
        auto sharpe_result = pyfolio::performance::sharpe_ratio(returns);
        if (sharpe_result.is_ok()) {
            std::cout << "Sharpe Ratio: " << sharpe_result.value() << "\n";
        } else {
            std::cout << "Failed to calculate Sharpe ratio: " << sharpe_result.error().to_string() << "\n";
        }

        auto sortino_result = pyfolio::performance::sortino_ratio(returns);
        if (sortino_result.is_ok()) {
            std::cout << "Sortino Ratio: " << sortino_result.value() << "\n";
        } else {
            std::cout << "Failed to calculate Sortino ratio: " << sortino_result.error().to_string() << "\n";
        }

        auto max_dd_result = pyfolio::performance::max_drawdown(returns);
        if (max_dd_result.is_ok()) {
            std::cout << "Maximum Drawdown: " << (max_dd_result.value() * 100.0) << "%\n";
        } else {
            std::cout << "Failed to calculate max drawdown: " << max_dd_result.error().to_string() << "\n";
        }

        auto volatility_result = pyfolio::performance::calculate_volatility(returns);
        if (volatility_result.is_ok()) {
            std::cout << "Annualized Volatility: " << (volatility_result.value() * 100.0) << "%\n";
        } else {
            std::cout << "Failed to calculate volatility: " << volatility_result.error().to_string() << "\n";
        }

        // Calculate total return
        auto total_return_result = pyfolio::performance::total_return(returns);
        if (total_return_result.is_ok()) {
            std::cout << "Total Return: " << (total_return_result.value() * 100.0) << "%\n";
        } else {
            std::cout << "Failed to calculate total return: " << total_return_result.error().to_string() << "\n";
        }

        std::cout << "\nExample completed successfully!\n";

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}