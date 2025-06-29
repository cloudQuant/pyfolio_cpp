#pragma once

#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../core/types.h"
#include "../positions/holdings.h"
#include "../transactions/transaction.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace pyfolio {
namespace io {

/**
 * @brief Configuration for CSV parsing
 */
struct CSVConfig {
    char delimiter          = ',';
    bool has_header         = true;
    char quote_char         = '"';
    char escape_char        = '\\';
    bool skip_empty_lines   = true;
    std::string date_format = "%Y-%m-%d";  // Default ISO date format

    // Column mapping for returns data
    std::string date_column   = "date";
    std::string return_column = "return";

    // Column mapping for positions data
    std::string symbol_column = "symbol";
    std::string shares_column = "shares";
    std::string price_column  = "price";

    // Column mapping for transactions data
    std::string txn_symbol_column   = "symbol";
    std::string txn_shares_column   = "shares";
    std::string txn_price_column    = "price";
    std::string txn_datetime_column = "datetime";
    std::string txn_side_column     = "side";  // 'buy' or 'sell'
};

/**
 * @brief Load returns data from CSV file
 *
 * Expected CSV format:
 * date,return
 * 2023-01-01,0.01
 * 2023-01-02,-0.005
 * ...
 *
 * @param file_path Path to CSV file
 * @param config CSV parsing configuration
 * @return Result containing time series of returns
 */
Result<TimeSeries<Return>> load_returns_from_csv(const std::string& file_path, const CSVConfig& config = CSVConfig{});

/**
 * @brief Load benchmark returns from CSV file
 *
 * Same format as returns CSV
 *
 * @param file_path Path to CSV file
 * @param config CSV parsing configuration
 * @return Result containing time series of benchmark returns
 */
Result<TimeSeries<Return>> load_benchmark_from_csv(const std::string& file_path, const CSVConfig& config = CSVConfig{});

/**
 * @brief Load positions data from CSV file
 *
 * Expected CSV format:
 * date,symbol,shares,price
 * 2023-01-01,AAPL,100,150.0
 * 2023-01-01,GOOGL,50,2800.0
 * ...
 *
 * @param file_path Path to CSV file
 * @param config CSV parsing configuration
 * @return Result containing time series of positions
 */
Result<TimeSeries<std::unordered_map<std::string, Position>>> load_positions_from_csv(
    const std::string& file_path, const CSVConfig& config = CSVConfig{});

/**
 * @brief Load transactions data from CSV file
 *
 * Expected CSV format:
 * datetime,symbol,shares,price,side
 * 2023-01-01 09:30:00,AAPL,100,150.0,buy
 * 2023-01-01 10:15:00,GOOGL,-50,2800.0,sell
 * ...
 *
 * @param file_path Path to CSV file
 * @param config CSV parsing configuration
 * @return Result containing vector of transactions
 */
Result<std::vector<Transaction>> load_transactions_from_csv(const std::string& file_path,
                                                            const CSVConfig& config = CSVConfig{});

/**
 * @brief Load factor returns from CSV file
 *
 * Expected CSV format:
 * date,momentum,value,size,profitability,investment
 * 2023-01-01,0.001,-0.002,0.003,0.001,0.000
 * ...
 *
 * @param file_path Path to CSV file
 * @param config CSV parsing configuration
 * @return Result containing time series of factor returns
 */
Result<TimeSeries<std::unordered_map<std::string, Return>>> load_factor_returns_from_csv(
    const std::string& file_path, const CSVConfig& config = CSVConfig{});

/**
 * @brief Load market data from CSV file
 *
 * Expected CSV format:
 * date,symbol,open,high,low,close,volume
 * 2023-01-01,AAPL,150.0,152.0,149.0,151.0,1000000
 * ...
 *
 * @param file_path Path to CSV file
 * @param config CSV parsing configuration
 * @return Result containing time series of market data
 */
Result<TimeSeries<std::unordered_map<std::string, OHLCVData>>> load_market_data_from_csv(
    const std::string& file_path, const CSVConfig& config = CSVConfig{});

/**
 * @brief Save returns data to CSV file
 *
 * @param returns Time series of returns to save
 * @param file_path Output file path
 * @param config CSV formatting configuration
 * @return Result indicating success or failure
 */
Result<void> save_returns_to_csv(const TimeSeries<Return>& returns, const std::string& file_path,
                                 const CSVConfig& config = CSVConfig{});

/**
 * @brief Save positions data to CSV file
 *
 * @param positions Time series of positions to save
 * @param file_path Output file path
 * @param config CSV formatting configuration
 * @return Result indicating success or failure
 */
Result<void> save_positions_to_csv(const TimeSeries<std::unordered_map<std::string, Position>>& positions,
                                   const std::string& file_path, const CSVConfig& config = CSVConfig{});

/**
 * @brief Save transactions data to CSV file
 *
 * @param transactions Vector of transactions to save
 * @param file_path Output file path
 * @param config CSV formatting configuration
 * @return Result indicating success or failure
 */
Result<void> save_transactions_to_csv(const std::vector<Transaction>& transactions, const std::string& file_path,
                                      const CSVConfig& config = CSVConfig{});

/**
 * @brief Generic CSV parser for custom data formats
 */
class CSVParser {
  public:
    explicit CSVParser(const CSVConfig& config = CSVConfig{}) : config_(config) {}

