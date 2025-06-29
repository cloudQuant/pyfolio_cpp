#pragma once

#include "../core/dataframe.h"
#include "../core/datetime.h"
#include "../core/error_handling.h"
#include "../core/types.h"
#include "../transactions/transaction.h"
#include <algorithm>
#include <map>

namespace pyfolio::positions {

/**
 * @brief Detailed holding information
 */
struct Holding {
    Symbol symbol;
    Shares shares;
    Price average_cost;
    Price current_price;
    double market_value;
    double cost_basis;
    double unrealized_pnl;
    Weight weight;

    /**
     * @brief Calculate holding metrics
     */
    void calculate_metrics(double total_portfolio_value) {
        market_value   = shares * current_price;
        cost_basis     = shares * average_cost;
        unrealized_pnl = market_value - cost_basis;
        weight         = total_portfolio_value > 0 ? market_value / total_portfolio_value : 0.0;
    }

    /**
     * @brief Return on holding
     */
    double return_pct() const { return cost_basis != 0 ? unrealized_pnl / std::abs(cost_basis) : 0.0; }
};

/**
 * @brief Portfolio holdings at a point in time
 */
class PortfolioHoldings {
  private:
    DateTime timestamp_;
    std::map<Symbol, Holding> holdings_;
    double cash_balance_;
    double total_value_;

    void update_total_value() {
        total_value_ = cash_balance_;
        for (const auto& [symbol, holding] : holdings_) {
            total_value_ += holding.market_value;
        }
    }

  public:
    PortfolioHoldings(const DateTime& timestamp, double cash_balance = 0.0)
        : timestamp_(timestamp), cash_balance_(cash_balance), total_value_(cash_balance) {}

    // Getters
    const DateTime& timestamp() const { return timestamp_; }
    double cash_balance() const { return cash_balance_; }
    double total_value() const { return total_value_; }
    const std::map<Symbol, Holding>& holdings() const { return holdings_; }

    /**
     * @brief Update cash balance
     */
    void set_cash_balance(double cash) {
        cash_balance_ = cash;
        update_total_value();
    }

    /**
     * @brief Add or update a holding
     */
    Result<void> update_holding(const Symbol& symbol, Shares shares, Price average_cost, Price current_price) {
        if (current_price <= 0) {
            return Result<void>::error(ErrorCode::InvalidInput, "Current price must be positive");
        }

        Holding& holding      = holdings_[symbol];
        holding.symbol        = symbol;
        holding.shares        = shares;
        holding.average_cost  = average_cost;
        holding.current_price = current_price;

        // Remove if position is closed
        if (std::abs(shares) < 1e-6) {
            holdings_.erase(symbol);
        } else {
            holding.calculate_metrics(total_value_);
        }

        update_total_value();

        // Recalculate weights after value update
        for (auto& [sym, h] : holdings_) {
            h.calculate_metrics(total_value_);
        }

        return Result<void>::success();
    }

    /**
     * @brief Get holding for a symbol
     */
    Result<Holding> get_holding(const Symbol& symbol) const {
        auto it = holdings_.find(symbol);
        if (it == holdings_.end()) {
            return Result<Holding>::error(ErrorCode::InvalidSymbol, "No holding found for symbol: " + symbol);
        }
        return Result<Holding>::success(it->second);
    }

    /**
     * @brief Calculate portfolio metrics
     */
    struct PortfolioMetrics {
        double gross_exposure;
        double net_exposure;
        double long_exposure;
        double short_exposure;
        double cash_weight;
        int num_positions;
        int num_long_positions;
        int num_short_positions;
    };

    PortfolioMetrics calculate_metrics() const {
        PortfolioMetrics metrics{};

        metrics.cash_weight = total_value_ > 0 ? cash_balance_ / total_value_ : 0.0;

        for (const auto& [symbol, holding] : holdings_) {
            metrics.num_positions++;

            if (holding.shares > 0) {
                metrics.num_long_positions++;
                metrics.long_exposure += holding.market_value;
            } else {
                metrics.num_short_positions++;
                metrics.short_exposure += std::abs(holding.market_value);
            }
        }

        metrics.gross_exposure = metrics.long_exposure + metrics.short_exposure;
        metrics.net_exposure   = metrics.long_exposure - metrics.short_exposure;

        // Normalize by total value
        if (total_value_ > 0) {
            metrics.gross_exposure /= total_value_;
            metrics.net_exposure /= total_value_;
            metrics.long_exposure /= total_value_;
            metrics.short_exposure /= total_value_;
        }

        return metrics;
    }

