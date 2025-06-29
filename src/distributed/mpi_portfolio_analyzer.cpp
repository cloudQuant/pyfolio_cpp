#include "pyfolio/distributed/mpi_portfolio_analyzer.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <iostream>

namespace pyfolio::distributed {

// Static method implementations that couldn't be defined in header

/**
 * @brief Advanced data partitioning strategy
 */
class DataPartitioner {
public:
    enum class PartitionStrategy {
        Temporal,     // Partition by time periods
        Symbols,      // Partition by asset symbols
        Hybrid,       // Combination of temporal and symbol partitioning
        Balanced      // Load-balanced partitioning
    };
    
    struct PartitionPlan {
        PartitionStrategy strategy;
        std::vector<std::pair<size_t, size_t>> time_ranges;    // start_idx, end_idx
        std::vector<std::vector<std::string>> symbol_groups;
        std::vector<double> computational_loads;               // Estimated load per partition
        std::vector<int> node_assignments;                     // Which node handles each partition
    };
    
    static Result<PartitionPlan> create_partition_plan(
        const std::unordered_map<std::string, TimeSeries<Price>>& price_data,
        int num_nodes,
        PartitionStrategy strategy = PartitionStrategy::Balanced) {
        
        PartitionPlan plan;
        plan.strategy = strategy;
        
        // Extract basic information
        std::vector<std::string> all_symbols;
        size_t max_data_points = 0;
        
        for (const auto& [symbol, prices] : price_data) {
            all_symbols.push_back(symbol);
            max_data_points = std::max(max_data_points, prices.size());
        }
        
        switch (strategy) {
            case PartitionStrategy::Temporal:
                return create_temporal_partition(all_symbols, max_data_points, num_nodes, plan);
            
            case PartitionStrategy::Symbols:
                return create_symbol_partition(all_symbols, max_data_points, num_nodes, plan);
            
            case PartitionStrategy::Hybrid:
                return create_hybrid_partition(all_symbols, max_data_points, num_nodes, plan);
            
            case PartitionStrategy::Balanced:
                return create_balanced_partition(price_data, num_nodes, plan);
        }
        
        return Result<PartitionPlan>::error(ErrorCode::InvalidInput, "Unknown partition strategy");
    }
    
private:
    static Result<PartitionPlan> create_temporal_partition(
        const std::vector<std::string>& symbols,
        size_t total_time_points,
        int num_nodes,
        PartitionPlan& plan) {
        
        size_t points_per_node = total_time_points / num_nodes;
        size_t remaining_points = total_time_points % num_nodes;
        
        size_t current_start = 0;
        for (int node = 0; node < num_nodes; ++node) {
            size_t node_points = points_per_node + (node < remaining_points ? 1 : 0);
            size_t current_end = current_start + node_points;
            
            plan.time_ranges.emplace_back(current_start, current_end);
            plan.symbol_groups.push_back(symbols); // All symbols on each node
            plan.computational_loads.push_back(symbols.size() * node_points);
            plan.node_assignments.push_back(node);
            
            current_start = current_end;
        }
        
        return Result<PartitionPlan>::success(std::move(plan));
    }
    
    static Result<PartitionPlan> create_symbol_partition(
        const std::vector<std::string>& symbols,
        size_t total_time_points,
        int num_nodes,
        PartitionPlan& plan) {
        
        size_t symbols_per_node = symbols.size() / num_nodes;
        size_t remaining_symbols = symbols.size() % num_nodes;
        
        size_t current_start = 0;
        for (int node = 0; node < num_nodes; ++node) {
            size_t node_symbols = symbols_per_node + (node < remaining_symbols ? 1 : 0);
            
            std::vector<std::string> node_symbol_group(
                symbols.begin() + current_start,
                symbols.begin() + current_start + node_symbols
            );
            
            plan.time_ranges.emplace_back(0, total_time_points); // Full time range on each node
            plan.symbol_groups.push_back(std::move(node_symbol_group));
            plan.computational_loads.push_back(node_symbols * total_time_points);
            plan.node_assignments.push_back(node);
            
            current_start += node_symbols;
        }
        
        return Result<PartitionPlan>::success(std::move(plan));
    }
    