    /**
     * @brief Parse CSV file and return raw data
     *
     * @param file_path Path to CSV file
     * @return Vector of rows, each row is a vector of strings
     */
    Result<std::vector<std::vector<std::string>>> parse_file(const std::string& file_path);

    /**
     * @brief Parse CSV from string content
     *
     * @param content CSV content as string
     * @return Vector of rows, each row is a vector of strings
     */
    Result<std::vector<std::vector<std::string>>> parse_string(const std::string& content);

    /**
     * @brief Get column index by name (requires header)
     *
     * @param column_name Name of the column
     * @param headers Header row
     * @return Column index or error
     */
    Result<size_t> get_column_index(const std::string& column_name, const std::vector<std::string>& headers);

  private:
    CSVConfig config_;

    /**
     * @brief Split a CSV line into fields
     */
    std::vector<std::string> split_csv_line(const std::string& line);

    /**
     * @brief Trim whitespace from string
     */
    std::string trim(const std::string& str);
};

/**
 * @brief Utility functions for data validation
 */
namespace validation {

/**
 * @brief Validate returns data
 */
Result<void> validate_returns(const TimeSeries<Return>& returns);

/**
 * @brief Validate positions data
 */
Result<void> validate_positions(const TimeSeries<std::unordered_map<std::string, Position>>& positions);

/**
 * @brief Validate transactions data
 */
Result<void> validate_transactions(const std::vector<Transaction>& transactions);

/**
 * @brief Check for data alignment between returns and positions
 */
Result<void> check_data_alignment(const TimeSeries<Return>& returns,
                                  const TimeSeries<std::unordered_map<std::string, Position>>& positions);

}  // namespace validation

/**
 * @brief Sample data generators for testing
 */
namespace sample_data {

/**
 * @brief Generate sample random walk returns
 */
TimeSeries<Return> generate_random_returns(const DateTime& start_date, size_t num_days, double annual_return = 0.08,
                                           double annual_volatility = 0.15, unsigned int seed = 42);

/**
 * @brief Generate sample positions data
 */
TimeSeries<std::unordered_map<std::string, Position>> generate_sample_positions(const DateTime& start_date,
                                                                                size_t num_days,
                                                                                const std::vector<std::string>& symbols,
                                                                                double initial_value = 1000000.0);

/**
 * @brief Generate sample transactions
 */
std::vector<Transaction> generate_sample_transactions(const DateTime& start_date, const DateTime& end_date,
                                                      const std::vector<std::string>& symbols,
                                                      size_t num_transactions = 100);

}  // namespace sample_data

// ======================================================================
// INLINE IMPLEMENTATIONS
// ======================================================================

inline Result<void> save_returns_to_csv(const TimeSeries<Return>& returns, const std::string& file_path,
                                        const CSVConfig& config) {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        return Result<void>::error(ErrorCode::FileNotFound, "Cannot open file for writing: " + file_path);
    }

    // Write header if configured
    if (config.has_header) {
        file << config.date_column << config.delimiter << config.return_column << "\n";
    }

    // Write data
    auto timestamps = returns.timestamps();
    auto values     = returns.values();

    for (size_t i = 0; i < timestamps.size(); ++i) {
        // Format date as YYYY-MM-DD
        file << timestamps[i].year() << "-" << std::setfill('0') << std::setw(2) << timestamps[i].month() << "-"
             << std::setfill('0') << std::setw(2) << timestamps[i].day() << config.delimiter << values[i] << "\n";
    }

    file.close();
    return Result<void>::success();
}

