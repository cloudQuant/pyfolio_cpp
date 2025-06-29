#pragma once

/**
 * @file model_persistence.h
 * @brief Machine learning model persistence and versioning system
 * @version 1.0.0
 * @date 2024
 * 
 * @section overview Overview
 * This module provides a comprehensive framework for saving, loading, and versioning
 * machine learning models used in financial analytics:
 * - Model serialization/deserialization with multiple formats
 * - Version control with metadata tracking
 * - Model registry for centralized management
 * - Automatic model comparison and validation
 * - Cloud storage integration (S3, Azure Blob, GCS)
 * - Model deployment and rollback capabilities
 * - Performance tracking across versions
 * 
 * @section features Key Features
 * - **Multiple Formats**: Binary, JSON, HDF5, ONNX support
 * - **Version Control**: Git-like versioning with tags and branches
 * - **Model Registry**: Centralized model catalog with search
 * - **Validation**: Automatic validation on load/save
 * - **Cloud Integration**: S3, Azure, GCS backends
 * - **Deployment**: Model serving and A/B testing support
 * - **Monitoring**: Performance tracking and drift detection
 * 
 * @section usage Usage Example
 * @code{.cpp}
 * #include <pyfolio/ml/model_persistence.h>
 * 
 * using namespace pyfolio::ml;
 * 
 * // Save a model
 * ModelSerializer serializer;
 * auto save_result = serializer.save_model(my_model, "models/regime_detector_v1.0");
 * 
 * // Load a model with version
 * ModelLoader loader;
 * auto model_result = loader.load_model<RegimeDetector>("models/regime_detector", "v1.0");
 * 
 * // Model registry
 * ModelRegistry registry("./model_registry");
 * registry.register_model(my_model, "regime_detector", "production");
 * 
 * // Version comparison
 * ModelComparator comparator;
 * auto diff = comparator.compare_versions("v1.0", "v2.0");
 * @endcode
 */

#include "../core/error_handling.h"
#include "../core/types.h"
#include "../core/datetime.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <chrono>
#include <variant>
#include <optional>
#include <fstream>
#include <sstream>

namespace pyfolio::ml {

/**
 * @brief Model format types
 */
enum class ModelFormat {
    Binary,      ///< Native binary format (fastest)
    JSON,        ///< JSON format (human-readable)
    HDF5,        ///< HDF5 format (large models)
    ONNX,        ///< ONNX format (interoperable)
    MessagePack, ///< MessagePack format (compact)
    Protobuf     ///< Protocol Buffers (efficient)
};

/**
 * @brief Storage backend types
 */
enum class StorageBackend {
    LocalFile,   ///< Local filesystem
    S3,          ///< Amazon S3
    AzureBlob,   ///< Azure Blob Storage
    GCS,         ///< Google Cloud Storage
    MongoDB,     ///< MongoDB GridFS
    Redis        ///< Redis (for small models)
};

/**
 * @brief Model metadata
 */
struct ModelMetadata {
    std::string model_id;           ///< Unique model identifier
    std::string name;               ///< Model name
    std::string version;            ///< Version string (e.g., "v1.2.3")
    std::string type;               ///< Model type (e.g., "RegimeDetector")
    std::string description;        ///< Model description
    
    DateTime created_at;            ///< Creation timestamp
    DateTime modified_at;           ///< Last modification timestamp
    std::string author;             ///< Model author/creator
    
    std::unordered_map<std::string, std::string> tags;  ///< Custom tags
    std::unordered_map<std::string, double> metrics;    ///< Performance metrics
    
    size_t model_size_bytes;        ///< Serialized model size
    std::string checksum;           ///< SHA256 checksum
    
    // Training information
    size_t training_samples;        ///< Number of training samples
    double training_time_seconds;   ///< Training duration
    std::string framework;          ///< ML framework used
    
    // Deployment information
    bool is_production;             ///< Production flag
    std::string deployment_env;     ///< Target deployment environment
    
