#pragma once

#include "../core/datetime.h"
#include "../core/error_handling.h"
#include "../core/types.h"
#include "transaction.h"
#include <map>
#include <queue>
#include <vector>

namespace pyfolio::transactions {

/**
 * @brief Round trip trade information
 */
struct RoundTrip {
    Symbol symbol;
    DateTime open_date;
    DateTime close_date;
    Shares shares;
    Price open_price;
    Price close_price;
    double commission_open;
    double commission_close;
    double slippage_open;
    double slippage_close;

    /**
     * @brief Calculate round trip P&L
     */
    double pnl() const {
        double gross_pnl   = shares * (close_price - open_price);
        double total_costs = commission_open + commission_close + std::abs(shares) * (slippage_open + slippage_close);
        return gross_pnl - total_costs;
    }

    /**
     * @brief Calculate return percentage
     */
    double return_pct() const {
        double initial_value = std::abs(shares * open_price);
        if (initial_value == 0.0)
            return 0.0;
        return pnl() / initial_value;
    }

    /**
     * @brief Calculate duration in days
     */
    int duration_days() const { return open_date.business_days_until(close_date); }

    /**
     * @brief Check if round trip is a win
     */
    bool is_win() const { return pnl() > 0.0; }
};

/**
 * @brief FIFO queue for open positions
 */
struct OpenPosition {
    DateTime timestamp;
    Shares shares;
    Price price;
    double commission;
    double slippage;

    OpenPosition(const TransactionRecord& txn)
        : timestamp(txn.timestamp()), shares(txn.shares()), price(txn.price()), commission(txn.commission()),
          slippage(txn.slippage()) {}
};

/**
 * @brief Round trip analyzer using FIFO methodology
 */
class RoundTripAnalyzer {
  private:
    std::map<Symbol, std::queue<OpenPosition>> open_positions_;
    std::vector<RoundTrip> completed_trips_;

    /**
     * @brief Process a single transaction
     */
    void process_transaction(const TransactionRecord& txn) {
        const Symbol& symbol = txn.symbol();
        auto& position_queue = open_positions_[symbol];

        if (txn.shares() > 0) {
            // Buy transaction - add to open positions
            position_queue.push(OpenPosition{txn});
        } else {
            // Sell transaction - close positions FIFO
            Shares remaining_shares = std::abs(txn.shares());

            while (remaining_shares > 0 && !position_queue.empty()) {
                OpenPosition& open_pos = position_queue.front();

                Shares shares_to_close = std::min(remaining_shares, open_pos.shares);

                // Create round trip
                RoundTrip trip;
                trip.symbol      = symbol;
                trip.open_date   = open_pos.timestamp;
                trip.close_date  = txn.timestamp();
                trip.shares      = shares_to_close;
                trip.open_price  = open_pos.price;
                trip.close_price = txn.price();

                // Allocate costs proportionally
                double share_ratio    = shares_to_close / std::abs(txn.shares());
                trip.commission_open  = open_pos.commission * (shares_to_close / open_pos.shares);
                trip.commission_close = txn.commission() * share_ratio;
                trip.slippage_open    = open_pos.slippage * (shares_to_close / open_pos.shares);
                trip.slippage_close   = txn.slippage() * share_ratio;

                completed_trips_.push_back(trip);

                // Update open position
                open_pos.shares -= shares_to_close;
                open_pos.commission *= (1.0 - shares_to_close / open_pos.shares);
                open_pos.slippage *= (1.0 - shares_to_close / open_pos.shares);

                if (open_pos.shares <= 0) {
                    position_queue.pop();
                }

                remaining_shares -= shares_to_close;
            }

            // If there are still shares to sell, it's a short position
            if (remaining_shares > 0) {
                OpenPosition short_pos{txn};
                short_pos.shares = -remaining_shares;
                position_queue.push(short_pos);
            }
        }
    }