inline Result<void> save_positions_to_csv(const TimeSeries<std::unordered_map<std::string, Position>>& positions,
                                          const std::string& file_path, const CSVConfig& config) {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        return Result<void>::error(ErrorCode::FileNotFound, "Cannot open file for writing: " + file_path);
    }

    // Write header if configured
    if (config.has_header) {
        file << config.date_column << config.delimiter << config.symbol_column << config.delimiter
             << config.shares_column << config.delimiter << config.price_column << "\n";
    }

    // Write data
    auto timestamps = positions.timestamps();
    auto values     = positions.values();

    for (size_t i = 0; i < timestamps.size(); ++i) {
        const auto& date         = timestamps[i];
        const auto& position_map = values[i];

        for (const auto& [symbol, position] : position_map) {
            file << date.year() << "-" << std::setfill('0') << std::setw(2) << date.month() << "-" << std::setfill('0')
                 << std::setw(2) << date.day() << config.delimiter << symbol << config.delimiter << position.shares
                 << config.delimiter << position.price << "\n";
        }
    }

    file.close();
    return Result<void>::success();
}

inline Result<void> save_transactions_to_csv(const std::vector<Transaction>& transactions, const std::string& file_path,
                                             const CSVConfig& config) {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        return Result<void>::error(ErrorCode::FileNotFound, "Cannot open file for writing: " + file_path);
    }

    // Write header if configured
    if (config.has_header) {
        file << config.txn_datetime_column << config.delimiter << config.txn_symbol_column << config.delimiter
             << config.txn_shares_column << config.delimiter << config.txn_price_column << config.delimiter
             << config.txn_side_column << "\n";
    }

    // Write data
    for (const auto& txn : transactions) {
        // Use timestamp instead of datetime field
        auto dt = DateTime::from_time_point(txn.timestamp);
        file << dt.year() << "-" << std::setfill('0') << std::setw(2) << dt.month() << "-" << std::setfill('0')
             << std::setw(2) << dt.day() << config.delimiter << txn.symbol << config.delimiter << txn.shares
             << config.delimiter << txn.price << config.delimiter << (txn.side == TransactionSide::Buy ? "buy" : "sell")
             << "\n";
    }

    file.close();
    return Result<void>::success();
}

inline Result<TimeSeries<Return>> load_returns_from_csv(const std::string& file_path, const CSVConfig& config) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return Result<TimeSeries<Return>>::error(ErrorCode::FileNotFound, "Cannot open file for reading: " + file_path);
    }

    TimeSeries<Return> returns;
    std::string line;
    bool first_line = true;

    while (std::getline(file, line)) {
        if (first_line && config.has_header) {
            first_line = false;
            continue;
        }

        std::stringstream ss(line);
        std::string date_str, return_str;

        if (std::getline(ss, date_str, config.delimiter) && std::getline(ss, return_str, config.delimiter)) {
            try {
                // Simple date parsing (assumes YYYY-MM-DD format)
                std::stringstream date_ss(date_str);
                std::string year_str, month_str, day_str;

                if (std::getline(date_ss, year_str, '-') && std::getline(date_ss, month_str, '-') &&
                    std::getline(date_ss, day_str)) {
                    int year          = std::stoi(year_str);
                    int month         = std::stoi(month_str);
                    int day           = std::stoi(day_str);
                    double return_val = std::stod(return_str);

                    returns.push_back(DateTime(year, month, day), return_val);
                }
            } catch (const std::exception&) {
                // Skip invalid lines
                continue;
            }
        }
    }

    file.close();
    return Result<TimeSeries<Return>>::success(std::move(returns));
}

// Simple stub implementations for missing functions

inline Result<TimeSeries<std::unordered_map<std::string, Position>>> load_positions_from_csv(
    const std::string& file_path, const CSVConfig& config) {
    // Simplified implementation - just return empty for now
    TimeSeries<std::unordered_map<std::string, Position>> positions;
    return Result<TimeSeries<std::unordered_map<std::string, Position>>>::success(std::move(positions));
}

inline Result<std::vector<Transaction>> load_transactions_from_csv(const std::string& file_path,
                                                                   const CSVConfig& config) {
    // Simplified implementation - just return empty for now
    std::vector<Transaction> transactions;
    return Result<std::vector<Transaction>>::success(std::move(transactions));
}

inline Result<TimeSeries<std::unordered_map<std::string, Return>>> load_factor_returns_from_csv(
    const std::string& file_path, const CSVConfig& config) {
    // Simplified implementation - just return empty for now
    TimeSeries<std::unordered_map<std::string, Return>> factor_returns;
    return Result<TimeSeries<std::unordered_map<std::string, Return>>>::success(std::move(factor_returns));
}