    ModelMetadata() : model_size_bytes(0), training_samples(0), 
                     training_time_seconds(0.0), is_production(false) {}
};

/**
 * @brief Model version information
 */
struct ModelVersion {
    std::string version;            ///< Version identifier
    std::string parent_version;     ///< Parent version (for branching)
    DateTime timestamp;             ///< Version creation time
    std::string commit_message;     ///< Version description
    std::vector<std::string> changes;  ///< List of changes
    
    ModelVersion() = default;
    ModelVersion(const std::string& ver, const std::string& msg) 
        : version(ver), commit_message(msg), timestamp(DateTime()) {}
};

/**
 * @brief Model comparison result
 */
struct ModelDiff {
    std::string model_id;
    std::string version1;
    std::string version2;
    
    // Structural differences
    std::vector<std::string> added_layers;
    std::vector<std::string> removed_layers;
    std::vector<std::string> modified_layers;
    
    // Parameter differences
    size_t total_params_v1;
    size_t total_params_v2;
    double param_change_percentage;
    
    // Performance differences
    std::unordered_map<std::string, double> metric_changes;
    
    // Size differences
    size_t size_v1_bytes;
    size_t size_v2_bytes;
    
    ModelDiff() : total_params_v1(0), total_params_v2(0), 
                 param_change_percentage(0.0), size_v1_bytes(0), size_v2_bytes(0) {}
};

/**
 * @brief Base class for serializable models
 */
class SerializableModel {
public:
    virtual ~SerializableModel() = default;
    
    /**
     * @brief Serialize model to bytes
     * @return Serialized model data
     */
    [[nodiscard]] virtual Result<std::vector<uint8_t>> serialize() const = 0;
    
    /**
     * @brief Deserialize model from bytes
     * @param data Serialized model data
     * @return Success or error
     */
    [[nodiscard]] virtual Result<void> deserialize(const std::vector<uint8_t>& data) = 0;
    
    /**
     * @brief Get model type identifier
     * @return Model type string
     */
    [[nodiscard]] virtual std::string get_model_type() const = 0;
    
    /**
     * @brief Get model metadata
     * @return Model metadata
     */
    [[nodiscard]] virtual ModelMetadata get_metadata() const = 0;
    
    /**
     * @brief Validate model integrity
     * @return Validation result
     */
    [[nodiscard]] virtual Result<void> validate() const = 0;
};

/**
 * @brief Model serializer with format support
 */
class ModelSerializer {
public:
    /**
     * @brief Constructor
     * @param format Serialization format
     */
    explicit ModelSerializer(ModelFormat format = ModelFormat::Binary);
    
    /**
     * @brief Save model to file
     * @param model Model to save
     * @param path File path
     * @param metadata Additional metadata
     * @return Success or error
     */
    [[nodiscard]] Result<void> save_model(const SerializableModel& model,
                                         const std::filesystem::path& path,
                                         const ModelMetadata& metadata = {});
    
    /**
     * @brief Save model to storage backend
     * @param model Model to save
     * @param key Storage key
     * @param backend Storage backend
     * @param metadata Additional metadata
     * @return Success or error
     */
    [[nodiscard]] Result<void> save_to_backend(const SerializableModel& model,
                                              const std::string& key,
                                              StorageBackend backend,
                                              const ModelMetadata& metadata = {});
    
    /**
     * @brief Serialize model to bytes
     * @param model Model to serialize
     * @return Serialized data
     */
    [[nodiscard]] Result<std::vector<uint8_t>> serialize_to_bytes(
        const SerializableModel& model) const;
    
    /**
     * @brief Serialize model to string
     * @param model Model to serialize
     * @return Serialized string
     */
    [[nodiscard]] Result<std::string> serialize_to_string(
        const SerializableModel& model) const;
    
    /**
     * @brief Set compression level (0-9)
     * @param level Compression level
     */
    void set_compression_level(int level) { compression_level_ = level; }
    
    /**
     * @brief Enable encryption
     * @param key Encryption key
     */
    void enable_encryption(const std::string& key) { 
        encryption_enabled_ = true;
        encryption_key_ = key;
    }

private:
    ModelFormat format_;
    int compression_level_;
    bool encryption_enabled_;
    std::string encryption_key_;
    
