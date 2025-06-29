#pragma once

#include "../core/dataframe.h"
#include "../core/datetime.h"
#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../core/types.h"
#include <algorithm>
#include <numeric>
#include <set>
#include <vector>

namespace pyfolio::transactions {

/**
 * @brief Transaction type enumeration
 */
enum class TransactionType { Buy, Sell };

/**
 * @brief Enhanced transaction class with comprehensive features
 */
class TransactionRecord {
  private:
    Symbol symbol_;
    Shares shares_;
    Price price_;
    DateTime timestamp_;
    Currency currency_;
    double commission_;
    double slippage_;
    std::string exchange_;
    std::string order_id_;
    TransactionType type_;

  public:
    // Constructors
    TransactionRecord(const Symbol& symbol, Shares shares, Price price, const DateTime& timestamp,
                      TransactionType type = TransactionType::Buy, const Currency& currency = "USD",
                      double commission = 0.0, double slippage = 0.0)
        : symbol_(symbol), shares_(shares), price_(price), timestamp_(timestamp), currency_(currency),
          commission_(commission), slippage_(slippage), type_(type) {}

    // Alternative constructor matching test expectations
    TransactionRecord(const Symbol& symbol, const DateTime& timestamp, Shares shares, Price price, TransactionType type,
                      const Currency& currency = "USD")
        : symbol_(symbol), shares_(shares), price_(price), timestamp_(timestamp), currency_(currency), commission_(0.0),
          slippage_(0.0), type_(type) {}

    // Getters
    const Symbol& symbol() const { return symbol_; }
    Shares shares() const { return shares_; }
    Price price() const { return price_; }
    const DateTime& timestamp() const { return timestamp_; }
    const DateTime& date() const { return timestamp_; }  // Alias for test compatibility
    const Currency& currency() const { return currency_; }
    double commission() const { return commission_; }
    double slippage() const { return slippage_; }
    const std::string& exchange() const { return exchange_; }
    const std::string& order_id() const { return order_id_; }
    TransactionType type() const { return type_; }

    // Calculated values
    double notional_value() const { return std::abs(shares_) * price_; }

    // Static factory method for validation
    static Result<TransactionRecord> create(const Symbol& symbol, const DateTime& timestamp, Shares shares, Price price,
                                            TransactionType type, const Currency& currency = "USD") {
        if (symbol.empty()) {
            return Result<TransactionRecord>::error(ErrorCode::InvalidInput, "Symbol cannot be empty");
        }
        if (price <= 0.0) {
            return Result<TransactionRecord>::error(ErrorCode::InvalidInput, "Price must be positive");
        }
        if (shares == 0.0) {
            return Result<TransactionRecord>::error(ErrorCode::InvalidInput, "Shares cannot be zero");
        }
        return Result<TransactionRecord>::success(TransactionRecord(symbol, timestamp, shares, price, type, currency));
    }

    // Setters
    void set_exchange(const std::string& exchange) { exchange_ = exchange; }
    void set_order_id(const std::string& order_id) { order_id_ = order_id; }

    /**
     * @brief Calculate transaction value
     */
    double value() const { return shares_ * price_; }

    /**
     * @brief Calculate net cash flow (including costs)
     */
    double net_cash_flow() const {
        // Negative for buys, positive for sells
        return -(shares_ * price_ + commission_ + std::abs(shares_ * slippage_));
    }

    /**
     * @brief Total transaction cost
     */
    double total_cost() const { return commission_ + std::abs(shares_ * slippage_); }

    /**
     * @brief Check if transaction is a buy
     */
    bool is_buy() const { return shares_ > 0; }

    /**
     * @brief Check if transaction is a sell
     */
    bool is_sell() const { return shares_ < 0; }

    /**
     * @brief Check if transaction is valid
     */
    Result<void> validate() const {
        if (symbol_.empty()) {
            return Result<void>::error(ErrorCode::InvalidInput, "Transaction symbol cannot be empty");
        }

        if (shares_ == 0) {
            return Result<void>::error(ErrorCode::InvalidInput, "Transaction shares cannot be zero");
        }

        if (price_ <= 0) {
            return Result<void>::error(ErrorCode::InvalidInput, "Transaction price must be positive");
        }

        if (commission_ < 0) {
            return Result<void>::error(ErrorCode::InvalidInput, "Commission cannot be negative");
        }

        return Result<void>::success();
    }
};

/**
 * @brief Collection of transactions with analysis capabilities
 */
class TransactionSeries {
  private:
    std::vector<TransactionRecord> transactions_;