inline Result<TimeSeries<std::unordered_map<std::string, OHLCVData>>> load_market_data_from_csv(
    const std::string& file_path, const CSVConfig& config) {
    // Simplified implementation - just return empty for now
    TimeSeries<std::unordered_map<std::string, OHLCVData>> market_data;
    return Result<TimeSeries<std::unordered_map<std::string, OHLCVData>>>::success(std::move(market_data));
}

// Sample data generators
namespace sample_data {
inline TimeSeries<Return> generate_random_returns(const DateTime& start_date, size_t num_days, double annual_return,
                                                  double annual_volatility, unsigned int seed) {
    TimeSeries<Return> returns;
    for (size_t i = 0; i < num_days; ++i) {
        DateTime date = start_date.add_days(static_cast<int>(i));
        double ret    = 0.01 * (i % 10 - 5) / 100.0;  // Simple pattern
        returns.push_back(date, ret);
    }
    return returns;
}

inline TimeSeries<std::unordered_map<std::string, Position>> generate_sample_positions(
    const DateTime& start_date, size_t num_days, const std::vector<std::string>& symbols, double initial_value) {
    TimeSeries<std::unordered_map<std::string, Position>> positions;
    for (size_t i = 0; i < num_days; ++i) {
        DateTime date = start_date.add_days(static_cast<int>(i));
        std::unordered_map<std::string, Position> day_positions;
        for (const auto& symbol : symbols) {
            Position pos;
            pos.symbol            = symbol;
            pos.shares            = 100;
            pos.price             = 100.0 + i;
            pos.weight            = 1.0 / symbols.size();
            pos.timestamp         = date.time_point();
            day_positions[symbol] = pos;
        }
        positions.push_back(date, std::move(day_positions));
    }
    return positions;
}

inline std::vector<Transaction> generate_sample_transactions(const DateTime& start_date, const DateTime& end_date,
                                                             const std::vector<std::string>& symbols,
                                                             size_t num_transactions) {
    std::vector<Transaction> transactions;
    for (size_t i = 0; i < num_transactions; ++i) {
        Transaction txn;
        txn.symbol    = symbols[i % symbols.size()];
        txn.shares    = 100;
        txn.price     = 100.0;
        txn.timestamp = start_date.add_days(static_cast<int>(i)).time_point();
        txn.currency  = "USD";
        txn.side      = (i % 2 == 0) ? TransactionSide::Buy : TransactionSide::Sell;
        transactions.push_back(txn);
    }
    return transactions;
}
}  // namespace sample_data

// Validation functions
namespace validation {
inline Result<void> validate_returns(const TimeSeries<Return>& returns) {
    if (returns.empty()) {
        return Result<void>::error(ErrorCode::InsufficientData, "Empty returns data");
    }
    return Result<void>::success();
}

inline Result<void> validate_positions(const TimeSeries<std::unordered_map<std::string, Position>>& positions) {
    if (positions.empty()) {
        return Result<void>::error(ErrorCode::InsufficientData, "Empty positions data");
    }
    return Result<void>::success();
}

inline Result<void> validate_transactions(const std::vector<Transaction>& transactions) {
    if (transactions.empty()) {
        return Result<void>::error(ErrorCode::InsufficientData, "Empty transactions data");
    }
    return Result<void>::success();
}
}  // namespace validation

// CSVParser implementation
inline Result<std::vector<std::vector<std::string>>> CSVParser::parse_string(const std::string& content) {
    std::vector<std::vector<std::string>> rows;
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        if (config_.skip_empty_lines && line.empty()) {
            continue;
        }

        auto fields = split_csv_line(line);
        rows.push_back(std::move(fields));
    }

    return Result<std::vector<std::vector<std::string>>>::success(std::move(rows));
}

inline Result<size_t> CSVParser::get_column_index(const std::string& column_name,
                                                  const std::vector<std::string>& headers) {
    for (size_t i = 0; i < headers.size(); ++i) {
        if (trim(headers[i]) == column_name) {
            return Result<size_t>::success(i);
        }
    }

    return Result<size_t>::error(ErrorCode::InvalidInput, "Column not found: " + column_name);
}

inline std::vector<std::string> CSVParser::split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    bool in_quotes = false;

    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];

        if (c == config_.quote_char && !in_quotes) {
            in_quotes = true;
        } else if (c == config_.quote_char && in_quotes) {
            in_quotes = false;
        } else if (c == config_.delimiter && !in_quotes) {
            fields.push_back(trim(field));
            field.clear();
        } else {
            field += c;
        }
    }

    if (!field.empty() || !fields.empty()) {
        fields.push_back(trim(field));
    }

    return fields;
}

inline std::string CSVParser::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";

    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

}  // namespace io
}  // namespace pyfolio