    // Format-specific serialization
    [[nodiscard]] Result<std::vector<uint8_t>> serialize_binary(
        const SerializableModel& model) const;
    [[nodiscard]] Result<std::vector<uint8_t>> serialize_json(
        const SerializableModel& model) const;
    [[nodiscard]] Result<std::vector<uint8_t>> serialize_hdf5(
        const SerializableModel& model) const;
    [[nodiscard]] Result<std::vector<uint8_t>> serialize_onnx(
        const SerializableModel& model) const;
    
    // Compression and encryption
    [[nodiscard]] Result<std::vector<uint8_t>> compress_data(
        const std::vector<uint8_t>& data) const;
    [[nodiscard]] Result<std::vector<uint8_t>> encrypt_data(
        const std::vector<uint8_t>& data) const;
};

/**
 * @brief Model loader with format detection
 */
class ModelLoader {
public:
    /**
     * @brief Constructor
     */
    ModelLoader();
    
    /**
     * @brief Load model from file
     * @tparam T Model type
     * @param path File path
     * @return Loaded model
     */
    template<typename T>
    [[nodiscard]] Result<std::unique_ptr<T>> load_model(
        const std::filesystem::path& path) {
        auto data_result = load_file_data(path);
        if (data_result.is_error()) {
            return Result<std::unique_ptr<T>>::error(
                data_result.error().code, data_result.error().message);
        }
        
        auto model = std::make_unique<T>();
        auto deserialize_result = model->deserialize(data_result.value());
        if (deserialize_result.is_error()) {
            return Result<std::unique_ptr<T>>::error(
                deserialize_result.error().code, deserialize_result.error().message);
        }
        
        return Result<std::unique_ptr<T>>::success(std::move(model));
    }
    
    /**
     * @brief Load model from storage backend
     * @tparam T Model type
     * @param key Storage key
     * @param backend Storage backend
     * @return Loaded model
     */
    template<typename T>
    [[nodiscard]] Result<std::unique_ptr<T>> load_from_backend(
        const std::string& key, StorageBackend backend) {
        auto data_result = load_backend_data(key, backend);
        if (data_result.is_error()) {
            return Result<std::unique_ptr<T>>::error(
                data_result.error().code, data_result.error().message);
        }
        
        auto model = std::make_unique<T>();
        auto deserialize_result = model->deserialize(data_result.value());
        if (deserialize_result.is_error()) {
            return Result<std::unique_ptr<T>>::error(
                deserialize_result.error().code, deserialize_result.error().message);
        }
        
        return Result<std::unique_ptr<T>>::success(std::move(model));
    }
    
    /**
     * @brief Load model metadata without loading full model
     * @param path File path
     * @return Model metadata
     */
    [[nodiscard]] Result<ModelMetadata> load_metadata(
        const std::filesystem::path& path) const;
    
    /**
     * @brief Detect model format from file
     * @param path File path
     * @return Detected format
     */
    [[nodiscard]] Result<ModelFormat> detect_format(
        const std::filesystem::path& path) const;
    
    /**
     * @brief Set decryption key
     * @param key Decryption key
     */
    void set_decryption_key(const std::string& key) { decryption_key_ = key; }

private:
    std::string decryption_key_;
    
    [[nodiscard]] Result<std::vector<uint8_t>> load_file_data(
        const std::filesystem::path& path) const;
    [[nodiscard]] Result<std::vector<uint8_t>> load_backend_data(
        const std::string& key, StorageBackend backend) const;
    
    [[nodiscard]] Result<std::vector<uint8_t>> decompress_data(
        const std::vector<uint8_t>& data) const;
    [[nodiscard]] Result<std::vector<uint8_t>> decrypt_data(
        const std::vector<uint8_t>& data) const;
};

/**
 * @brief Model version control system
 */
class ModelVersionControl {
public:
    /**
     * @brief Constructor
     * @param repository_path Repository path
     */
    explicit ModelVersionControl(const std::filesystem::path& repository_path);
    