    static Result<PartitionPlan> create_hybrid_partition(
        const std::vector<std::string>& symbols,
        size_t total_time_points,
        int num_nodes,
        PartitionPlan& plan) {
        
        // Create a 2D partitioning grid
        int time_partitions = static_cast<int>(std::sqrt(num_nodes));
        int symbol_partitions = num_nodes / time_partitions;
        
        size_t time_points_per_partition = total_time_points / time_partitions;
        size_t symbols_per_partition = symbols.size() / symbol_partitions;
        
        int node_id = 0;
        for (int t_part = 0; t_part < time_partitions && node_id < num_nodes; ++t_part) {
            size_t time_start = t_part * time_points_per_partition;
            size_t time_end = (t_part == time_partitions - 1) ? 
                             total_time_points : (t_part + 1) * time_points_per_partition;
            
            for (int s_part = 0; s_part < symbol_partitions && node_id < num_nodes; ++s_part) {
                size_t symbol_start = s_part * symbols_per_partition;
                size_t symbol_end = (s_part == symbol_partitions - 1) ? 
                                   symbols.size() : (s_part + 1) * symbols_per_partition;
                
                plan.time_ranges.emplace_back(time_start, time_end);
                
                std::vector<std::string> partition_symbols(
                    symbols.begin() + symbol_start,
                    symbols.begin() + symbol_end
                );
                plan.symbol_groups.push_back(std::move(partition_symbols));
                
                double load = (time_end - time_start) * (symbol_end - symbol_start);
                plan.computational_loads.push_back(load);
                plan.node_assignments.push_back(node_id);
                
                ++node_id;
            }
        }
        
        return Result<PartitionPlan>::success(std::move(plan));
    }
    
    static Result<PartitionPlan> create_balanced_partition(
        const std::unordered_map<std::string, TimeSeries<Price>>& price_data,
        int num_nodes,
        PartitionPlan& plan) {
        
        // Calculate workload for each symbol (based on data size and complexity)
        std::vector<std::pair<std::string, double>> symbol_workloads;
        
        for (const auto& [symbol, prices] : price_data) {
            double workload = prices.size() * 1.0; // Base workload
            
            // Add complexity factors
            if (prices.size() > 10000) workload *= 1.2;  // Large datasets
            if (symbol.find("ETF") != std::string::npos) workload *= 0.8;  // ETFs are simpler
            
            symbol_workloads.emplace_back(symbol, workload);
        }
        
        // Sort by workload (largest first for better load balancing)
        std::sort(symbol_workloads.begin(), symbol_workloads.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        // Initialize node loads
        std::vector<double> node_loads(num_nodes, 0.0);
        std::vector<std::vector<std::string>> node_symbols(num_nodes);
        
        // Greedy assignment to least loaded node
        for (const auto& [symbol, workload] : symbol_workloads) {
            auto min_it = std::min_element(node_loads.begin(), node_loads.end());
            int min_node = static_cast<int>(min_it - node_loads.begin());
            
            node_symbols[min_node].push_back(symbol);
            node_loads[min_node] += workload;
        }
        
        // Create partition plan
        size_t total_time_points = price_data.empty() ? 0 : price_data.begin()->second.size();
        
        for (int node = 0; node < num_nodes; ++node) {
            plan.time_ranges.emplace_back(0, total_time_points);
            plan.symbol_groups.push_back(std::move(node_symbols[node]));
            plan.computational_loads.push_back(node_loads[node]);
            plan.node_assignments.push_back(node);
        }
        
        return Result<PartitionPlan>::success(std::move(plan));
    }
};

/**
 * @brief MPI Communication utilities
 */
class MPICommunicator {
public:
    /**
     * @brief Broadcast TimeSeries data efficiently
     */
    static Result<void> broadcast_timeseries(
        TimeSeries<double>& timeseries,
        int root_rank) {
        
        int rank;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        
        // First broadcast the size
        size_t data_size = 0;
        if (rank == root_rank) {
            data_size = timeseries.size();
        }
        
        MPI_Bcast(&data_size, sizeof(size_t), MPI_BYTE, root_rank, MPI_COMM_WORLD);
        
        if (data_size == 0) {
            return Result<void>::success();
        }
        
        // Prepare data buffers
        std::vector<double> timestamps_buffer(data_size);
        std::vector<double> values_buffer(data_size);
        
        if (rank == root_rank) {
            // Convert DateTime to double for transmission
            const auto& timestamps = timeseries.timestamps();
            const auto& values = timeseries.values();
            
            for (size_t i = 0; i < data_size; ++i) {
                auto time_point = timestamps[i].time_point();
                auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                    time_point.time_since_epoch()).count();
                timestamps_buffer[i] = static_cast<double>(timestamp);
                values_buffer[i] = values[i];
            }
        }
        
