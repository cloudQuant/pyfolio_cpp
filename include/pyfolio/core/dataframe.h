#pragma once

#include "datetime.h"
#include "error_handling.h"
#include "time_series.h"
#include "types.h"
#include <any>
#include <map>
#include <typeindex>
#include <variant>

namespace pyfolio {

/**
 * @brief Column data variant for different data types
 */
using ColumnData = std::variant<std::vector<double>, std::vector<int>, std::vector<std::string>, std::vector<DateTime>>;

/**
 * @brief DataFrame for tabular financial data (similar to pandas DataFrame)
 */
class DataFrame {
  private:
    std::vector<DateTime> index_;
    std::map<std::string, ColumnData> columns_;
    std::vector<std::string> column_names_;

    template <typename T>
    std::vector<T>& get_column_data(const std::string& name) {
        auto it = columns_.find(name);
        if (it == columns_.end()) {
            throw std::runtime_error("Column not found: " + name);
        }

        try {
            return std::get<std::vector<T>>(it->second);
        } catch (const std::bad_variant_access&) {
            throw std::runtime_error("Column type mismatch for: " + name);
        }
    }

    template <typename T>
    const std::vector<T>& get_column_data(const std::string& name) const {
        auto it = columns_.find(name);
        if (it == columns_.end()) {
            throw std::runtime_error("Column not found: " + name);
        }

        try {
            return std::get<std::vector<T>>(it->second);
        } catch (const std::bad_variant_access&) {
            throw std::runtime_error("Column type mismatch for: " + name);
        }
    }

  public:
    using size_type = std::size_t;

    // Constructors
    DataFrame() = default;

    explicit DataFrame(const std::vector<DateTime>& index) : index_(index) {}

    DataFrame(const std::vector<DateTime>& index, const std::map<std::string, ColumnData>& columns)
        : index_(index), columns_(columns) {
        // Populate column names
        for (const auto& [name, data] : columns_) {
            column_names_.push_back(name);
        }

        validate_consistency();
    }

    // Capacity
    size_type size() const noexcept { return index_.size(); }
    size_type rows() const noexcept { return index_.size(); }
    size_type cols() const noexcept { return column_names_.size(); }
    bool empty() const noexcept { return index_.empty(); }

    // Index operations
    const std::vector<DateTime>& index() const { return index_; }
    const std::vector<std::string>& columns() const { return column_names_; }

    /**
     * @brief Add a column
     */
    template <typename T>
    Result<void> add_column(const std::string& name, const std::vector<T>& data) {
        if (data.size() != index_.size()) {
            return Result<void>::error(ErrorCode::InvalidInput, "Column data size must match index size");
        }

        if (columns_.find(name) != columns_.end()) {
            return Result<void>::error(ErrorCode::InvalidInput, "Column already exists: " + name);
        }

        columns_[name] = data;
        column_names_.push_back(name);

        return Result<void>::success();
    }

    /**
     * @brief Remove a column
     */
    Result<void> remove_column(const std::string& name) {
        auto it = columns_.find(name);
        if (it == columns_.end()) {
            return Result<void>::error(ErrorCode::InvalidInput, "Column not found: " + name);
        }

        columns_.erase(it);
        column_names_.erase(std::remove(column_names_.begin(), column_names_.end(), name), column_names_.end());

        return Result<void>::success();
    }

    /**
     * @brief Get column as TimeSeries
     */
    template <typename T>
    Result<TimeSeries<T>> get_column(const std::string& name) const {
        try {
            const auto& data = get_column_data<T>(name);
            return Result<TimeSeries<T>>::success(TimeSeries<T>{index_, data, name});
        } catch (const std::exception& e) {
            return Result<TimeSeries<T>>::error(ErrorCode::InvalidInput, e.what());
        }
    }

    /**
     * @brief Set column values
     */
    template <typename T>
    Result<void> set_column(const std::string& name, const std::vector<T>& data) {
        if (data.size() != index_.size()) {
            return Result<void>::error(ErrorCode::InvalidInput, "Column data size must match index size");
        }

        auto it = columns_.find(name);
        if (it == columns_.end()) {
            // Add new column
            return add_column(name, data);
        } else {
            // Update existing column
            it->second = data;
            return Result<void>::success();
        }
    }

    /**
     * @brief Check if column exists
     */
    bool has_column(const std::string& name) const { return columns_.find(name) != columns_.end(); }

    /**
     * @brief Get value at specific row and column
     */
    template <typename T>
    Result<T> at(size_type row, const std::string& column) const {
        if (row >= size()) {
            return Result<T>::error(ErrorCode::InvalidInput, "Row index out of bounds: " + std::to_string(row));
        }

        try {
            const auto& data = get_column_data<T>(column);
            return Result<T>::success(data[row]);
        } catch (const std::exception& e) {
            return Result<T>::error(ErrorCode::InvalidInput, e.what());
        }
    }