    /**
     * @brief Initialize new repository
     * @return Success or error
     */
    [[nodiscard]] Result<void> init_repository();
    
    /**
     * @brief Commit model version
     * @param model Model to commit
     * @param message Commit message
     * @param tags Optional tags
     * @return Version identifier
     */
    [[nodiscard]] Result<std::string> commit_model(
        const SerializableModel& model,
        const std::string& message,
        const std::vector<std::string>& tags = {});
    
    /**
     * @brief Checkout model version
     * @tparam T Model type
     * @param version Version identifier
     * @return Model at specified version
     */
    template<typename T>
    [[nodiscard]] Result<std::unique_ptr<T>> checkout(const std::string& version) {
        auto path_result = get_version_path(version);
        if (path_result.is_error()) {
            return Result<std::unique_ptr<T>>::error(
                path_result.error().code, path_result.error().message);
        }
        
        ModelLoader loader;
        return loader.load_model<T>(path_result.value());
    }
    
    /**
     * @brief Create new branch
     * @param branch_name Branch name
     * @param from_version Base version
     * @return Success or error
     */
    [[nodiscard]] Result<void> create_branch(const std::string& branch_name,
                                           const std::string& from_version = "main");
    
    /**
     * @brief Switch to branch
     * @param branch_name Branch name
     * @return Success or error
     */
    [[nodiscard]] Result<void> switch_branch(const std::string& branch_name);
    
    /**
     * @brief Merge branches
     * @param source_branch Source branch
     * @param target_branch Target branch
     * @return Merge result
     */
    [[nodiscard]] Result<std::string> merge_branches(const std::string& source_branch,
                                                    const std::string& target_branch);
    
    /**
     * @brief Tag version
     * @param version Version to tag
     * @param tag_name Tag name
     * @return Success or error
     */
    [[nodiscard]] Result<void> tag_version(const std::string& version,
                                         const std::string& tag_name);
    
    /**
     * @brief Get version history
     * @param branch Branch name (default: current)
     * @param limit Maximum entries
     * @return Version history
     */
    [[nodiscard]] Result<std::vector<ModelVersion>> get_history(
        const std::string& branch = "", size_t limit = 100) const;
    
    /**
     * @brief Compare two versions
     * @param version1 First version
     * @param version2 Second version
     * @return Comparison result
     */
    [[nodiscard]] Result<ModelDiff> compare_versions(const std::string& version1,
                                                    const std::string& version2) const;
    
    /**
     * @brief Rollback to previous version
     * @param steps Number of versions to rollback
     * @return New current version
     */
    [[nodiscard]] Result<std::string> rollback(size_t steps = 1);
    
    /**
     * @brief Get current version
     * @return Current version identifier
     */
    [[nodiscard]] std::string get_current_version() const { return current_version_; }
    
    /**
     * @brief Get current branch
     * @return Current branch name
     */
    [[nodiscard]] std::string get_current_branch() const { return current_branch_; }

private:
    std::filesystem::path repository_path_;
    std::string current_branch_;
    std::string current_version_;
    
    // Version management
    [[nodiscard]] Result<std::filesystem::path> get_version_path(
        const std::string& version) const;
    [[nodiscard]] Result<std::string> generate_version_id() const;
    [[nodiscard]] Result<void> update_refs(const std::string& branch,
                                         const std::string& version);
    [[nodiscard]] Result<void> write_version_metadata(const std::string& version,
                                                    const ModelVersion& metadata);
};

/**
 * @brief Centralized model registry
 */
class ModelRegistry {
public:
    /**
     * @brief Constructor
     * @param registry_path Registry storage path
     */
    explicit ModelRegistry(const std::filesystem::path& registry_path);
    
    /**
     * @brief Register new model
     * @param model Model to register
     * @param name Model name
     * @param tags Tags for categorization
     * @return Model ID
     */
    [[nodiscard]] Result<std::string> register_model(const SerializableModel& model,
                                                    const std::string& name,
                                                    const std::vector<std::string>& tags = {});
    
