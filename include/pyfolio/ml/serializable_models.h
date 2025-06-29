#pragma once

/**
 * @file serializable_models.h
 * @brief Example serializable models for the persistence framework
 * @version 1.0.0
 * @date 2024
 */

#include "model_persistence.h"
#include "../analytics/regime_detection.h"
#include <vector>
#include <memory>

namespace pyfolio::ml {

/**
 * @brief Serializable regime detection model
 */
class SerializableRegimeDetector : public SerializableModel {
public:
    /**
     * @brief Constructor
     */
    SerializableRegimeDetector();
    
    /**
     * @brief Constructor with underlying model
     * @param detector Regime detector instance
     */
    explicit SerializableRegimeDetector(std::shared_ptr<analytics::RegimeDetector> detector);
    
    /**
     * @brief Serialize model to bytes
     * @return Serialized model data
     */
    [[nodiscard]] Result<std::vector<uint8_t>> serialize() const override;
    
    /**
     * @brief Deserialize model from bytes
     * @param data Serialized model data
     * @return Success or error
     */
    [[nodiscard]] Result<void> deserialize(const std::vector<uint8_t>& data) override;
    
    /**
     * @brief Get model type identifier
     * @return Model type string
     */
    [[nodiscard]] std::string get_model_type() const override;
    
    /**
     * @brief Get model metadata
     * @return Model metadata
     */
    [[nodiscard]] ModelMetadata get_metadata() const override;
    
    /**
     * @brief Validate model integrity
     * @return Validation result
     */
    [[nodiscard]] Result<void> validate() const override;
    
    /**
     * @brief Get underlying detector
     * @return Regime detector instance
     */
    [[nodiscard]] std::shared_ptr<analytics::RegimeDetector> get_detector() const {
        return detector_;
    }
    
    /**
     * @brief Set model parameters
     * @param params Model parameters
     */
    void set_parameters(const std::vector<double>& params) { parameters_ = params; }
    
    /**
     * @brief Get model parameters
     * @return Model parameters
     */
    [[nodiscard]] const std::vector<double>& get_parameters() const { return parameters_; }

private:
    std::shared_ptr<analytics::RegimeDetector> detector_;
    std::vector<double> parameters_;
    std::string model_name_;
    std::string model_version_;
    DateTime creation_time_;
    
    // Serialization helpers
    [[nodiscard]] std::vector<uint8_t> serialize_parameters() const;
    [[nodiscard]] Result<void> deserialize_parameters(const std::vector<uint8_t>& data);
};

/**
 * @brief Serializable linear regression model
 */
class SerializableLinearRegression : public SerializableModel {
public:
    /**
     * @brief Constructor
     */
    SerializableLinearRegression();
    
    /**
     * @brief Train the model
     * @param X Feature matrix
     * @param y Target vector
     * @return Training result
     */
    [[nodiscard]] Result<void> train(const std::vector<std::vector<double>>& X,
                                   const std::vector<double>& y);
    
    /**
     * @brief Make predictions
     * @param X Feature matrix
     * @return Predictions
     */
    [[nodiscard]] Result<std::vector<double>> predict(
        const std::vector<std::vector<double>>& X) const;
    
    /**
     * @brief Serialize model to bytes
     * @return Serialized model data
     */
    [[nodiscard]] Result<std::vector<uint8_t>> serialize() const override;
    
    /**
     * @brief Deserialize model from bytes
     * @param data Serialized model data
     * @return Success or error
     */
    [[nodiscard]] Result<void> deserialize(const std::vector<uint8_t>& data) override;
    
    /**
     * @brief Get model type identifier
     * @return Model type string
     */
    [[nodiscard]] std::string get_model_type() const override;
    
    /**
     * @brief Get model metadata
     * @return Model metadata
     */
    [[nodiscard]] ModelMetadata get_metadata() const override;
    
    /**
     * @brief Validate model integrity
     * @return Validation result
     */
    [[nodiscard]] Result<void> validate() const override;
    