    /**
     * @brief Set value at specific row and column
     */
    template <typename T>
    Result<void> set_at(size_type row, const std::string& column, const T& value) {
        if (row >= size()) {
            return Result<void>::error(ErrorCode::InvalidInput, "Row index out of bounds: " + std::to_string(row));
        }

        try {
            auto& data = get_column_data<T>(column);
            data[row]  = value;
            return Result<void>::success();
        } catch (const std::exception& e) {
            return Result<void>::error(ErrorCode::InvalidInput, e.what());
        }
    }

    /**
     * @brief Select rows by date range
     */
    Result<DataFrame> loc(const DateTime& start, const DateTime& end) const {
        if (start >= end) {
            return Result<DataFrame>::error(ErrorCode::InvalidDateRange, "Start date must be before end date");
        }

        std::vector<size_type> selected_indices;
        std::vector<DateTime> selected_index;

        for (size_type i = 0; i < size(); ++i) {
            if (index_[i] >= start && index_[i] <= end) {
                selected_indices.push_back(i);
                selected_index.push_back(index_[i]);
            }
        }

        if (selected_indices.empty()) {
            return Result<DataFrame>::error(ErrorCode::MissingData, "No data found in specified date range");
        }

        // Create new DataFrame with selected rows
        DataFrame result{selected_index};

        for (const auto& column_name : column_names_) {
            std::visit(
                [&](const auto& column_data) {
                    using T = typename std::decay_t<decltype(column_data)>::value_type;
                    std::vector<T> selected_data;
                    selected_data.reserve(selected_indices.size());

                    for (size_type idx : selected_indices) {
                        selected_data.push_back(column_data[idx]);
                    }

                    result.add_column(column_name, selected_data);
                },
                columns_.at(column_name));
        }

        return Result<DataFrame>::success(std::move(result));
    }

    /**
     * @brief Select specific columns
     */
    Result<DataFrame> select(const std::vector<std::string>& column_names) const {
        DataFrame result{index_};

        for (const auto& name : column_names) {
            auto it = columns_.find(name);
            if (it == columns_.end()) {
                return Result<DataFrame>::error(ErrorCode::InvalidInput, "Column not found: " + name);
            }

            result.columns_[name] = it->second;
            result.column_names_.push_back(name);
        }

        return Result<DataFrame>::success(std::move(result));
    }

    /**
     * @brief Sort by column
     */
    template <typename T>
    Result<DataFrame> sort_by(const std::string& column_name, bool ascending = true) const {
        try {
            const auto& sort_column = get_column_data<T>(column_name);

            // Create index vector and sort it
            std::vector<size_type> indices(size());
            std::iota(indices.begin(), indices.end(), 0);

            if (ascending) {
                std::sort(indices.begin(), indices.end(),
                          [&sort_column](size_type a, size_type b) { return sort_column[a] < sort_column[b]; });
            } else {
                std::sort(indices.begin(), indices.end(),
                          [&sort_column](size_type a, size_type b) { return sort_column[a] > sort_column[b]; });
            }

            // Create sorted DataFrame
            std::vector<DateTime> sorted_index;
            sorted_index.reserve(size());
            for (size_type idx : indices) {
                sorted_index.push_back(index_[idx]);
            }

            DataFrame result{sorted_index};

            for (const auto& name : column_names_) {
                std::visit(
                    [&](const auto& column_data) {
                        using ColType = typename std::decay_t<decltype(column_data)>::value_type;
                        std::vector<ColType> sorted_data;
                        sorted_data.reserve(size());

                        for (size_type idx : indices) {
                            sorted_data.push_back(column_data[idx]);
                        }

                        result.add_column(name, sorted_data);
                    },
                    columns_.at(name));
            }

            return Result<DataFrame>::success(std::move(result));

        } catch (const std::exception& e) {
            return Result<DataFrame>::error(ErrorCode::InvalidInput, e.what());
        }
    }

    /**
     * @brief Group by time period
     */
    Result<std::map<DateTime, DataFrame>> groupby_period(Frequency freq) const {
        std::map<DateTime, std::vector<size_type>> groups;

        for (size_type i = 0; i < size(); ++i) {
            DateTime period_start = get_period_start(index_[i], freq);
            groups[period_start].push_back(i);
        }

        std::map<DateTime, DataFrame> result;

        for (const auto& [period_start, indices] : groups) {
            std::vector<DateTime> group_index;
            group_index.reserve(indices.size());

            for (size_type idx : indices) {
                group_index.push_back(index_[idx]);
            }

            DataFrame group_df{group_index};

            for (const auto& name : column_names_) {
                std::visit(
                    [&](const auto& column_data) {
                        using T = typename std::decay_t<decltype(column_data)>::value_type;
                        std::vector<T> group_data;
                        group_data.reserve(indices.size());

                        for (size_type idx : indices) {
                            group_data.push_back(column_data[idx]);
                        }

                        group_df.add_column(name, group_data);
                    },
                    columns_.at(name));
            }

            result[period_start] = std::move(group_df);
        }

        return Result<std::map<DateTime, DataFrame>>::success(std::move(result));
    }

