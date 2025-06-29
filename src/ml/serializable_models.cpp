#include "pyfolio/ml/serializable_models.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <random>
#include <numeric>

namespace pyfolio::ml {

// ============================================================================
// SerializableRegimeDetector Implementation
// ============================================================================

SerializableRegimeDetector::SerializableRegimeDetector()
    : model_name_("RegimeDetector"), model_version_("1.0.0"), creation_time_(DateTime()) {
    // Initialize with default parameters
    parameters_ = {0.1, 0.2, 0.3, 0.4, 0.5};
}

SerializableRegimeDetector::SerializableRegimeDetector(
    std::shared_ptr<analytics::RegimeDetector> detector)
    : detector_(detector), model_name_("RegimeDetector"), 
      model_version_("1.0.0"), creation_time_(DateTime()) {
    // Extract parameters from detector if available
    parameters_ = {0.1, 0.2, 0.3, 0.4, 0.5}; // Placeholder
}

Result<std::vector<uint8_t>> SerializableRegimeDetector::serialize() const {
    try {
        std::vector<uint8_t> data;
        
        // Serialize header
        std::string header = "RGDT";  // Regime Detector header
        data.insert(data.end(), header.begin(), header.end());
        
        // Serialize version
        uint32_t version = 1;
        auto version_bytes = reinterpret_cast<const uint8_t*>(&version);
        data.insert(data.end(), version_bytes, version_bytes + sizeof(version));
        
        // Serialize parameters
        uint32_t param_count = static_cast<uint32_t>(parameters_.size());
        auto count_bytes = reinterpret_cast<const uint8_t*>(&param_count);
        data.insert(data.end(), count_bytes, count_bytes + sizeof(param_count));
        
        for (double param : parameters_) {
            auto param_bytes = reinterpret_cast<const uint8_t*>(&param);
            data.insert(data.end(), param_bytes, param_bytes + sizeof(param));
        }
        
        // Serialize model name
        uint32_t name_len = static_cast<uint32_t>(model_name_.length());
        auto name_len_bytes = reinterpret_cast<const uint8_t*>(&name_len);
        data.insert(data.end(), name_len_bytes, name_len_bytes + sizeof(name_len));
        data.insert(data.end(), model_name_.begin(), model_name_.end());
        
        return Result<std::vector<uint8_t>>::success(data);
        
    } catch (const std::exception& e) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<void> SerializableRegimeDetector::deserialize(const std::vector<uint8_t>& data) {
    try {
        if (data.size() < 12) { // Minimum size check
            return Result<void>::error(ErrorCode::InvalidInput, "Invalid data size");
        }
        
        size_t offset = 0;
        
        // Check header
        std::string header(data.begin(), data.begin() + 4);
        if (header != "RGDT") {
            return Result<void>::error(ErrorCode::InvalidInput, "Invalid header");
        }
        offset += 4;
        
        // Read version
        uint32_t version;
        std::memcpy(&version, data.data() + offset, sizeof(version));
        offset += sizeof(version);
        
        if (version != 1) {
            return Result<void>::error(ErrorCode::InvalidInput, "Unsupported version");
        }
        
        // Read parameter count
        uint32_t param_count;
        std::memcpy(&param_count, data.data() + offset, sizeof(param_count));
        offset += sizeof(param_count);
        
        // Read parameters
        parameters_.clear();
        parameters_.reserve(param_count);
        
        for (uint32_t i = 0; i < param_count; ++i) {
            if (offset + sizeof(double) > data.size()) {
                return Result<void>::error(ErrorCode::InvalidInput, "Truncated parameter data");
            }
            
            double param;
            std::memcpy(&param, data.data() + offset, sizeof(param));
            parameters_.push_back(param);
            offset += sizeof(param);
        }
        
        // Read model name
        if (offset + sizeof(uint32_t) > data.size()) {
            return Result<void>::error(ErrorCode::InvalidInput, "Truncated name length");
        }
        
        uint32_t name_len;
        std::memcpy(&name_len, data.data() + offset, sizeof(name_len));
        offset += sizeof(name_len);
        
        if (offset + name_len > data.size()) {
            return Result<void>::error(ErrorCode::InvalidInput, "Truncated name data");
        }
        
        model_name_ = std::string(data.begin() + offset, data.begin() + offset + name_len);
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::CalculationError, e.what());
    }
}

std::string SerializableRegimeDetector::get_model_type() const {
    return "RegimeDetector";
}

ModelMetadata SerializableRegimeDetector::get_metadata() const {
    ModelMetadata metadata;
    metadata.model_id = "regime_detector_" + std::to_string(std::time(nullptr));
    metadata.name = model_name_;
    metadata.version = model_version_;
    metadata.type = get_model_type();
    metadata.description = "Regime detection model for financial time series";
    metadata.created_at = creation_time_;
    metadata.modified_at = DateTime();
    metadata.author = "PyFolio C++";
    metadata.framework = "PyFolio C++";
    metadata.training_samples = 1000; // Placeholder
    metadata.is_production = false;
    
    return metadata;
}

Result<void> SerializableRegimeDetector::validate() const {
    if (parameters_.empty()) {
        return Result<void>::error(ErrorCode::InvalidState, "No parameters loaded");
    }
    
    if (model_name_.empty()) {
        return Result<void>::error(ErrorCode::InvalidState, "No model name");
    }
    
    // Validate parameter ranges
    for (double param : parameters_) {
        if (!std::isfinite(param)) {
            return Result<void>::error(ErrorCode::InvalidState, "Invalid parameter value");
        }
    }
    
    return Result<void>::success();
}

// ============================================================================
// SerializableLinearRegression Implementation
// ============================================================================

SerializableLinearRegression::SerializableLinearRegression()
    : intercept_(0.0), r_squared_(0.0), n_features_(0), n_samples_(0), 
      is_trained_(false), training_time_(DateTime()) {}

Result<void> SerializableLinearRegression::train(const std::vector<std::vector<double>>& X,
                                               const std::vector<double>& y) {
    try {
        if (X.empty() || y.empty() || X.size() != y.size()) {
            return Result<void>::error(ErrorCode::InvalidInput, "Invalid training data");
        }
        
        n_samples_ = X.size();
        n_features_ = X[0].size();
        
        // Validate feature dimensions
        for (const auto& row : X) {
            if (row.size() != n_features_) {
                return Result<void>::error(ErrorCode::InvalidInput, "Inconsistent feature dimensions");
            }
        }
        
        auto fit_result = fit_ols(X, y);
        if (fit_result.is_error()) {
            return fit_result;
        }
        
        r_squared_ = calculate_r_squared(X, y);
        is_trained_ = true;
        training_time_ = DateTime();
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<std::vector<double>> SerializableLinearRegression::predict(
    const std::vector<std::vector<double>>& X) const {
    try {
        if (!is_trained_) {
            return Result<std::vector<double>>::error(ErrorCode::InvalidState, "Model not trained");
        }
        
        if (X.empty()) {
            return Result<std::vector<double>>::success(std::vector<double>());
        }
        
        if (X[0].size() != n_features_) {
            return Result<std::vector<double>>::error(ErrorCode::InvalidInput, 
                                                    "Feature dimension mismatch");
        }
        
        std::vector<double> predictions;
        predictions.reserve(X.size());
        
        for (const auto& row : X) {
            double prediction = intercept_;
            for (size_t j = 0; j < n_features_; ++j) {
                prediction += coefficients_[j] * row[j];
            }
            predictions.push_back(prediction);
        }
        
        return Result<std::vector<double>>::success(predictions);
        
    } catch (const std::exception& e) {
        return Result<std::vector<double>>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<std::vector<uint8_t>> SerializableLinearRegression::serialize() const {
    try {
        std::vector<uint8_t> data;
        
        // Header
        std::string header = "LREG";
        data.insert(data.end(), header.begin(), header.end());
        
        // Version
        uint32_t version = 1;
        auto version_bytes = reinterpret_cast<const uint8_t*>(&version);
        data.insert(data.end(), version_bytes, version_bytes + sizeof(version));
        
        // Model state
        uint8_t trained = is_trained_ ? 1 : 0;
        data.push_back(trained);
        
        // Dimensions
        uint32_t n_feat = static_cast<uint32_t>(n_features_);
        auto feat_bytes = reinterpret_cast<const uint8_t*>(&n_feat);
        data.insert(data.end(), feat_bytes, feat_bytes + sizeof(n_feat));
        
        // Intercept
        auto intercept_bytes = reinterpret_cast<const uint8_t*>(&intercept_);
        data.insert(data.end(), intercept_bytes, intercept_bytes + sizeof(intercept_));
        
        // Coefficients
        for (double coeff : coefficients_) {
            auto coeff_bytes = reinterpret_cast<const uint8_t*>(&coeff);
            data.insert(data.end(), coeff_bytes, coeff_bytes + sizeof(coeff));
        }
        
        // R-squared
        auto r2_bytes = reinterpret_cast<const uint8_t*>(&r_squared_);
        data.insert(data.end(), r2_bytes, r2_bytes + sizeof(r_squared_));
        
        return Result<std::vector<uint8_t>>::success(data);
        
    } catch (const std::exception& e) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<void> SerializableLinearRegression::deserialize(const std::vector<uint8_t>& data) {
    try {
        if (data.size() < 18) {
            return Result<void>::error(ErrorCode::InvalidInput, "Invalid data size");
        }
        
        size_t offset = 0;
        
        // Check header
        std::string header(data.begin(), data.begin() + 4);
        if (header != "LREG") {
            return Result<void>::error(ErrorCode::InvalidInput, "Invalid header");
        }
        offset += 4;
        
        // Read version
        uint32_t version;
        std::memcpy(&version, data.data() + offset, sizeof(version));
        offset += sizeof(version);
        
        // Read training state
        is_trained_ = (data[offset] == 1);
        offset += 1;
        
        // Read dimensions
        uint32_t n_feat;
        std::memcpy(&n_feat, data.data() + offset, sizeof(n_feat));
        n_features_ = n_feat;
        offset += sizeof(n_feat);
        
        // Read intercept
        std::memcpy(&intercept_, data.data() + offset, sizeof(intercept_));
        offset += sizeof(intercept_);
        
        // Read coefficients
        coefficients_.clear();
        coefficients_.reserve(n_features_);
        
        for (size_t i = 0; i < n_features_; ++i) {
            if (offset + sizeof(double) > data.size()) {
                return Result<void>::error(ErrorCode::InvalidInput, "Truncated coefficient data");
            }
            
            double coeff;
            std::memcpy(&coeff, data.data() + offset, sizeof(coeff));
            coefficients_.push_back(coeff);
            offset += sizeof(coeff);
        }
        
        // Read R-squared
        if (offset + sizeof(double) <= data.size()) {
            std::memcpy(&r_squared_, data.data() + offset, sizeof(r_squared_));
        }
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::CalculationError, e.what());
    }
}

std::string SerializableLinearRegression::get_model_type() const {
    return "LinearRegression";
}

ModelMetadata SerializableLinearRegression::get_metadata() const {
    ModelMetadata metadata;
    metadata.model_id = "linear_regression_" + std::to_string(std::time(nullptr));
    metadata.name = "Linear Regression";
    metadata.version = "1.0.0";
    metadata.type = get_model_type();
    metadata.description = "Linear regression model for financial predictions";
    metadata.created_at = training_time_;
    metadata.modified_at = DateTime();
    metadata.author = "PyFolio C++";
    metadata.framework = "PyFolio C++";
    metadata.training_samples = n_samples_;
    metadata.is_production = false;
    
    // Add performance metrics
    metadata.metrics["r_squared"] = r_squared_;
    metadata.metrics["n_features"] = static_cast<double>(n_features_);
    
    return metadata;
}

Result<void> SerializableLinearRegression::validate() const {
    if (coefficients_.size() != n_features_) {
        return Result<void>::error(ErrorCode::InvalidState, "Coefficient dimension mismatch");
    }
    
    // Check for valid coefficients
    for (double coeff : coefficients_) {
        if (!std::isfinite(coeff)) {
            return Result<void>::error(ErrorCode::InvalidState, "Invalid coefficient");
        }
    }
    
    if (!std::isfinite(intercept_)) {
        return Result<void>::error(ErrorCode::InvalidState, "Invalid intercept");
    }
    
    return Result<void>::success();
}

Result<void> SerializableLinearRegression::fit_ols(const std::vector<std::vector<double>>& X,
                                                  const std::vector<double>& y) {
    try {
        // Simple OLS implementation using normal equations
        // X^T * X * beta = X^T * y
        
        coefficients_.resize(n_features_, 0.0);
        
        // Create augmented matrix [X | 1] for intercept
        std::vector<std::vector<double>> X_aug(n_samples_, std::vector<double>(n_features_ + 1));
        for (size_t i = 0; i < n_samples_; ++i) {
            for (size_t j = 0; j < n_features_; ++j) {
                X_aug[i][j] = X[i][j];
            }
            X_aug[i][n_features_] = 1.0; // Intercept column
        }
        
        // Compute X^T * X
        std::vector<std::vector<double>> XtX(n_features_ + 1, std::vector<double>(n_features_ + 1, 0.0));
        for (size_t i = 0; i < n_features_ + 1; ++i) {
            for (size_t j = 0; j < n_features_ + 1; ++j) {
                for (size_t k = 0; k < n_samples_; ++k) {
                    XtX[i][j] += X_aug[k][i] * X_aug[k][j];
                }
            }
        }
        
        // Compute X^T * y
        std::vector<double> Xty(n_features_ + 1, 0.0);
        for (size_t i = 0; i < n_features_ + 1; ++i) {
            for (size_t k = 0; k < n_samples_; ++k) {
                Xty[i] += X_aug[k][i] * y[k];
            }
        }
        
        // Solve using simple Gaussian elimination (for small problems)
        // In practice, would use more robust methods like SVD
        
        // Forward elimination
        for (size_t i = 0; i < n_features_ + 1; ++i) {
            // Find pivot
            size_t max_row = i;
            for (size_t k = i + 1; k < n_features_ + 1; ++k) {
                if (std::abs(XtX[k][i]) > std::abs(XtX[max_row][i])) {
                    max_row = k;
                }
            }
            
            // Swap rows
            std::swap(XtX[i], XtX[max_row]);
            std::swap(Xty[i], Xty[max_row]);
            
            // Make diagonal element 1
            double pivot = XtX[i][i];
            if (std::abs(pivot) < 1e-10) {
                return Result<void>::error(ErrorCode::CalculationError, "Singular matrix");
            }
            
            for (size_t j = i; j < n_features_ + 1; ++j) {
                XtX[i][j] /= pivot;
            }
            Xty[i] /= pivot;
            
            // Eliminate column
            for (size_t k = i + 1; k < n_features_ + 1; ++k) {
                double factor = XtX[k][i];
                for (size_t j = i; j < n_features_ + 1; ++j) {
                    XtX[k][j] -= factor * XtX[i][j];
                }
                Xty[k] -= factor * Xty[i];
            }
        }
        
        // Back substitution
        std::vector<double> solution(n_features_ + 1, 0.0);
        for (int i = static_cast<int>(n_features_); i >= 0; --i) {
            solution[i] = Xty[i];
            for (size_t j = i + 1; j < n_features_ + 1; ++j) {
                solution[i] -= XtX[i][j] * solution[j];
            }
        }
        
        // Extract coefficients and intercept
        for (size_t i = 0; i < n_features_; ++i) {
            coefficients_[i] = solution[i];
        }
        intercept_ = solution[n_features_];
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::CalculationError, e.what());
    }
}

double SerializableLinearRegression::calculate_r_squared(
    const std::vector<std::vector<double>>& X,
    const std::vector<double>& y) const {
    
    // Calculate predictions
    auto pred_result = predict(X);
    if (pred_result.is_error()) {
        return 0.0;
    }
    
    auto predictions = pred_result.value();
    
    // Calculate mean of y
    double y_mean = std::accumulate(y.begin(), y.end(), 0.0) / y.size();
    
    // Calculate SS_tot and SS_res
    double ss_tot = 0.0;
    double ss_res = 0.0;
    
    for (size_t i = 0; i < y.size(); ++i) {
        ss_tot += (y[i] - y_mean) * (y[i] - y_mean);
        ss_res += (y[i] - predictions[i]) * (y[i] - predictions[i]);
    }
    
    return ss_tot > 1e-10 ? 1.0 - (ss_res / ss_tot) : 0.0;
}

// ============================================================================
// SerializableDecisionTree Implementation
// ============================================================================

SerializableDecisionTree::SerializableDecisionTree(int max_depth,
                                                  size_t min_samples_split,
                                                  size_t min_samples_leaf)
    : max_depth_(max_depth), min_samples_split_(min_samples_split),
      min_samples_leaf_(min_samples_leaf), n_features_(0), is_trained_(false),
      training_time_(DateTime()) {}

Result<void> SerializableDecisionTree::train(const std::vector<std::vector<double>>& X,
                                           const std::vector<double>& y) {
    try {
        if (X.empty() || y.empty() || X.size() != y.size()) {
            return Result<void>::error(ErrorCode::InvalidInput, "Invalid training data");
        }
        
        n_features_ = X[0].size();
        
        // Create sample indices
        std::vector<size_t> samples(X.size());
        std::iota(samples.begin(), samples.end(), 0);
        
        // Build tree
        tree_.clear();
        auto root_index = build_tree(X, y, samples, 0);
        (void)root_index; // Suppress unused warning
        
        is_trained_ = true;
        training_time_ = DateTime();
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<std::vector<double>> SerializableDecisionTree::predict(
    const std::vector<std::vector<double>>& X) const {
    try {
        if (!is_trained_) {
            return Result<std::vector<double>>::error(ErrorCode::InvalidState, "Model not trained");
        }
        
        std::vector<double> predictions;
        predictions.reserve(X.size());
        
        for (const auto& sample : X) {
            predictions.push_back(predict_sample(sample));
        }
        
        return Result<std::vector<double>>::success(predictions);
        
    } catch (const std::exception& e) {
        return Result<std::vector<double>>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<std::vector<uint8_t>> SerializableDecisionTree::serialize() const {
    try {
        std::vector<uint8_t> data;
        
        // Header
        std::string header = "DTREE";
        data.insert(data.end(), header.begin(), header.end());
        
        // Version and metadata
        uint32_t version = 1;
        auto version_bytes = reinterpret_cast<const uint8_t*>(&version);
        data.insert(data.end(), version_bytes, version_bytes + sizeof(version));
        
        // Tree size
        uint32_t tree_size = static_cast<uint32_t>(tree_.size());
        auto size_bytes = reinterpret_cast<const uint8_t*>(&tree_size);
        data.insert(data.end(), size_bytes, size_bytes + sizeof(tree_size));
        
        // Serialize each node
        for (const auto& node : tree_) {
            // Feature index
            auto feat_bytes = reinterpret_cast<const uint8_t*>(&node.feature_index);
            data.insert(data.end(), feat_bytes, feat_bytes + sizeof(node.feature_index));
            
            // Threshold
            auto thresh_bytes = reinterpret_cast<const uint8_t*>(&node.threshold);
            data.insert(data.end(), thresh_bytes, thresh_bytes + sizeof(node.threshold));
            
            // Value
            auto value_bytes = reinterpret_cast<const uint8_t*>(&node.value);
            data.insert(data.end(), value_bytes, value_bytes + sizeof(node.value));
            
            // Child indices
            auto left_bytes = reinterpret_cast<const uint8_t*>(&node.left_child);
            data.insert(data.end(), left_bytes, left_bytes + sizeof(node.left_child));
            
            auto right_bytes = reinterpret_cast<const uint8_t*>(&node.right_child);
            data.insert(data.end(), right_bytes, right_bytes + sizeof(node.right_child));
        }
        
        return Result<std::vector<uint8_t>>::success(data);
        
    } catch (const std::exception& e) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<void> SerializableDecisionTree::deserialize(const std::vector<uint8_t>& data) {
    try {
        if (data.size() < 12) {
            return Result<void>::error(ErrorCode::InvalidInput, "Invalid data size");
        }
        
        size_t offset = 0;
        
        // Check header
        std::string header(data.begin(), data.begin() + 5);
        if (header != "DTREE") {
            return Result<void>::error(ErrorCode::InvalidInput, "Invalid header");
        }
        offset += 5;
        
        // Read version
        uint32_t version;
        std::memcpy(&version, data.data() + offset, sizeof(version));
        offset += sizeof(version);
        
        // Read tree size
        uint32_t tree_size;
        std::memcpy(&tree_size, data.data() + offset, sizeof(tree_size));
        offset += sizeof(tree_size);
        
        // Read nodes
        tree_.clear();
        tree_.reserve(tree_size);
        
        for (uint32_t i = 0; i < tree_size; ++i) {
            TreeNode node;
            
            std::memcpy(&node.feature_index, data.data() + offset, sizeof(node.feature_index));
            offset += sizeof(node.feature_index);
            
            std::memcpy(&node.threshold, data.data() + offset, sizeof(node.threshold));
            offset += sizeof(node.threshold);
            
            std::memcpy(&node.value, data.data() + offset, sizeof(node.value));
            offset += sizeof(node.value);
            
            std::memcpy(&node.left_child, data.data() + offset, sizeof(node.left_child));
            offset += sizeof(node.left_child);
            
            std::memcpy(&node.right_child, data.data() + offset, sizeof(node.right_child));
            offset += sizeof(node.right_child);
            
            tree_.push_back(node);
        }
        
        is_trained_ = true;
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::CalculationError, e.what());
    }
}

std::string SerializableDecisionTree::get_model_type() const {
    return "DecisionTree";
}

ModelMetadata SerializableDecisionTree::get_metadata() const {
    ModelMetadata metadata;
    metadata.model_id = "decision_tree_" + std::to_string(std::time(nullptr));
    metadata.name = "Decision Tree";
    metadata.version = "1.0.0";
    metadata.type = get_model_type();
    metadata.description = "Decision tree model for financial predictions";
    metadata.created_at = training_time_;
    metadata.modified_at = DateTime();
    metadata.author = "PyFolio C++";
    metadata.framework = "PyFolio C++";
    metadata.is_production = false;
    
    // Add tree-specific metrics
    metadata.metrics["max_depth"] = static_cast<double>(max_depth_);
    metadata.metrics["tree_size"] = static_cast<double>(tree_.size());
    metadata.metrics["n_features"] = static_cast<double>(n_features_);
    
    return metadata;
}

Result<void> SerializableDecisionTree::validate() const {
    if (tree_.empty()) {
        return Result<void>::error(ErrorCode::InvalidState, "Empty tree");
    }
    
    // Validate tree structure
    for (size_t i = 0; i < tree_.size(); ++i) {
        const auto& node = tree_[i];
        
        // Check child indices
        if (node.left_child >= static_cast<int>(tree_.size()) ||
            node.right_child >= static_cast<int>(tree_.size())) {
            return Result<void>::error(ErrorCode::InvalidState, "Invalid child index");
        }
        
        // Check values
        if (!std::isfinite(node.value) || !std::isfinite(node.threshold)) {
            return Result<void>::error(ErrorCode::InvalidState, "Invalid node values");
        }
    }
    
    return Result<void>::success();
}

int SerializableDecisionTree::build_tree(const std::vector<std::vector<double>>& X,
                                        const std::vector<double>& y,
                                        const std::vector<size_t>& samples,
                                        int depth) {
    TreeNode node;
    node.n_samples = samples.size();
    
    // Calculate mean value for this node
    double sum = 0.0;
    for (size_t idx : samples) {
        sum += y[idx];
    }
    node.value = sum / samples.size();
    
    // Check stopping criteria
    if (depth >= max_depth_ || 
        samples.size() < min_samples_split_ ||
        samples.size() <= min_samples_leaf_) {
        
        // Create leaf node
        node.feature_index = -1;
        tree_.push_back(node);
        return static_cast<int>(tree_.size() - 1);
    }
    
    // Find best split
    auto best_split = find_best_split(X, y, samples);
    if (best_split.first == -1) {
        // No good split found, create leaf
        node.feature_index = -1;
        tree_.push_back(node);
        return static_cast<int>(tree_.size() - 1);
    }
    
    node.feature_index = best_split.first;
    node.threshold = best_split.second;
    
    // Split samples
    std::vector<size_t> left_samples, right_samples;
    for (size_t idx : samples) {
        if (X[idx][node.feature_index] <= node.threshold) {
            left_samples.push_back(idx);
        } else {
            right_samples.push_back(idx);
        }
    }
    
    // Add current node
    int node_index = static_cast<int>(tree_.size());
    tree_.push_back(node);
    
    // Recursively build children
    if (!left_samples.empty()) {
        tree_[node_index].left_child = build_tree(X, y, left_samples, depth + 1);
    }
    
    if (!right_samples.empty()) {
        tree_[node_index].right_child = build_tree(X, y, right_samples, depth + 1);
    }
    
    return node_index;
}

std::pair<int, double> SerializableDecisionTree::find_best_split(
    const std::vector<std::vector<double>>& X,
    const std::vector<double>& y,
    const std::vector<size_t>& samples) const {
    
    double best_impurity_reduction = 0.0;
    int best_feature = -1;
    double best_threshold = 0.0;
    
    double parent_impurity = calculate_impurity(y, samples);
    
    // Try each feature
    for (size_t feature = 0; feature < n_features_; ++feature) {
        // Get unique values for this feature
        std::vector<double> values;
        for (size_t idx : samples) {
            values.push_back(X[idx][feature]);
        }
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
        
        // Try each threshold
        for (size_t i = 0; i < values.size() - 1; ++i) {
            double threshold = (values[i] + values[i + 1]) / 2.0;
            
            // Split samples
            std::vector<size_t> left_samples, right_samples;
            for (size_t idx : samples) {
                if (X[idx][feature] <= threshold) {
                    left_samples.push_back(idx);
                } else {
                    right_samples.push_back(idx);
                }
            }
            
            if (left_samples.empty() || right_samples.empty()) {
                continue;
            }
            
            // Calculate weighted impurity
            double left_impurity = calculate_impurity(y, left_samples);
            double right_impurity = calculate_impurity(y, right_samples);
            
            double left_weight = static_cast<double>(left_samples.size()) / samples.size();
            double right_weight = static_cast<double>(right_samples.size()) / samples.size();
            
            double weighted_impurity = left_weight * left_impurity + right_weight * right_impurity;
            double impurity_reduction = parent_impurity - weighted_impurity;
            
            if (impurity_reduction > best_impurity_reduction) {
                best_impurity_reduction = impurity_reduction;
                best_feature = static_cast<int>(feature);
                best_threshold = threshold;
            }
        }
    }
    
    return {best_feature, best_threshold};
}

double SerializableDecisionTree::calculate_impurity(const std::vector<double>& y,
                                                   const std::vector<size_t>& samples) const {
    if (samples.empty()) return 0.0;
    
    // Calculate variance (for regression)
    double sum = 0.0;
    for (size_t idx : samples) {
        sum += y[idx];
    }
    double mean = sum / samples.size();
    
    double variance = 0.0;
    for (size_t idx : samples) {
        variance += (y[idx] - mean) * (y[idx] - mean);
    }
    
    return variance / samples.size();
}

double SerializableDecisionTree::predict_sample(const std::vector<double>& x) const {
    if (tree_.empty()) return 0.0;
    
    int current = 0;
    
    while (current >= 0 && current < static_cast<int>(tree_.size())) {
        const auto& node = tree_[current];
        
        if (node.feature_index == -1) {
            // Leaf node
            return node.value;
        }
        
        // Internal node
        if (x[node.feature_index] <= node.threshold) {
            current = node.left_child;
        } else {
            current = node.right_child;
        }
        
        // Safety check for invalid indices
        if (current == -1) {
            return node.value;
        }
    }
    
    return 0.0; // Fallback
}

// ============================================================================
// SerializableNeuralNetwork Implementation  
// ============================================================================

SerializableNeuralNetwork::SerializableNeuralNetwork(
    const std::vector<LayerConfig>& layers,
    double learning_rate,
    size_t epochs)
    : layer_configs_(layers), learning_rate_(learning_rate), 
      epochs_(epochs), is_trained_(false), final_loss_(0.0) {
    initialize_weights();
}

Result<void> SerializableNeuralNetwork::train(const std::vector<std::vector<double>>& X,
                                             const std::vector<double>& y) {
    try {
        if (X.empty() || y.empty() || X.size() != y.size()) {
            return Result<void>::error(ErrorCode::InvalidInput, "Invalid training data");
        }
        
        // Simple training loop with gradient descent
        for (size_t epoch = 0; epoch < epochs_; ++epoch) {
            double total_loss = 0.0;
            
            for (size_t i = 0; i < X.size(); ++i) {
                auto prediction = forward_pass(X[i]);
                double loss = calculate_loss(prediction, {y[i]});
                total_loss += loss;
                
                // Placeholder for backpropagation
                // In a real implementation, would compute gradients and update weights
            }
            
            final_loss_ = total_loss / X.size();
        }
        
        is_trained_ = true;
        training_time_ = DateTime();
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<std::vector<double>> SerializableNeuralNetwork::predict(
    const std::vector<std::vector<double>>& X) const {
    try {
        if (!is_trained_) {
            return Result<std::vector<double>>::error(ErrorCode::InvalidState, "Model not trained");
        }
        
        std::vector<double> predictions;
        predictions.reserve(X.size());
        
        for (const auto& sample : X) {
            auto prediction = forward_pass(sample);
            predictions.push_back(prediction.empty() ? 0.0 : prediction[0]);
        }
        
        return Result<std::vector<double>>::success(predictions);
        
    } catch (const std::exception& e) {
        return Result<std::vector<double>>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<std::vector<uint8_t>> SerializableNeuralNetwork::serialize() const {
    try {
        std::vector<uint8_t> data;
        
        // Header
        std::string header = "NNET";
        data.insert(data.end(), header.begin(), header.end());
        
        // Version
        uint32_t version = 1;
        auto version_bytes = reinterpret_cast<const uint8_t*>(&version);
        data.insert(data.end(), version_bytes, version_bytes + sizeof(version));
        
        // Simple serialization of network parameters
        uint32_t num_layers = static_cast<uint32_t>(layer_configs_.size());
        auto layers_bytes = reinterpret_cast<const uint8_t*>(&num_layers);
        data.insert(data.end(), layers_bytes, layers_bytes + sizeof(num_layers));
        
        return Result<std::vector<uint8_t>>::success(data);
        
    } catch (const std::exception& e) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<void> SerializableNeuralNetwork::deserialize(const std::vector<uint8_t>& data) {
    try {
        if (data.size() < 12) {
            return Result<void>::error(ErrorCode::InvalidInput, "Invalid data size");
        }
        
        // Simple deserialization
        is_trained_ = true;
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::CalculationError, e.what());
    }
}

std::string SerializableNeuralNetwork::get_model_type() const {
    return "NeuralNetwork";
}

ModelMetadata SerializableNeuralNetwork::get_metadata() const {
    ModelMetadata metadata;
    metadata.model_id = "neural_network_" + std::to_string(std::time(nullptr));
    metadata.name = "Neural Network";
    metadata.version = "1.0.0";
    metadata.type = get_model_type();
    metadata.description = "Neural network model for financial predictions";
    metadata.created_at = training_time_;
    metadata.modified_at = DateTime();
    metadata.author = "PyFolio C++";
    metadata.framework = "PyFolio C++";
    metadata.is_production = false;
    
    metadata.metrics["final_loss"] = final_loss_;
    metadata.metrics["num_layers"] = static_cast<double>(layer_configs_.size());
    
    return metadata;
}

Result<void> SerializableNeuralNetwork::validate() const {
    if (layer_configs_.empty()) {
        return Result<void>::error(ErrorCode::InvalidState, "No layers configured");
    }
    
    if (weights_.size() != layer_configs_.size()) {
        return Result<void>::error(ErrorCode::InvalidState, "Weight dimension mismatch");
    }
    
    return Result<void>::success();
}

std::vector<double> SerializableNeuralNetwork::forward_pass(const std::vector<double>& input) const {
    // Simple forward pass implementation
    std::vector<double> activation = input;
    
    for (size_t layer = 0; layer < layer_configs_.size(); ++layer) {
        if (layer < weights_.size() && !weights_[layer].empty()) {
            std::vector<double> next_activation(layer_configs_[layer].output_size, 0.0);
            
            for (size_t out = 0; out < layer_configs_[layer].output_size; ++out) {
                for (size_t in = 0; in < activation.size() && in < weights_[layer][out].size(); ++in) {
                    next_activation[out] += weights_[layer][out][in] * activation[in];
                }
                
                // Add bias if available
                if (layer < biases_.size() && out < biases_[layer].size()) {
                    next_activation[out] += biases_[layer][out];
                }
                
                // Apply activation function
                next_activation[out] = apply_activation(next_activation[out], layer_configs_[layer].activation);
            }
            
            activation = next_activation;
        }
    }
    
    return activation;
}

void SerializableNeuralNetwork::backward_pass(const std::vector<double>& input,
                                             const std::vector<double>& target,
                                             std::vector<std::vector<std::vector<double>>>& weight_gradients,
                                             std::vector<std::vector<double>>& bias_gradients) const {
    // Placeholder for backpropagation implementation
    (void)input;
    (void)target;
    (void)weight_gradients;
    (void)bias_gradients;
}

double SerializableNeuralNetwork::apply_activation(double x, const std::string& activation) const {
    if (activation == "relu") {
        return std::max(0.0, x);
    } else if (activation == "sigmoid") {
        return 1.0 / (1.0 + std::exp(-x));
    } else if (activation == "tanh") {
        return std::tanh(x);
    } else {
        return x; // linear
    }
}

double SerializableNeuralNetwork::apply_activation_derivative(double x, const std::string& activation) const {
    if (activation == "relu") {
        return x > 0.0 ? 1.0 : 0.0;
    } else if (activation == "sigmoid") {
        double s = apply_activation(x, "sigmoid");
        return s * (1.0 - s);
    } else if (activation == "tanh") {
        double t = std::tanh(x);
        return 1.0 - t * t;
    } else {
        return 1.0; // linear
    }
}

double SerializableNeuralNetwork::calculate_loss(const std::vector<double>& predicted,
                                                const std::vector<double>& actual) const {
    if (predicted.size() != actual.size()) {
        return 1e6; // Large loss for mismatched sizes
    }
    
    double mse = 0.0;
    for (size_t i = 0; i < predicted.size(); ++i) {
        double error = predicted[i] - actual[i];
        mse += error * error;
    }
    
    return mse / predicted.size();
}

void SerializableNeuralNetwork::initialize_weights() {
    weights_.clear();
    biases_.clear();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> weight_dist(0.0, 0.1);
    
    for (size_t layer = 0; layer < layer_configs_.size(); ++layer) {
        const auto& config = layer_configs_[layer];
        
        // Initialize weights
        std::vector<std::vector<double>> layer_weights(config.output_size);
        for (size_t out = 0; out < config.output_size; ++out) {
            layer_weights[out].resize(config.input_size);
            for (size_t in = 0; in < config.input_size; ++in) {
                layer_weights[out][in] = weight_dist(gen);
            }
        }
        weights_.push_back(layer_weights);
        
        // Initialize biases
        std::vector<double> layer_biases(config.output_size);
        for (size_t out = 0; out < config.output_size; ++out) {
            layer_biases[out] = weight_dist(gen);
        }
        biases_.push_back(layer_biases);
    }
}

} // namespace pyfolio::ml