    /**
     * @brief Get model coefficients
     * @return Coefficients
     */
    [[nodiscard]] const std::vector<double>& get_coefficients() const { return coefficients_; }
    
    /**
     * @brief Get intercept
     * @return Intercept value
     */
    [[nodiscard]] double get_intercept() const { return intercept_; }
    
    /**
     * @brief Get R-squared score
     * @return R-squared value
     */
    [[nodiscard]] double get_r_squared() const { return r_squared_; }

private:
    std::vector<double> coefficients_;
    double intercept_;
    double r_squared_;
    size_t n_features_;
    size_t n_samples_;
    bool is_trained_;
    DateTime training_time_;
    
    // Model fitting helpers
    [[nodiscard]] Result<void> fit_ols(const std::vector<std::vector<double>>& X,
                                     const std::vector<double>& y);
    [[nodiscard]] double calculate_r_squared(const std::vector<std::vector<double>>& X,
                                           const std::vector<double>& y) const;
};

/**
 * @brief Serializable decision tree model
 */
class SerializableDecisionTree : public SerializableModel {
public:
    /**
     * @brief Tree node structure
     */
    struct TreeNode {
        int feature_index;      ///< Feature index for split (-1 for leaf)
        double threshold;       ///< Split threshold
        double value;          ///< Leaf value
        int left_child;        ///< Left child index (-1 if none)
        int right_child;       ///< Right child index (-1 if none)
        double impurity;       ///< Node impurity
        size_t n_samples;      ///< Number of samples
        
        TreeNode() : feature_index(-1), threshold(0.0), value(0.0),
                    left_child(-1), right_child(-1), impurity(0.0), n_samples(0) {}
    };
    
    /**
     * @brief Constructor
     * @param max_depth Maximum tree depth
     * @param min_samples_split Minimum samples to split
     * @param min_samples_leaf Minimum samples in leaf
     */
    SerializableDecisionTree(int max_depth = 10, 
                           size_t min_samples_split = 2,
                           size_t min_samples_leaf = 1);
    
    /**
     * @brief Train the model
     * @param X Feature matrix
     * @param y Target vector
     * @return Training result
     */
    [[nodiscard]] Result<void> train(const std::vector<std::vector<double>>& X,
                                   const std::vector<double>& y);
    
    /**
     * @brief Make predictions
     * @param X Feature matrix
     * @return Predictions
     */
    [[nodiscard]] Result<std::vector<double>> predict(
        const std::vector<std::vector<double>>& X) const;
    
    /**
     * @brief Serialize model to bytes
     * @return Serialized model data
     */
    [[nodiscard]] Result<std::vector<uint8_t>> serialize() const override;
    
    /**
     * @brief Deserialize model from bytes
     * @param data Serialized model data
     * @return Success or error
     */
    [[nodiscard]] Result<void> deserialize(const std::vector<uint8_t>& data) override;
    
    /**
     * @brief Get model type identifier
     * @return Model type string
     */
    [[nodiscard]] std::string get_model_type() const override;
    
    /**
     * @brief Get model metadata
     * @return Model metadata
     */
    [[nodiscard]] ModelMetadata get_metadata() const override;
    
    /**
     * @brief Validate model integrity
     * @return Validation result
     */
    [[nodiscard]] Result<void> validate() const override;
    
    /**
     * @brief Get tree structure
     * @return Tree nodes
     */
    [[nodiscard]] const std::vector<TreeNode>& get_tree() const { return tree_; }
    
    /**
     * @brief Get feature importance scores
     * @return Feature importance
     */
    [[nodiscard]] std::vector<double> get_feature_importance() const;

private:
    std::vector<TreeNode> tree_;
    int max_depth_;
    size_t min_samples_split_;
    size_t min_samples_leaf_;
    size_t n_features_;
    bool is_trained_;
    DateTime training_time_;
    
    // Tree building helpers
    [[nodiscard]] int build_tree(const std::vector<std::vector<double>>& X,
                                const std::vector<double>& y,
                                const std::vector<size_t>& samples,
                                int depth);
    