    void sort_by_timestamp() {
        std::sort(transactions_.begin(), transactions_.end(),
                  [](const TransactionRecord& a, const TransactionRecord& b) { return a.timestamp() < b.timestamp(); });
    }

  public:
    using iterator       = std::vector<TransactionRecord>::iterator;
    using const_iterator = std::vector<TransactionRecord>::const_iterator;

    // Constructors
    TransactionSeries() = default;

    explicit TransactionSeries(const std::vector<TransactionRecord>& transactions) : transactions_(transactions) {
        sort_by_timestamp();
    }

    explicit TransactionSeries(std::vector<TransactionRecord>&& transactions) : transactions_(std::move(transactions)) {
        sort_by_timestamp();
    }

    // Capacity
    size_t size() const { return transactions_.size(); }
    bool empty() const { return transactions_.empty(); }

    // Element access
    const TransactionRecord& operator[](size_t i) const { return transactions_[i]; }
    TransactionRecord& operator[](size_t i) { return transactions_[i]; }

    const TransactionRecord& front() const { return transactions_.front(); }
    const TransactionRecord& back() const { return transactions_.back(); }

    // Iterators
    iterator begin() { return transactions_.begin(); }
    const_iterator begin() const { return transactions_.begin(); }
    iterator end() { return transactions_.end(); }
    const_iterator end() const { return transactions_.end(); }

    /**
     * @brief Add a transaction
     */
    Result<void> add_transaction(const TransactionRecord& transaction) {
        auto validation = transaction.validate();
        if (validation.is_error()) {
            return validation;
        }

        transactions_.push_back(transaction);
        sort_by_timestamp();

        return Result<void>::success();
    }

    /**
     * @brief Filter transactions by symbol
     */
    Result<TransactionSeries> filter_by_symbol(const Symbol& symbol) const {
        std::vector<TransactionRecord> filtered;

        std::copy_if(transactions_.begin(), transactions_.end(), std::back_inserter(filtered),
                     [&symbol](const TransactionRecord& t) { return t.symbol() == symbol; });

        return Result<TransactionSeries>::success(TransactionSeries{std::move(filtered)});
    }

    /**
     * @brief Filter transactions by date range
     */
    Result<TransactionSeries> filter_by_date_range(const DateTime& start, const DateTime& end) const {
        std::vector<TransactionRecord> filtered;

        std::copy_if(
            transactions_.begin(), transactions_.end(), std::back_inserter(filtered),
            [&start, &end](const TransactionRecord& t) { return t.timestamp() >= start && t.timestamp() <= end; });

        return Result<TransactionSeries>::success(TransactionSeries{std::move(filtered)});
    }

    /**
     * @brief Get unique symbols
     */
    std::vector<Symbol> get_symbols() const {
        std::vector<Symbol> symbols;
        for (const auto& txn : transactions_) {
            if (std::find(symbols.begin(), symbols.end(), txn.symbol()) == symbols.end()) {
                symbols.push_back(txn.symbol());
            }
        }
        return symbols;
    }

    /**
     * @brief Calculate total value traded
     */
    double total_value() const {
        return std::accumulate(transactions_.begin(), transactions_.end(), 0.0,
                               [](double sum, const TransactionRecord& t) { return sum + std::abs(t.value()); });
    }

    /**
     * @brief Calculate total commissions
     */
    double total_commissions() const {
        return std::accumulate(transactions_.begin(), transactions_.end(), 0.0,
                               [](double sum, const TransactionRecord& t) { return sum + t.commission(); });
    }

    /**
     * @brief Calculate total slippage
     */
    double total_slippage() const {
        return std::accumulate(
            transactions_.begin(), transactions_.end(), 0.0,
            [](double sum, const TransactionRecord& t) { return sum + std::abs(t.shares() * t.slippage()); });
    }

    /**
     * @brief Calculate net shares for a symbol
     */
    Shares net_shares(const Symbol& symbol) const {
        return std::accumulate(transactions_.begin(), transactions_.end(), 0.0,
                               [&symbol](double sum, const TransactionRecord& t) {
                                   return sum + (t.symbol() == symbol ? t.shares() : 0.0);
                               });
    }

    /**
     * @brief Group transactions by symbol
     */
    std::map<Symbol, TransactionSeries> group_by_symbol() const {
        std::map<Symbol, TransactionSeries> grouped;

        for (const auto& symbol : get_symbols()) {
            auto filtered = filter_by_symbol(symbol);
            if (filtered.is_ok()) {
                grouped[symbol] = filtered.value();
            }
        }

        return grouped;
    }