    /**
     * @brief Calculate basic statistics for numeric columns
     */
    Result<DataFrame> describe() const {
        std::vector<std::string> stats_names = {"count", "mean", "std", "min", "max"};
        std::vector<DateTime> stats_index;
        for (const auto& name : stats_names) {
            stats_index.push_back(DateTime::parse(name, "%s").value_or(DateTime::now()));
        }

        DataFrame stats{stats_index};

        for (const auto& column_name : column_names_) {
            std::visit(
                [&](const auto& column_data) {
                    using T = typename std::decay_t<decltype(column_data)>::value_type;

                    if constexpr (std::is_arithmetic_v<T>) {
                        std::vector<double> column_stats;

                        // Count
                        column_stats.push_back(static_cast<double>(column_data.size()));

                        // Mean
                        double sum  = std::accumulate(column_data.begin(), column_data.end(), 0.0);
                        double mean = sum / column_data.size();
                        column_stats.push_back(mean);

                        // Standard deviation
                        double sq_sum =
                            std::accumulate(column_data.begin(), column_data.end(), 0.0,
                                            [mean](double acc, T val) { return acc + (val - mean) * (val - mean); });
                        double std_dev = std::sqrt(sq_sum / column_data.size());
                        column_stats.push_back(std_dev);

                        // Min/Max
                        auto [min_it, max_it] = std::minmax_element(column_data.begin(), column_data.end());
                        column_stats.push_back(static_cast<double>(*min_it));
                        column_stats.push_back(static_cast<double>(*max_it));

                        stats.add_column(column_name, column_stats);
                    }
                },
                columns_.at(column_name));
        }

        return Result<DataFrame>::success(std::move(stats));
    }

    /**
     * @brief Calculate correlation matrix
     */
    Result<DataFrame> corr() const {
        std::vector<std::string> numeric_columns;

        // Find numeric columns
        for (const auto& name : column_names_) {
            std::visit(
                [&](const auto& column_data) {
                    using T = typename std::decay_t<decltype(column_data)>::value_type;
                    if constexpr (std::is_arithmetic_v<T>) {
                        numeric_columns.push_back(name);
                    }
                },
                columns_.at(name));
        }

        if (numeric_columns.empty()) {
            return Result<DataFrame>::error(ErrorCode::InvalidInput,
                                            "No numeric columns found for correlation calculation");
        }

        // Create correlation matrix index (use column names as dates for simplicity)
        std::vector<DateTime> corr_index;
        for (const auto& name : numeric_columns) {
            corr_index.push_back(DateTime::now());
        }

        DataFrame corr_df{corr_index};

        for (const auto& col1 : numeric_columns) {
            std::vector<double> corr_column;

            for (const auto& col2 : numeric_columns) {
                double correlation = calculate_correlation(col1, col2);
                corr_column.push_back(correlation);
            }

            corr_df.add_column(col1, corr_column);
        }

        return Result<DataFrame>::success(std::move(corr_df));
    }

  private:
    void validate_consistency() const {
        for (const auto& [name, data] : columns_) {
            std::visit(
                [this, &name](const auto& column_data) {
                    if (column_data.size() != index_.size()) {
                        throw std::runtime_error("Column '" + name + "' size mismatch with index");
                    }
                },
                data);
        }
    }

    DateTime get_period_start(const DateTime& timestamp, Frequency freq) const {
        // Simplified period calculation (same as TimeSeries)
        auto date = timestamp.to_date();

        switch (freq) {
            case Frequency::Monthly:
                return DateTime{date.year() / date.month() / std::chrono::day{1}};
            case Frequency::Quarterly: {
                auto month               = date.month();
                auto quarter_start_month = ((static_cast<unsigned>(month) - 1) / 3) * 3 + 1;
                return DateTime{date.year() / std::chrono::month{quarter_start_month} / std::chrono::day{1}};
            }
            case Frequency::Yearly:
                return DateTime{date.year() / std::chrono::January / std::chrono::day{1}};
            default:
                return timestamp;
        }
    }

    double calculate_correlation(const std::string& col1, const std::string& col2) const {
        // Simplified correlation calculation
        if (col1 == col2)
            return 1.0;

        // Would implement proper correlation calculation here
        // For now, return a placeholder
        return 0.0;
    }
};

}  // namespace pyfolio