    /**
     * @brief Update model in registry
     * @param model_id Model ID
     * @param model Updated model
     * @param update_message Update description
     * @return New version ID
     */
    [[nodiscard]] Result<std::string> update_model(const std::string& model_id,
                                                  const SerializableModel& model,
                                                  const std::string& update_message);
    
    /**
     * @brief Get model by ID
     * @tparam T Model type
     * @param model_id Model ID
     * @param version Version (latest if empty)
     * @return Model instance
     */
    template<typename T>
    [[nodiscard]] Result<std::unique_ptr<T>> get_model(
        const std::string& model_id, const std::string& version = "") {
        auto path_result = get_model_path(model_id, version);
        if (path_result.is_error()) {
            return Result<std::unique_ptr<T>>::error(
                path_result.error().code, path_result.error().message);
        }
        
        ModelLoader loader;
        return loader.load_model<T>(path_result.value());
    }
    
    /**
     * @brief Search models by criteria
     * @param query Search query
     * @param tags Filter by tags
     * @param model_type Filter by type
     * @return Matching model IDs
     */
    [[nodiscard]] Result<std::vector<std::string>> search_models(
        const std::string& query = "",
        const std::vector<std::string>& tags = {},
        const std::string& model_type = "") const;
    
    /**
     * @brief Get model metadata
     * @param model_id Model ID
     * @param version Version (latest if empty)
     * @return Model metadata
     */
    [[nodiscard]] Result<ModelMetadata> get_model_metadata(
        const std::string& model_id, const std::string& version = "") const;
    
    /**
     * @brief List all versions of a model
     * @param model_id Model ID
     * @return Version list
     */
    [[nodiscard]] Result<std::vector<std::string>> list_versions(
        const std::string& model_id) const;
    
    /**
     * @brief Promote model to production
     * @param model_id Model ID
     * @param version Version to promote
     * @return Success or error
     */
    [[nodiscard]] Result<void> promote_to_production(const std::string& model_id,
                                                    const std::string& version);
    
    /**
     * @brief Archive model
     * @param model_id Model ID
     * @return Success or error
     */
    [[nodiscard]] Result<void> archive_model(const std::string& model_id);
    
    /**
     * @brief Delete model from registry
     * @param model_id Model ID
     * @param version Specific version (all if empty)
     * @return Success or error
     */
    [[nodiscard]] Result<void> delete_model(const std::string& model_id,
                                          const std::string& version = "");
    
    /**
     * @brief Export registry catalog
     * @param format Export format
     * @return Catalog data
     */
    [[nodiscard]] Result<std::string> export_catalog(const std::string& format = "json") const;

private:
    std::filesystem::path registry_path_;
    std::unordered_map<std::string, ModelMetadata> model_index_;
    
    [[nodiscard]] Result<std::filesystem::path> get_model_path(
        const std::string& model_id, const std::string& version) const;
    [[nodiscard]] Result<void> update_index();
    [[nodiscard]] Result<void> save_index() const;
};

/**
 * @brief Model deployment manager
 */
class ModelDeploymentManager {
public:
    /**
     * @brief Constructor
     */
    ModelDeploymentManager();
    
    /**
     * @brief Deploy model to environment
     * @param model_id Model ID from registry
     * @param version Model version
     * @param environment Target environment
     * @param config Deployment configuration
     * @return Deployment ID
     */
    [[nodiscard]] Result<std::string> deploy_model(
        const std::string& model_id,
        const std::string& version,
        const std::string& environment,
        const std::unordered_map<std::string, std::string>& config = {});
    
    /**
     * @brief Rollback deployment
     * @param deployment_id Deployment ID
     * @return Success or error
     */
    [[nodiscard]] Result<void> rollback_deployment(const std::string& deployment_id);
    
    /**
     * @brief Get deployment status
     * @param deployment_id Deployment ID
     * @return Status information
     */
    [[nodiscard]] Result<std::unordered_map<std::string, std::string>> get_deployment_status(
        const std::string& deployment_id) const;
    