    /**
     * @brief Calculate daily transaction summary
     */
    Result<DataFrame> daily_summary() const {
        if (empty()) {
            return Result<DataFrame>::error(ErrorCode::InsufficientData, "No transactions to summarize");
        }

        std::map<DateTime, std::map<Symbol, double>> daily_volumes;
        std::map<DateTime, double> daily_totals;
        std::map<DateTime, double> daily_costs;

        for (const auto& txn : transactions_) {
            auto date = DateTime{txn.timestamp().to_date()};
            daily_volumes[date][txn.symbol()] += std::abs(txn.value());
            daily_totals[date] += std::abs(txn.value());
            daily_costs[date] += txn.total_cost();
        }

        // Create DataFrame
        std::vector<DateTime> dates;
        std::vector<double> totals;
        std::vector<double> costs;

        for (const auto& [date, total] : daily_totals) {
            dates.push_back(date);
            totals.push_back(total);
            costs.push_back(daily_costs[date]);
        }

        DataFrame df{dates};
        df.add_column("total_value", totals);
        df.add_column("total_costs", costs);

        // Add per-symbol columns
        for (const auto& symbol : get_symbols()) {
            std::vector<double> symbol_volumes;
            for (const auto& date : dates) {
                auto it = daily_volumes.find(date);
                if (it != daily_volumes.end()) {
                    auto sym_it = it->second.find(symbol);
                    symbol_volumes.push_back(sym_it != it->second.end() ? sym_it->second : 0.0);
                } else {
                    symbol_volumes.push_back(0.0);
                }
            }
            df.add_column(symbol + "_volume", symbol_volumes);
        }

        return Result<DataFrame>::success(std::move(df));
    }

    /**
     * @brief Aggregate transactions by day
     */
    Result<std::map<DateTime, std::vector<TransactionRecord>>> aggregate_daily() const {
        if (empty()) {
            return Result<std::map<DateTime, std::vector<TransactionRecord>>>::error(ErrorCode::InsufficientData,
                                                                                     "No transactions to aggregate");
        }

        std::map<DateTime, std::vector<TransactionRecord>> daily_agg;
        for (const auto& txn : transactions_) {
            auto date = DateTime{txn.timestamp().to_date()};
            daily_agg[date].push_back(txn);
        }

        return Result<std::map<DateTime, std::vector<TransactionRecord>>>::success(daily_agg);
    }

    /**
     * @brief Aggregate transactions by symbol
     */
    Result<std::map<Symbol, std::vector<TransactionRecord>>> aggregate_by_symbol() const {
        if (empty()) {
            return Result<std::map<Symbol, std::vector<TransactionRecord>>>::error(ErrorCode::InsufficientData,
                                                                                   "No transactions to aggregate");
        }

        std::map<Symbol, std::vector<TransactionRecord>> symbol_agg;
        for (const auto& txn : transactions_) {
            symbol_agg[txn.symbol()].push_back(txn);
        }

        return Result<std::map<Symbol, std::vector<TransactionRecord>>>::success(symbol_agg);
    }

    /**
     * @brief Calculate total notional value
     */
    Result<double> total_notional_value() const {
        if (empty()) {
            return Result<double>::error(ErrorCode::InsufficientData, "No transactions to calculate total");
        }

        double total = 0.0;
        for (const auto& txn : transactions_) {
            total += std::abs(txn.notional_value());
        }

        return Result<double>::success(total);
    }

    /**
     * @brief Calculate net shares by symbol
     */
    Result<std::map<Symbol, double>> net_shares_by_symbol() const {
        if (empty()) {
            return Result<std::map<Symbol, double>>::error(ErrorCode::InsufficientData,
                                                           "No transactions to calculate net shares");
        }

        std::map<Symbol, double> net_shares;
        for (const auto& txn : transactions_) {
            net_shares[txn.symbol()] += txn.shares();
        }

        return Result<std::map<Symbol, double>>::success(net_shares);
    }

    /**
     * @brief Calculate average transaction size
     */
    Result<double> average_transaction_size() const {
        if (empty()) {
            return Result<double>::error(ErrorCode::InsufficientData, "No transactions for average");
        }

        auto total_result = total_notional_value();
        if (total_result.is_error()) {
            return total_result;
        }

        return Result<double>::success(total_result.value() / transactions_.size());
    }

    /**
     * @brief Transaction statistics structure
     */
    struct TransactionStatistics {
        size_t total_transactions;
        double total_notional_value;
        double average_transaction_size;
        size_t unique_symbols;
        size_t trading_days;
    };