    /**
     * @brief Get top N holdings by absolute weight
     */
    std::vector<Holding> top_holdings(size_t n) const {
        std::vector<Holding> sorted_holdings;

        for (const auto& [symbol, holding] : holdings_) {
            sorted_holdings.push_back(holding);
        }

        std::partial_sort(sorted_holdings.begin(), sorted_holdings.begin() + std::min(n, sorted_holdings.size()),
                          sorted_holdings.end(),
                          [](const Holding& a, const Holding& b) { return std::abs(a.weight) > std::abs(b.weight); });

        sorted_holdings.resize(std::min(n, sorted_holdings.size()));
        return sorted_holdings;
    }

    /**
     * @brief Convert to DataFrame
     */
    Result<DataFrame> to_dataframe() const {
        if (holdings_.empty()) {
            return Result<DataFrame>::error(ErrorCode::InsufficientData, "No holdings to convert");
        }

        std::vector<DateTime> timestamps;
        std::vector<std::string> symbols;
        std::vector<double> shares;
        std::vector<double> avg_costs;
        std::vector<double> current_prices;
        std::vector<double> market_values;
        std::vector<double> weights;
        std::vector<double> unrealized_pnls;

        for (const auto& [symbol, holding] : holdings_) {
            timestamps.push_back(timestamp_);
            symbols.push_back(symbol);
            shares.push_back(holding.shares);
            avg_costs.push_back(holding.average_cost);
            current_prices.push_back(holding.current_price);
            market_values.push_back(holding.market_value);
            weights.push_back(holding.weight);
            unrealized_pnls.push_back(holding.unrealized_pnl);
        }

        DataFrame df{timestamps};
        df.add_column("symbol", symbols);
        df.add_column("shares", shares);
        df.add_column("avg_cost", avg_costs);
        df.add_column("current_price", current_prices);
        df.add_column("market_value", market_values);
        df.add_column("weight", weights);
        df.add_column("unrealized_pnl", unrealized_pnls);

        return Result<DataFrame>::success(std::move(df));
    }
};

/**
 * @brief Time series of portfolio holdings
 */
class HoldingsSeries {
  private:
    std::vector<PortfolioHoldings> holdings_series_;

    void sort_by_timestamp() {
        std::sort(holdings_series_.begin(), holdings_series_.end(),
                  [](const PortfolioHoldings& a, const PortfolioHoldings& b) { return a.timestamp() < b.timestamp(); });
    }

  public:
    using iterator       = std::vector<PortfolioHoldings>::iterator;
    using const_iterator = std::vector<PortfolioHoldings>::const_iterator;

    // Constructors
    HoldingsSeries() = default;

    explicit HoldingsSeries(std::vector<PortfolioHoldings> holdings) : holdings_series_(std::move(holdings)) {
        sort_by_timestamp();
    }

    // Capacity
    size_t size() const { return holdings_series_.size(); }
    bool empty() const { return holdings_series_.empty(); }

    // Element access
    const PortfolioHoldings& operator[](size_t i) const { return holdings_series_[i]; }
    PortfolioHoldings& operator[](size_t i) { return holdings_series_[i]; }

    const PortfolioHoldings& front() const { return holdings_series_.front(); }
    const PortfolioHoldings& back() const { return holdings_series_.back(); }

    // Iterators
    iterator begin() { return holdings_series_.begin(); }
    const_iterator begin() const { return holdings_series_.begin(); }
    iterator end() { return holdings_series_.end(); }
    const_iterator end() const { return holdings_series_.end(); }

    /**
     * @brief Add holdings snapshot
     */
    void add_holdings(const PortfolioHoldings& holdings) {
        holdings_series_.push_back(holdings);
        sort_by_timestamp();
    }

    /**
     * @brief Get holdings at specific time
     */
    Result<PortfolioHoldings> at_time(const DateTime& timestamp) const {
        auto it = std::lower_bound(holdings_series_.begin(), holdings_series_.end(), timestamp,
                                   [](const PortfolioHoldings& h, const DateTime& t) { return h.timestamp() < t; });

        if (it == holdings_series_.end() || it->timestamp() != timestamp) {
            return Result<PortfolioHoldings>::error(ErrorCode::MissingData, "No holdings found for timestamp");
        }

        return Result<PortfolioHoldings>::success(*it);
    }