        // Broadcast the data
        MPI_Bcast(timestamps_buffer.data(), data_size, MPI_DOUBLE, root_rank, MPI_COMM_WORLD);
        MPI_Bcast(values_buffer.data(), data_size, MPI_DOUBLE, root_rank, MPI_COMM_WORLD);
        
        // Reconstruct TimeSeries on non-root nodes
        if (rank != root_rank) {
            std::vector<DateTime> datetime_vec;
            datetime_vec.reserve(data_size);
            
            for (size_t i = 0; i < data_size; ++i) {
                auto time_point = std::chrono::system_clock::from_time_t(
                    static_cast<time_t>(timestamps_buffer[i]));
                datetime_vec.push_back(DateTime::from_time_point(time_point));
            }
            
            auto result = TimeSeries<double>::create(datetime_vec, values_buffer, timeseries.name());
            if (result.is_ok()) {
                timeseries = result.value();
            } else {
                return Result<void>::error(result.error().code, result.error().message);
            }
        }
        
        return Result<void>::success();
    }
    
    /**
     * @brief Gather variable-length data from all processes
     */
    template<typename T>
    static Result<std::vector<T>> gather_variable_data(const std::vector<T>& local_data) {
        int rank, size;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &size);
        
        // Gather sizes first
        int local_size = static_cast<int>(local_data.size());
        std::vector<int> all_sizes(size);
        
        MPI_Allgather(&local_size, 1, MPI_INT, all_sizes.data(), 1, MPI_INT, MPI_COMM_WORLD);
        
        // Calculate total size and displacements
        int total_size = 0;
        std::vector<int> displs(size);
        for (int i = 0; i < size; ++i) {
            displs[i] = total_size;
            total_size += all_sizes[i];
        }
        
        // Gather all data
        std::vector<T> all_data(total_size);
        
        MPI_Allgatherv(local_data.data(), local_size, get_mpi_type<T>(),
                      all_data.data(), all_sizes.data(), displs.data(), 
                      get_mpi_type<T>(), MPI_COMM_WORLD);
        
        return Result<std::vector<T>>::success(std::move(all_data));
    }
    
    /**
     * @brief Perform reduction operation across all processes
     */
    template<typename T>
    static Result<T> reduce_all(const T& local_value, MPI_Op operation) {
        T global_value;
        
        int result = MPI_Allreduce(&local_value, &global_value, 1, 
                                  get_mpi_type<T>(), operation, MPI_COMM_WORLD);
        
        if (result != MPI_SUCCESS) {
            return Result<T>::error(ErrorCode::NetworkError, "MPI reduction failed");
        }
        
        return Result<T>::success(global_value);
    }
    
private:
    template<typename T>
    static MPI_Datatype get_mpi_type() {
        if constexpr (std::is_same_v<T, double>) return MPI_DOUBLE;
        else if constexpr (std::is_same_v<T, float>) return MPI_FLOAT;
        else if constexpr (std::is_same_v<T, int>) return MPI_INT;
        else if constexpr (std::is_same_v<T, size_t>) return MPI_UNSIGNED_LONG;
        else return MPI_BYTE;
    }
};

/**
 * @brief Distributed statistical computations
 */
class DistributedStatistics {
public:
    /**
     * @brief Compute mean across distributed data
     */
    static Result<double> distributed_mean(const std::vector<double>& local_data) {
        double local_sum = std::accumulate(local_data.begin(), local_data.end(), 0.0);
        size_t local_count = local_data.size();
        
        // Reduce sums and counts
        auto global_sum_result = MPICommunicator::reduce_all(local_sum, MPI_SUM);
        auto global_count_result = MPICommunicator::reduce_all(local_count, MPI_SUM);
        
        if (global_sum_result.is_error() || global_count_result.is_error()) {
            return Result<double>::error(ErrorCode::NetworkError, "Failed to compute distributed mean");
        }
        
        if (global_count_result.value() == 0) {
            return Result<double>::error(ErrorCode::InvalidInput, "No data for mean calculation");
        }
        
        double global_mean = global_sum_result.value() / global_count_result.value();
        return Result<double>::success(global_mean);
    }
    