    /**
     * @brief Calculate transaction statistics
     */
    Result<TransactionStatistics> calculate_statistics() const {
        if (empty()) {
            return Result<TransactionStatistics>::error(ErrorCode::InsufficientData, "No transactions for statistics");
        }

        TransactionStatistics stats;
        stats.total_transactions = transactions_.size();

        auto total_result = total_notional_value();
        if (total_result.is_error()) {
            return Result<TransactionStatistics>::error(total_result.error().code, total_result.error().message);
        }
        stats.total_notional_value = total_result.value();

        auto avg_result = average_transaction_size();
        if (avg_result.is_error()) {
            return Result<TransactionStatistics>::error(avg_result.error().code, avg_result.error().message);
        }
        stats.average_transaction_size = avg_result.value();

        auto symbols         = get_symbols();
        stats.unique_symbols = symbols.size();

        // Calculate unique trading days
        std::set<DateTime> unique_days;
        for (const auto& txn : transactions_) {
            unique_days.insert(DateTime{txn.timestamp().to_date()});
        }
        stats.trading_days = unique_days.size();

        return Result<TransactionStatistics>::success(stats);
    }

    /**
     * @brief Calculate transaction costs
     */
    Result<double> calculate_transaction_costs(double commission_per_trade) const {
        if (empty()) {
            return Result<double>::error(ErrorCode::InsufficientData, "No transactions for cost calculation");
        }

        double total_costs = transactions_.size() * commission_per_trade;
        return Result<double>::success(total_costs);
    }

    /**
     * @brief Convert to DataFrame
     */
    Result<DataFrame> to_dataframe() const {
        if (empty()) {
            return Result<DataFrame>::error(ErrorCode::InsufficientData, "No transactions to convert");
        }

        std::vector<DateTime> timestamps;
        std::vector<std::string> symbols;
        std::vector<double> shares;
        std::vector<double> prices;
        std::vector<double> values;
        std::vector<double> commissions;

        for (const auto& txn : transactions_) {
            timestamps.push_back(txn.timestamp());
            symbols.push_back(txn.symbol());
            shares.push_back(txn.shares());
            prices.push_back(txn.price());
            values.push_back(txn.value());
            commissions.push_back(txn.commission());
        }

        DataFrame df{timestamps};
        df.add_column("symbol", symbols);
        df.add_column("shares", shares);
        df.add_column("price", prices);
        df.add_column("value", values);
        df.add_column("commission", commissions);

        return Result<DataFrame>::success(std::move(df));
    }
};

/**
 * @brief Calculate average transaction price for a symbol
 */
inline Result<double> calculate_average_price(const TransactionSeries& transactions, const Symbol& symbol) {
    auto symbol_txns_result = transactions.filter_by_symbol(symbol);

    if (symbol_txns_result.is_error()) {
        return Result<double>::error(symbol_txns_result.error().code, symbol_txns_result.error().message);
    }

    auto symbol_txns = symbol_txns_result.value();
    if (symbol_txns.empty()) {
        return Result<double>::error(ErrorCode::InvalidSymbol, "No transactions found for symbol: " + symbol);
    }

    double total_value  = 0.0;
    double total_shares = 0.0;

    for (const auto& txn : symbol_txns) {
        total_value += std::abs(txn.value());
        total_shares += std::abs(txn.shares());
    }

    if (total_shares == 0.0) {
        return Result<double>::error(ErrorCode::DivisionByZero, "Total shares is zero");
    }

    return Result<double>::success(total_value / total_shares);
}

/**
 * @brief Calculate portfolio turnover
 */
inline Result<double> calculate_turnover(const TransactionSeries& transactions, double portfolio_value,
                                         const DateTime& start, const DateTime& end) {
    if (portfolio_value <= 0) {
        return Result<double>::error(ErrorCode::InvalidInput, "Portfolio value must be positive");
    }

    auto period_txns_result = transactions.filter_by_date_range(start, end);

    if (period_txns_result.is_error()) {
        return Result<double>::error(period_txns_result.error().code, period_txns_result.error().message);
    }

    auto period_txns = period_txns_result.value();
    if (period_txns.empty()) {
        return Result<double>::success(0.0);
    }

    // Calculate half of total traded value (to avoid double counting)
    double traded_value = period_txns.total_value() / 2.0;

    // Annualize based on period length
    int days = start.business_days_until(end);
    if (days <= 0) {
        return Result<double>::error(ErrorCode::InvalidDateRange, "Invalid date range for turnover calculation");
    }

    double annualization_factor = constants::TRADING_DAYS_PER_YEAR / days;
    double annualized_turnover  = (traded_value / portfolio_value) * annualization_factor;

    return Result<double>::success(annualized_turnover);
}

}  // namespace pyfolio::transactions