    /**
     * @brief Build holdings from transactions
     */
    static Result<HoldingsSeries> build_from_transactions(const transactions::TransactionSeries& txns,
                                                          const std::map<Symbol, PriceSeries>& price_data,
                                                          double initial_cash = 0.0) {
        if (txns.empty()) {
            return Result<HoldingsSeries>::error(ErrorCode::InsufficientData, "No transactions to process");
        }

        HoldingsSeries result;

        // Track positions and cash
        std::map<Symbol, std::pair<Shares, Price>> positions;  // shares, avg_cost
        double cash = initial_cash;

        // Process transactions day by day
        DateTime current_date{txns[0].timestamp().to_date()};
        DateTime last_date{txns.back().timestamp().to_date()};

        size_t txn_idx = 0;

        while (current_date <= last_date) {
            // Process all transactions for current date
            while (txn_idx < txns.size() && DateTime{txns[txn_idx].timestamp().to_date()} == current_date) {
                const auto& txn          = txns[txn_idx];
                auto& [shares, avg_cost] = positions[txn.symbol()];

                // Update position
                if (shares == 0) {
                    // New position
                    shares   = txn.shares();
                    avg_cost = txn.price();
                } else if ((shares > 0 && txn.shares() > 0) || (shares < 0 && txn.shares() < 0)) {
                    // Adding to position
                    double total_cost = shares * avg_cost + txn.shares() * txn.price();
                    shares += txn.shares();
                    avg_cost = shares != 0 ? total_cost / shares : 0.0;
                } else {
                    // Reducing or reversing position
                    shares += txn.shares();
                    if (std::abs(shares) < 1e-6) {
                        shares   = 0;
                        avg_cost = 0;
                    } else if (shares * txn.shares() < 0) {
                        // Position reversed
                        avg_cost = txn.price();
                    }
                }

                // Update cash
                cash += txn.net_cash_flow();

                txn_idx++;
            }

            // Create holdings snapshot
            PortfolioHoldings holdings{current_date, cash};

            // Update all positions with current prices
            for (const auto& [symbol, pos_data] : positions) {
                const auto& [shares, avg_cost] = pos_data;

                if (std::abs(shares) < 1e-6)
                    continue;

                // Get current price
                auto price_it = price_data.find(symbol);
                if (price_it != price_data.end()) {
                    auto price_result = price_it->second.at_time(current_date);
                    if (price_result.is_ok()) {
                        holdings.update_holding(symbol, shares, avg_cost, price_result.value());
                    }
                }
            }

            result.add_holdings(holdings);

            // Move to next business day
            current_date = current_date.next_business_day();
        }

        return Result<HoldingsSeries>::success(std::move(result));
    }

    /**
     * @brief Calculate portfolio value time series
     */
    Result<TimeSeries<double>> portfolio_value_series() const {
        if (empty()) {
            return Result<TimeSeries<double>>::error(ErrorCode::InsufficientData, "No holdings data");
        }

        std::vector<DateTime> timestamps;
        std::vector<double> values;

        for (const auto& holdings : holdings_series_) {
            timestamps.push_back(holdings.timestamp());
            values.push_back(holdings.total_value());
        }

        return Result<TimeSeries<double>>::success(
            TimeSeries<double>{std::move(timestamps), std::move(values), "portfolio_value"});
    }

    /**
     * @brief Calculate exposure time series
     */
    struct ExposureSeries {
        TimeSeries<double> gross_exposure;
        TimeSeries<double> net_exposure;
        TimeSeries<double> long_exposure;
        TimeSeries<double> short_exposure;
    };

    Result<ExposureSeries> exposure_series() const {
        if (empty()) {
            return Result<ExposureSeries>::error(ErrorCode::InsufficientData, "No holdings data");
        }

        std::vector<DateTime> timestamps;
        std::vector<double> gross_exp;
        std::vector<double> net_exp;
        std::vector<double> long_exp;
        std::vector<double> short_exp;

        for (const auto& holdings : holdings_series_) {
            auto metrics = holdings.calculate_metrics();

            timestamps.push_back(holdings.timestamp());
            gross_exp.push_back(metrics.gross_exposure);
            net_exp.push_back(metrics.net_exposure);
            long_exp.push_back(metrics.long_exposure);
            short_exp.push_back(metrics.short_exposure);
        }

        ExposureSeries result;
        result.gross_exposure = TimeSeries<double>{timestamps, gross_exp, "gross_exposure"};
        result.net_exposure   = TimeSeries<double>{timestamps, net_exp, "net_exposure"};
        result.long_exposure  = TimeSeries<double>{timestamps, long_exp, "long_exposure"};
        result.short_exposure = TimeSeries<double>{timestamps, short_exp, "short_exposure"};

        return Result<ExposureSeries>::success(std::move(result));
    }
};

}  // namespace pyfolio::positions