    /**
     * @brief Compute variance across distributed data
     */
    static Result<double> distributed_variance(const std::vector<double>& local_data) {
        // First compute global mean
        auto mean_result = distributed_mean(local_data);
        if (mean_result.is_error()) {
            return Result<double>::error(mean_result.error().code, mean_result.error().message);
        }
        
        double global_mean = mean_result.value();
        
        // Compute local sum of squared differences
        double local_sq_sum = 0.0;
        for (double value : local_data) {
            double diff = value - global_mean;
            local_sq_sum += diff * diff;
        }
        
        size_t local_count = local_data.size();
        
        // Reduce squared sums and counts
        auto global_sq_sum_result = MPICommunicator::reduce_all(local_sq_sum, MPI_SUM);
        auto global_count_result = MPICommunicator::reduce_all(local_count, MPI_SUM);
        
        if (global_sq_sum_result.is_error() || global_count_result.is_error()) {
            return Result<double>::error(ErrorCode::NetworkError, "Failed to compute distributed variance");
        }
        
        if (global_count_result.value() <= 1) {
            return Result<double>::error(ErrorCode::InvalidInput, "Insufficient data for variance calculation");
        }
        
        double global_variance = global_sq_sum_result.value() / (global_count_result.value() - 1);
        return Result<double>::success(global_variance);
    }
    
    /**
     * @brief Compute covariance matrix across distributed data
     */
    static Result<std::vector<std::vector<double>>> distributed_covariance_matrix(
        const std::vector<std::vector<double>>& local_data) {
        
        if (local_data.empty()) {
            return Result<std::vector<std::vector<double>>>::error(
                ErrorCode::InvalidInput, "Empty local data");
        }
        
        size_t n_assets = local_data[0].size();
        size_t n_observations = local_data.size();
        
        // Compute means for each asset
        std::vector<double> asset_means(n_assets, 0.0);
        for (size_t asset = 0; asset < n_assets; ++asset) {
            std::vector<double> asset_data;
            asset_data.reserve(n_observations);
            
            for (size_t obs = 0; obs < n_observations; ++obs) {
                asset_data.push_back(local_data[obs][asset]);
            }
            
            auto mean_result = distributed_mean(asset_data);
            if (mean_result.is_error()) {
                return Result<std::vector<std::vector<double>>>::error(
                    mean_result.error().code, mean_result.error().message);
            }
            asset_means[asset] = mean_result.value();
        }
        
        // Compute covariance matrix elements
        std::vector<std::vector<double>> covar_matrix(n_assets, std::vector<double>(n_assets, 0.0));
        
        for (size_t i = 0; i < n_assets; ++i) {
            for (size_t j = i; j < n_assets; ++j) {
                double local_covar_sum = 0.0;
                
                for (size_t obs = 0; obs < n_observations; ++obs) {
                    double diff_i = local_data[obs][i] - asset_means[i];
                    double diff_j = local_data[obs][j] - asset_means[j];
                    local_covar_sum += diff_i * diff_j;
                }
                
                auto global_covar_result = MPICommunicator::reduce_all(local_covar_sum, MPI_SUM);
                auto global_count_result = MPICommunicator::reduce_all(n_observations, MPI_SUM);
                
                if (global_covar_result.is_error() || global_count_result.is_error()) {
                    return Result<std::vector<std::vector<double>>>::error(
                        ErrorCode::NetworkError, "Failed to compute covariance");
                }
                
                double covariance = global_covar_result.value() / (global_count_result.value() - 1);
                covar_matrix[i][j] = covariance;
                covar_matrix[j][i] = covariance; // Symmetric matrix
            }
        }
        
        return Result<std::vector<std::vector<double>>>::success(std::move(covar_matrix));
    }
};

} // namespace pyfolio::distributed