  public:
    /**
     * @brief Analyze transactions to find round trips
     */
    Result<std::vector<RoundTrip>> analyze(const TransactionSeries& transactions) {
        // Clear previous analysis
        open_positions_.clear();
        completed_trips_.clear();

        // Process all transactions
        for (const auto& txn : transactions) {
            process_transaction(txn);
        }

        return Result<std::vector<RoundTrip>>::success(completed_trips_);
    }

    /**
     * @brief Get remaining open positions
     */
    std::map<Symbol, std::vector<OpenPosition>> get_open_positions() const {
        std::map<Symbol, std::vector<OpenPosition>> result;

        for (const auto& [symbol, queue] : open_positions_) {
            auto queue_copy = queue;
            while (!queue_copy.empty()) {
                result[symbol].push_back(queue_copy.front());
                queue_copy.pop();
            }
        }

        return result;
    }
};

/**
 * @brief Round trip statistics
 */
struct RoundTripStatistics {
    int total_trips;
    int winning_trips;
    int losing_trips;
    double win_rate;
    double average_pnl;
    double average_return;
    double average_duration_days;
    double total_pnl;
    double best_trade_pnl;
    double worst_trade_pnl;
    double profit_factor;

    /**
     * @brief Calculate statistics from round trips
     */
    static Result<RoundTripStatistics> calculate(const std::vector<RoundTrip>& trips) {
        if (trips.empty()) {
            // Return empty statistics for empty trips
            RoundTripStatistics empty_stats{};
            empty_stats.total_trips = 0;
            empty_stats.winning_trips = 0;
            empty_stats.losing_trips = 0;
            empty_stats.win_rate = 0.0;
            empty_stats.average_pnl = 0.0;
            empty_stats.average_return = 0.0;
            empty_stats.average_duration_days = 0.0;
            empty_stats.total_pnl = 0.0;
            empty_stats.best_trade_pnl = 0.0;
            empty_stats.worst_trade_pnl = 0.0;
            empty_stats.profit_factor = 1.0;
            return Result<RoundTripStatistics>::success(empty_stats);
        }

        RoundTripStatistics stats{};
        stats.total_trips = static_cast<int>(trips.size());

        double total_wins       = 0.0;
        double total_losses     = 0.0;
        double total_duration   = 0.0;
        double total_return_pct = 0.0;

        stats.best_trade_pnl  = std::numeric_limits<double>::lowest();
        stats.worst_trade_pnl = std::numeric_limits<double>::max();

        for (const auto& trip : trips) {
            double pnl = trip.pnl();
            stats.total_pnl += pnl;

            if (pnl > 0) {
                stats.winning_trips++;
                total_wins += pnl;
            } else {
                stats.losing_trips++;
                total_losses += std::abs(pnl);
            }

            stats.best_trade_pnl  = std::max(stats.best_trade_pnl, pnl);
            stats.worst_trade_pnl = std::min(stats.worst_trade_pnl, pnl);

            total_duration += trip.duration_days();
            total_return_pct += trip.return_pct();
        }

        stats.win_rate              = static_cast<double>(stats.winning_trips) / stats.total_trips;
        stats.average_pnl           = stats.total_pnl / stats.total_trips;
        stats.average_return        = total_return_pct / stats.total_trips;
        stats.average_duration_days = total_duration / stats.total_trips;

        // Profit factor = gross wins / gross losses
        stats.profit_factor =
            total_losses > 0 ? total_wins / total_losses : (total_wins > 0 ? std::numeric_limits<double>::max() : 1.0);

        return Result<RoundTripStatistics>::success(stats);
    }
};

/**
 * @brief Group round trips by various criteria
 */
inline std::map<Symbol, std::vector<RoundTrip>> group_by_symbol(const std::vector<RoundTrip>& trips) {
    std::map<Symbol, std::vector<RoundTrip>> grouped;

    for (const auto& trip : trips) {
        grouped[trip.symbol].push_back(trip);
    }

    return grouped;
}

/**
 * @brief Group round trips by duration bucket
 */
inline std::map<std::string, std::vector<RoundTrip>> group_by_duration(const std::vector<RoundTrip>& trips) {
    std::map<std::string, std::vector<RoundTrip>> grouped;

    for (const auto& trip : trips) {
        int days = trip.duration_days();

        std::string bucket;
        if (days == 0) {
            bucket = "intraday";
        } else if (days <= 1) {
            bucket = "1_day";
        } else if (days <= 5) {
            bucket = "2-5_days";
        } else if (days <= 10) {
            bucket = "6-10_days";
        } else if (days <= 21) {
            bucket = "11-21_days";
        } else if (days <= 42) {
            bucket = "22-42_days";
        } else if (days <= 63) {
            bucket = "43-63_days";
        } else {
            bucket = "64+_days";
        }

        grouped[bucket].push_back(trip);
    }

    return grouped;
}

/**
 * @brief Calculate round trip performance by time period
 */
inline Result<DataFrame> round_trip_performance_by_period(const std::vector<RoundTrip>& trips, Frequency freq) {
    if (trips.empty()) {
        return Result<DataFrame>::error(ErrorCode::InsufficientData, "No round trips to analyze");
    }

    // Group by close date period
    std::map<DateTime, std::vector<RoundTrip>> period_trips;

    for (const auto& trip : trips) {
        DateTime period_start;
        auto close_date = trip.close_date.to_date();

        switch (freq) {
            case Frequency::Monthly:
                period_start = DateTime{close_date.year() / close_date.month() / std::chrono::day{1}};
                break;
            case Frequency::Quarterly: {
                auto month               = close_date.month();
                auto quarter_start_month = ((static_cast<unsigned>(month) - 1) / 3) * 3 + 1;
                period_start =
                    DateTime{close_date.year() / std::chrono::month{quarter_start_month} / std::chrono::day{1}};
            } break;
            case Frequency::Yearly:
                period_start = DateTime{close_date.year() / std::chrono::January / std::chrono::day{1}};
                break;
            default:
                period_start = trip.close_date;
        }

        period_trips[period_start].push_back(trip);
    }

    // Calculate statistics for each period
    std::vector<DateTime> periods;
    std::vector<double> total_pnls;
    std::vector<double> win_rates;
    std::vector<double> avg_returns;
    std::vector<int> trip_counts;

    for (const auto& [period, trips_in_period] : period_trips) {
        auto stats = RoundTripStatistics::calculate(trips_in_period);
        if (stats.is_ok()) {
            periods.push_back(period);
            total_pnls.push_back(stats.value().total_pnl);
            win_rates.push_back(stats.value().win_rate);
            avg_returns.push_back(stats.value().average_return);
            trip_counts.push_back(stats.value().total_trips);
        }
    }

    DataFrame df{periods};
    df.add_column("total_pnl", total_pnls);
    df.add_column("win_rate", win_rates);
    df.add_column("avg_return", avg_returns);
    df.add_column("trip_count", trip_counts);

    return Result<DataFrame>::success(std::move(df));
}

/**
 * @brief Filter round trips by criteria
 */
inline std::vector<RoundTrip> filter_round_trips(const std::vector<RoundTrip>& trips,
                                                 const std::function<bool(const RoundTrip&)>& predicate) {
    std::vector<RoundTrip> filtered;

    std::copy_if(trips.begin(), trips.end(), std::back_inserter(filtered), predicate);

    return filtered;
}

/**
 * @brief Get top N round trips by P&L
 */
inline std::vector<RoundTrip> top_round_trips(const std::vector<RoundTrip>& trips, size_t n) {
    std::vector<RoundTrip> sorted_trips = trips;

    std::partial_sort(sorted_trips.begin(), sorted_trips.begin() + std::min(n, sorted_trips.size()), sorted_trips.end(),
                      [](const RoundTrip& a, const RoundTrip& b) { return a.pnl() > b.pnl(); });

    sorted_trips.resize(std::min(n, sorted_trips.size()));

    return sorted_trips;
}

}  // namespace pyfolio::transactions