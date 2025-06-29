#include <algorithm>
#include <cmath>
#include <numeric>
#include <pyfolio/utils/intraday.h>
#include <set>
#include <unordered_map>

namespace pyfolio::intraday {

Result<IntradayDetectionResult> IntradayAnalyzer::detect_intraday(const PositionSeries& positions,
                                                                  const TransactionSeries& transactions,
                                                                  double threshold) {
    if (positions.empty() || transactions.empty()) {
        return Result<IntradayDetectionResult>::error(ErrorCode::InvalidInput,
                                                      "Positions and transactions cannot be empty");
    }

    // Calculate overall position-to-transaction ratio
    double overall_ratio = calculate_position_transaction_ratio(positions, transactions);

    // Calculate per-symbol ratios for detailed analysis
    std::map<std::string, double> symbol_ratios;
    auto symbols = transactions.get_unique_symbols();

    for (const auto& symbol : symbols) {
        if (symbol != "cash") {
            double symbol_ratio   = calculate_symbol_ratio(positions, transactions, symbol);
            symbol_ratios[symbol] = symbol_ratio;
        }
    }

    // Determine if strategy is intraday
    bool is_intraday = overall_ratio < threshold;

    // Calculate confidence score
    double confidence = calculate_confidence_score(symbol_ratios, overall_ratio, threshold);

    // Find optimal sampling time (simplified - would use actual transaction timing)
    DateTime optimal_time = DateTime::parse("1900-01-01 15:30:00").value();  // Default 3:30 PM

    IntradayDetectionResult result{.is_intraday           = is_intraday,
                                   .threshold_ratio       = threshold,
                                   .optimal_sampling_time = optimal_time,
                                   .detection_method      = "position_transaction_ratio",
                                   .confidence_score      = confidence,
                                   .symbol_ratios         = symbol_ratios};

    // Validate results
    if (!validate_detection_results(result, positions, transactions)) {
        return Result<IntradayDetectionResult>::error(ErrorCode::CalculationError,
                                                      "Intraday detection validation failed");
    }

    return Result<IntradayDetectionResult>::success(result);
}

Result<PositionSeries> IntradayAnalyzer::estimate_intraday_positions(const TimeSeries<Return>& returns,
                                                                     const PositionSeries& positions,
                                                                     const TransactionSeries& transactions,
                                                                     const IntradayEstimationConfig& config) {
    if (positions.empty()) {
        return Result<PositionSeries>::error(ErrorCode::InvalidInput, "Position series cannot be empty");
    }

    PositionSeries estimated_positions;

    // Get all unique dates from positions
    auto dates = positions.get_dates();

    for (size_t i = 0; i < dates.size(); ++i) {
        const auto& date             = dates[i];
        const auto& current_snapshot = positions[i];

        // Get transactions for this date
        auto daily_transactions = get_transactions_for_date(transactions, date);

        if (daily_transactions.empty()) {
            // No transactions, use existing position
            estimated_positions.add_snapshot(current_snapshot);
            continue;
        }

        // For intraday strategies, estimate end-of-day positions
        PositionSnapshot previous_positions;
        if (i > 0) {
            previous_positions = positions[i - 1];
        }

        auto estimated_eod = estimate_eod_positions(date, daily_transactions, previous_positions);
        if (estimated_eod.is_ok()) {
            estimated_positions.add_snapshot(estimated_eod.value());
        } else {
            // Fallback to original position
            estimated_positions.add_snapshot(current_snapshot);
        }
    }

    return Result<PositionSeries>::success(estimated_positions);
}

Result<PositionSeries> IntradayAnalyzer::check_and_process_intraday(const IntradayDetectionResult& detection,
                                                                    const TimeSeries<Return>& returns,
                                                                    const PositionSeries& positions,
                                                                    const TransactionSeries& transactions) {
    if (!detection.is_intraday) {
        // Not intraday, return original positions
        return Result<PositionSeries>::success(positions);
    }

    // For intraday strategies, estimate better positions
    IntradayEstimationConfig config;
    auto estimated_positions = estimate_intraday_positions(returns, positions, transactions, config);

    if (estimated_positions.is_error()) {
        // Fallback to original positions if estimation fails
        return Result<PositionSeries>::success(positions);
    }

    return estimated_positions;
}

DateTime IntradayAnalyzer::find_peak_exposure_time(const std::vector<TransactionRecord>& daily_transactions) {
    if (daily_transactions.empty()) {
        return DateTime::parse("1900-01-01 15:30:00").value();  // Default
    }

    // Group transactions by hour
    auto hourly_volumes = group_transactions_by_hour(daily_transactions);

    // Find hour with maximum volume
    int peak_hour     = 15;  // Default 3 PM
    double max_volume = 0.0;

    for (const auto& [hour, volume] : hourly_volumes) {
        if (volume > max_volume) {
            max_volume = volume;
            peak_hour  = hour;
        }
    }

    // Create optimal sampling time
    std::string time_str = "1900-01-01 " + std::to_string(peak_hour) + ":30:00";
    return DateTime::parse(time_str).value();
}

double IntradayAnalyzer::calculate_position_transaction_ratio(const PositionSeries& positions,
                                                              const TransactionSeries& transactions) {
    if (positions.empty() || transactions.empty()) {
        return 1.0;  // Safe default
    }

    // Calculate average absolute position values (excluding cash)
    double total_position_value = 0.0;
    int position_count          = 0;

    for (size_t i = 0; i < positions.size(); ++i) {
        const auto& snapshot = positions[i];
        for (const auto& [symbol, value] : snapshot.positions()) {
            if (symbol != "cash") {
                total_position_value += std::abs(value);
                position_count++;
            }
        }
    }

    double avg_position_value = position_count > 0 ? total_position_value / position_count : 0.0;

    // Calculate average transaction volume
    auto txn_stats_result = transactions.calculate_statistics();
    if (txn_stats_result.is_error()) {
        return 1.0;  // Safe default
    }

    double avg_txn_value = txn_stats_result.value().average_transaction_size;

    // Calculate ratio
    if (avg_txn_value > 0) {
        return avg_position_value / avg_txn_value;
    }

    return 1.0;  // Safe default
}

double IntradayAnalyzer::calculate_symbol_ratio(const PositionSeries& positions, const TransactionSeries& transactions,
                                                const std::string& symbol) {
    // Filter transactions for this symbol
    auto symbol_filtered = transactions.filter_by_symbol(symbol);
    if (symbol_filtered.is_error()) {
        return 1.0;
    }

    auto symbol_transactions = symbol_filtered.value();
    if (symbol_transactions.empty()) {
        return 1.0;
    }

    // Calculate average position value for this symbol
    double total_position_value = 0.0;
    int count                   = 0;

    for (size_t i = 0; i < positions.size(); ++i) {
        double pos_value = positions[i].position_value(symbol);
        if (pos_value != 0.0) {
            total_position_value += std::abs(pos_value);
            count++;
        }
    }

    double avg_position_value = count > 0 ? total_position_value / count : 0.0;

    // Calculate average transaction value for this symbol
    auto stats_result = symbol_transactions.calculate_statistics();
    if (stats_result.is_error()) {
        return 1.0;
    }

    double avg_txn_value = stats_result.value().average_transaction_size;

    if (avg_txn_value > 0) {
        return avg_position_value / avg_txn_value;
    }

    return 1.0;
}

std::vector<TransactionRecord> IntradayAnalyzer::get_transactions_for_date(const TransactionSeries& transactions,
                                                                           const DateTime& date) {
    std::vector<TransactionRecord> daily_transactions;

    for (size_t i = 0; i < transactions.size(); ++i) {
        const auto& txn = transactions[i];
        if (txn.date().year() == date.year() && txn.date().month() == date.month() && txn.date().day() == date.day()) {
            daily_transactions.push_back(txn);
        }
    }

    return daily_transactions;
}

std::map<int, double> IntradayAnalyzer::group_transactions_by_hour(const std::vector<TransactionRecord>& transactions) {
    std::map<int, double> hourly_volumes;

    for (const auto& txn : transactions) {
        int hour      = txn.date().hour();  // Assuming DateTime has hour() method
        double volume = std::abs(txn.notional_value());
        hourly_volumes[hour] += volume;
    }

    return hourly_volumes;
}

Result<PositionSnapshot> IntradayAnalyzer::estimate_eod_positions(
    const DateTime& date, const std::vector<TransactionRecord>& daily_transactions,
    const PositionSnapshot& previous_positions) {
    // Start with previous day's positions
    std::map<std::string, double> estimated_positions = previous_positions.positions();

    // Apply daily transactions
    for (const auto& txn : daily_transactions) {
        estimated_positions[txn.symbol()] += txn.shares() * txn.price();
    }

    PositionSnapshot estimated_snapshot{date, estimated_positions};
    return Result<PositionSnapshot>::success(estimated_snapshot);
}

bool IntradayAnalyzer::validate_detection_results(const IntradayDetectionResult& result,
                                                  const PositionSeries& positions,
                                                  const TransactionSeries& transactions) {
    // Basic validation checks
    if (result.threshold_ratio <= 0 || result.threshold_ratio > 1.0) {
        return false;
    }

    if (result.confidence_score < 0 || result.confidence_score > 1.0) {
        return false;
    }

    // Check that symbol ratios are reasonable
    for (const auto& [symbol, ratio] : result.symbol_ratios) {
        if (ratio < 0 || ratio > 100.0) {  // Allow up to 100x as reasonable upper bound
            return false;
        }
    }

    return true;
}

double IntradayAnalyzer::calculate_confidence_score(const std::map<std::string, double>& symbol_ratios,
                                                    double overall_ratio, double threshold) {
    if (symbol_ratios.empty()) {
        return 0.5;  // Neutral confidence
    }

    // Calculate consistency across symbols
    std::vector<bool> symbol_classifications;
    for (const auto& [symbol, ratio] : symbol_ratios) {
        symbol_classifications.push_back(ratio < threshold);
    }

    // Count consistent classifications
    bool overall_classification = overall_ratio < threshold;
    int consistent_count        = 0;

    for (bool classification : symbol_classifications) {
        if (classification == overall_classification) {
            consistent_count++;
        }
    }

    double consistency_ratio = static_cast<double>(consistent_count) / symbol_classifications.size();

    // Distance from threshold also affects confidence
    double threshold_distance   = std::abs(overall_ratio - threshold) / threshold;
    double threshold_confidence = std::min(1.0, threshold_distance);

    // Combine consistency and threshold distance
    return (consistency_ratio + threshold_confidence) / 2.0;
}

namespace utils {

bool is_trading_hours(const DateTime& dt, int start_hour, int end_hour) {
    if (!dt.is_weekday()) {
        return false;
    }

    int hour = dt.hour();
    return hour >= start_hour && hour <= end_hour;
}

std::vector<double> normalize_positions(const std::vector<double>& positions) {
    if (positions.empty()) {
        return {};
    }

    // Calculate mean and standard deviation
    double sum  = std::accumulate(positions.begin(), positions.end(), 0.0);
    double mean = sum / positions.size();

    double sq_sum = 0.0;
    for (double pos : positions) {
        sq_sum += (pos - mean) * (pos - mean);
    }
    double std_dev = std::sqrt(sq_sum / positions.size());

    if (std_dev == 0.0) {
        return std::vector<double>(positions.size(), 0.0);
    }

    // Normalize
    std::vector<double> normalized;
    normalized.reserve(positions.size());

    for (double pos : positions) {
        normalized.push_back((pos - mean) / std_dev);
    }

    return normalized;
}

double calculate_average_absolute_deviation(const std::vector<double>& values) {
    if (values.empty()) {
        return 0.0;
    }

    // Calculate mean
    double sum  = std::accumulate(values.begin(), values.end(), 0.0);
    double mean = sum / values.size();

    // Calculate average absolute deviation
    double aad_sum = 0.0;
    for (double value : values) {
        aad_sum += std::abs(value - mean);
    }

    return aad_sum / values.size();
}

Result<std::vector<double>> interpolate_missing_positions(const std::vector<DateTime>& dates,
                                                          const std::vector<double>& known_positions,
                                                          const std::vector<TransactionRecord>& transactions) {
    if (dates.size() != known_positions.size()) {
        return Result<std::vector<double>>::error(ErrorCode::InvalidInput, "Dates and positions must have same size");
    }

    std::vector<double> interpolated = known_positions;

    // Simple forward fill for missing values (NaN represented as very large number)
    for (size_t i = 1; i < interpolated.size(); ++i) {
        if (std::isnan(interpolated[i]) || interpolated[i] == std::numeric_limits<double>::max()) {
            interpolated[i] = interpolated[i - 1];
        }
    }

    return Result<std::vector<double>>::success(interpolated);
}

}  // namespace utils

}  // namespace pyfolio::intraday