    /**
     * @brief Setup A/B test
     * @param model_a_id Model A ID
     * @param model_b_id Model B ID
     * @param traffic_split Traffic split (0-1 for model A)
     * @param environment Target environment
     * @return A/B test ID
     */
    [[nodiscard]] Result<std::string> setup_ab_test(
        const std::string& model_a_id,
        const std::string& model_b_id,
        double traffic_split,
        const std::string& environment);
    
    /**
     * @brief Monitor model performance
     * @param deployment_id Deployment ID
     * @param metric_name Metric to monitor
     * @param time_window Time window in seconds
     * @return Metric values
     */
    [[nodiscard]] Result<std::vector<double>> monitor_performance(
        const std::string& deployment_id,
        const std::string& metric_name,
        size_t time_window = 3600) const;

private:
    struct DeploymentInfo {
        std::string deployment_id;
        std::string model_id;
        std::string version;
        std::string environment;
        DateTime deployed_at;
        std::unordered_map<std::string, std::string> config;
        bool is_active;
    };
    
    std::unordered_map<std::string, DeploymentInfo> deployments_;
    std::unordered_map<std::string, std::vector<std::pair<DateTime, double>>> metrics_;
};

/**
 * @brief Model performance tracker
 */
class ModelPerformanceTracker {
public:
    /**
     * @brief Constructor
     */
    ModelPerformanceTracker();
    
    /**
     * @brief Record prediction
     * @param model_id Model ID
     * @param prediction Predicted value
     * @param actual Actual value
     * @param timestamp Prediction timestamp
     */
    void record_prediction(const std::string& model_id,
                          double prediction,
                          double actual,
                          const DateTime& timestamp = DateTime());
    
    /**
     * @brief Record batch predictions
     * @param model_id Model ID
     * @param predictions Predicted values
     * @param actuals Actual values
     * @param timestamp Batch timestamp
     */
    void record_batch_predictions(const std::string& model_id,
                                 const std::vector<double>& predictions,
                                 const std::vector<double>& actuals,
                                 const DateTime& timestamp = DateTime());
    
    /**
     * @brief Calculate performance metrics
     * @param model_id Model ID
     * @param start_time Start of evaluation period
     * @param end_time End of evaluation period
     * @return Performance metrics
     */
    [[nodiscard]] Result<std::unordered_map<std::string, double>> calculate_metrics(
        const std::string& model_id,
        const DateTime& start_time = DateTime(),
        const DateTime& end_time = DateTime()) const;
    
    /**
     * @brief Detect performance drift
     * @param model_id Model ID
     * @param baseline_window Baseline window size
     * @param current_window Current window size
     * @param threshold Drift threshold
     * @return Drift detection result
     */
    [[nodiscard]] Result<bool> detect_drift(const std::string& model_id,
                                           size_t baseline_window = 1000,
                                           size_t current_window = 100,
                                           double threshold = 0.1) const;
    
    /**
     * @brief Compare model performances
     * @param model_ids List of model IDs
     * @param metric Metric to compare
     * @param period Time period in seconds
     * @return Comparison results
     */
    [[nodiscard]] Result<std::unordered_map<std::string, double>> compare_models(
        const std::vector<std::string>& model_ids,
        const std::string& metric = "rmse",
        size_t period = 86400) const;
    
    /**
     * @brief Generate performance report
     * @param model_id Model ID
     * @param format Report format
     * @return Performance report
     */
    [[nodiscard]] Result<std::string> generate_report(
        const std::string& model_id,
        const std::string& format = "json") const;

private:
    struct PredictionRecord {
        DateTime timestamp;
        double prediction;
        double actual;
    };
    
    std::unordered_map<std::string, std::vector<PredictionRecord>> predictions_;
    
    [[nodiscard]] double calculate_rmse(const std::vector<PredictionRecord>& records) const;
    [[nodiscard]] double calculate_mae(const std::vector<PredictionRecord>& records) const;
    [[nodiscard]] double calculate_mape(const std::vector<PredictionRecord>& records) const;
    [[nodiscard]] double calculate_r2(const std::vector<PredictionRecord>& records) const;
};

} // namespace pyfolio::ml