    [[nodiscard]] std::pair<int, double> find_best_split(
        const std::vector<std::vector<double>>& X,
        const std::vector<double>& y,
        const std::vector<size_t>& samples) const;
    
    [[nodiscard]] double calculate_impurity(const std::vector<double>& y,
                                          const std::vector<size_t>& samples) const;
    
    [[nodiscard]] double predict_sample(const std::vector<double>& x) const;
};

/**
 * @brief Serializable neural network model
 */
class SerializableNeuralNetwork : public SerializableModel {
public:
    /**
     * @brief Layer configuration
     */
    struct LayerConfig {
        size_t input_size;
        size_t output_size;
        std::string activation;  ///< "relu", "sigmoid", "tanh", "linear"
        
        LayerConfig(size_t in, size_t out, const std::string& act = "relu")
            : input_size(in), output_size(out), activation(act) {}
    };
    
    /**
     * @brief Constructor
     * @param layers Layer configurations
     * @param learning_rate Learning rate
     * @param epochs Training epochs
     */
    SerializableNeuralNetwork(const std::vector<LayerConfig>& layers,
                            double learning_rate = 0.001,
                            size_t epochs = 100);
    
    /**
     * @brief Train the model
     * @param X Feature matrix
     * @param y Target vector
     * @return Training result
     */
    [[nodiscard]] Result<void> train(const std::vector<std::vector<double>>& X,
                                   const std::vector<double>& y);
    
    /**
     * @brief Make predictions
     * @param X Feature matrix
     * @return Predictions
     */
    [[nodiscard]] Result<std::vector<double>> predict(
        const std::vector<std::vector<double>>& X) const;
    
    /**
     * @brief Serialize model to bytes
     * @return Serialized model data
     */
    [[nodiscard]] Result<std::vector<uint8_t>> serialize() const override;
    
    /**
     * @brief Deserialize model from bytes
     * @param data Serialized model data
     * @return Success or error
     */
    [[nodiscard]] Result<void> deserialize(const std::vector<uint8_t>& data) override;
    
    /**
     * @brief Get model type identifier
     * @return Model type string
     */
    [[nodiscard]] std::string get_model_type() const override;
    
    /**
     * @brief Get model metadata
     * @return Model metadata
     */
    [[nodiscard]] ModelMetadata get_metadata() const override;
    
    /**
     * @brief Validate model integrity
     * @return Validation result
     */
    [[nodiscard]] Result<void> validate() const override;
    
    /**
     * @brief Get layer weights
     * @return Weight matrices for each layer
     */
    [[nodiscard]] const std::vector<std::vector<std::vector<double>>>& get_weights() const {
        return weights_;
    }
    
    /**
     * @brief Get layer biases
     * @return Bias vectors for each layer
     */
    [[nodiscard]] const std::vector<std::vector<double>>& get_biases() const {
        return biases_;
    }

private:
    std::vector<LayerConfig> layer_configs_;
    std::vector<std::vector<std::vector<double>>> weights_;  ///< Weights[layer][neuron][input]
    std::vector<std::vector<double>> biases_;               ///< Biases[layer][neuron]
    double learning_rate_;
    size_t epochs_;
    bool is_trained_;
    DateTime training_time_;
    double final_loss_;
    
    // Neural network helpers
    [[nodiscard]] std::vector<double> forward_pass(const std::vector<double>& input) const;
    void backward_pass(const std::vector<double>& input,
                      const std::vector<double>& target,
                      std::vector<std::vector<std::vector<double>>>& weight_gradients,
                      std::vector<std::vector<double>>& bias_gradients) const;
    
    [[nodiscard]] double apply_activation(double x, const std::string& activation) const;
    [[nodiscard]] double apply_activation_derivative(double x, const std::string& activation) const;
    
    [[nodiscard]] double calculate_loss(const std::vector<double>& predicted,
                                      const std::vector<double>& actual) const;
    
    void initialize_weights();
};

} // namespace pyfolio::ml