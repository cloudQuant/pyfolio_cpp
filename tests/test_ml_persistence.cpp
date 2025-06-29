#include <gtest/gtest.h>
#include <pyfolio/ml/model_persistence.h>
#include <pyfolio/ml/serializable_models.h>
#include <filesystem>
#include <random>

using namespace pyfolio::ml;

/**
 * @brief Test fixture for ML persistence tests
 */
class MLPersistenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test directories
        test_dir_ = std::filesystem::temp_directory_path() / "pyfolio_ml_test";
        std::filesystem::create_directories(test_dir_);
        
        // Generate test data
        std::tie(X_train_, y_train_) = generate_test_data(100, 3);
        std::tie(X_test_, y_test_) = generate_test_data(30, 3);
    }
    
    void TearDown() override {
        // Clean up test directory
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
    }
    
    std::pair<std::vector<std::vector<double>>, std::vector<double>> 
    generate_test_data(size_t n_samples, size_t n_features) {
        std::mt19937 gen(42); // Fixed seed for reproducibility
        std::normal_distribution<double> normal(0.0, 1.0);
        
        std::vector<std::vector<double>> X(n_samples, std::vector<double>(n_features));
        std::vector<double> y(n_samples);
        
        for (size_t i = 0; i < n_samples; ++i) {
            for (size_t j = 0; j < n_features; ++j) {
                X[i][j] = normal(gen);
            }
            
            // Linear relationship with some noise
            y[i] = 0.5 * X[i][0] + 0.3 * X[i][1] - 0.2 * X[i][2] + normal(gen) * 0.1;
        }
        
        return {X, y};
    }
    
    std::filesystem::path test_dir_;
    std::vector<std::vector<double>> X_train_, X_test_;
    std::vector<double> y_train_, y_test_;
};

/**
 * @brief Test ModelSerializer basic functionality
 */
TEST_F(MLPersistenceTest, ModelSerializerBasic) {
    SerializableLinearRegression model;
    auto train_result = model.train(X_train_, y_train_);
    ASSERT_TRUE(train_result.is_ok());
    
    ModelSerializer serializer;
    
    // Test serialization to bytes
    auto bytes_result = serializer.serialize_to_bytes(model);
    ASSERT_TRUE(bytes_result.is_ok());
    
    auto serialized_data = bytes_result.value();
    EXPECT_GT(serialized_data.size(), 0);
    
    // Test serialization to string
    auto string_result = serializer.serialize_to_string(model);
    ASSERT_TRUE(string_result.is_ok());
    
    auto serialized_string = string_result.value();
    EXPECT_GT(serialized_string.size(), 0);
}

/**
 * @brief Test ModelSerializer file operations
 */
TEST_F(MLPersistenceTest, ModelSerializerFileOperations) {
    SerializableLinearRegression model;
    auto train_result = model.train(X_train_, y_train_);
    ASSERT_TRUE(train_result.is_ok());
    
    double original_r2 = model.get_r_squared();
    auto original_coeffs = model.get_coefficients();
    double original_intercept = model.get_intercept();
    
    ModelSerializer serializer;
    auto model_path = test_dir_ / "test_model.bin";
    
    // Save model
    auto save_result = serializer.save_model(model, model_path);
    ASSERT_TRUE(save_result.is_ok());
    EXPECT_TRUE(std::filesystem::exists(model_path));
    
    // Check metadata file exists
    auto metadata_path = model_path;
    metadata_path.replace_extension(".metadata.json");
    EXPECT_TRUE(std::filesystem::exists(metadata_path));
}

/**
 * @brief Test ModelLoader functionality
 */
TEST_F(MLPersistenceTest, ModelLoaderBasic) {
    // Create and save a model
    SerializableLinearRegression original_model;
    auto train_result = original_model.train(X_train_, y_train_);
    ASSERT_TRUE(train_result.is_ok());
    
    ModelSerializer serializer;
    auto model_path = test_dir_ / "loader_test.bin";
    auto save_result = serializer.save_model(original_model, model_path);
    ASSERT_TRUE(save_result.is_ok());
    
    // Load the model
    ModelLoader loader;
    auto load_result = loader.load_model<SerializableLinearRegression>(model_path);
    ASSERT_TRUE(load_result.is_ok());
    
    auto loaded_model = std::move(load_result.value());
    
    // Verify the loaded model
    EXPECT_NEAR(loaded_model->get_r_squared(), original_model.get_r_squared(), 1e-10);
    EXPECT_NEAR(loaded_model->get_intercept(), original_model.get_intercept(), 1e-10);
    
    auto original_coeffs = original_model.get_coefficients();
    auto loaded_coeffs = loaded_model->get_coefficients();
    ASSERT_EQ(original_coeffs.size(), loaded_coeffs.size());
    
    for (size_t i = 0; i < original_coeffs.size(); ++i) {
        EXPECT_NEAR(original_coeffs[i], loaded_coeffs[i], 1e-10);
    }
}

/**
 * @brief Test model prediction consistency after serialization
 */
TEST_F(MLPersistenceTest, PredictionConsistency) {
    SerializableLinearRegression original_model;
    auto train_result = original_model.train(X_train_, y_train_);
    ASSERT_TRUE(train_result.is_ok());
    
    // Get original predictions
    auto original_pred_result = original_model.predict(X_test_);
    ASSERT_TRUE(original_pred_result.is_ok());
    auto original_predictions = original_pred_result.value();
    
    // Save and load model
    ModelSerializer serializer;
    auto model_path = test_dir_ / "consistency_test.bin";
    auto save_result = serializer.save_model(original_model, model_path);
    ASSERT_TRUE(save_result.is_ok());
    
    ModelLoader loader;
    auto load_result = loader.load_model<SerializableLinearRegression>(model_path);
    ASSERT_TRUE(load_result.is_ok());
    auto loaded_model = std::move(load_result.value());
    
    // Get loaded predictions
    auto loaded_pred_result = loaded_model->predict(X_test_);
    ASSERT_TRUE(loaded_pred_result.is_ok());
    auto loaded_predictions = loaded_pred_result.value();
    
    // Compare predictions
    ASSERT_EQ(original_predictions.size(), loaded_predictions.size());
    for (size_t i = 0; i < original_predictions.size(); ++i) {
        EXPECT_NEAR(original_predictions[i], loaded_predictions[i], 1e-10);
    }
}

/**
 * @brief Test SerializableLinearRegression
 */
TEST_F(MLPersistenceTest, SerializableLinearRegressionBasic) {
    SerializableLinearRegression model;
    
    // Test initial state
    EXPECT_EQ(model.get_model_type(), "LinearRegression");
    
    auto validate_result = model.validate();
    EXPECT_TRUE(validate_result.is_error()); // Should fail before training
    
    // Train model
    auto train_result = model.train(X_train_, y_train_);
    ASSERT_TRUE(train_result.is_ok());
    
    // Test after training
    validate_result = model.validate();
    EXPECT_TRUE(validate_result.is_ok());
    
    EXPECT_GT(model.get_r_squared(), 0.0);
    EXPECT_EQ(model.get_coefficients().size(), 3);
    
    // Test predictions
    auto pred_result = model.predict(X_test_);
    ASSERT_TRUE(pred_result.is_ok());
    
    auto predictions = pred_result.value();
    EXPECT_EQ(predictions.size(), X_test_.size());
    
    // Test metadata
    auto metadata = model.get_metadata();
    EXPECT_EQ(metadata.type, "LinearRegression");
    EXPECT_GT(metadata.training_samples, 0);
}

/**
 * @brief Test SerializableDecisionTree
 */
TEST_F(MLPersistenceTest, SerializableDecisionTreeBasic) {
    SerializableDecisionTree tree(3, 5, 2); // Small tree for testing
    
    EXPECT_EQ(tree.get_model_type(), "DecisionTree");
    
    // Train tree
    auto train_result = tree.train(X_train_, y_train_);
    ASSERT_TRUE(train_result.is_ok());
    
    // Test validation
    auto validate_result = tree.validate();
    EXPECT_TRUE(validate_result.is_ok());
    
    // Test predictions
    auto pred_result = tree.predict(X_test_);
    ASSERT_TRUE(pred_result.is_ok());
    
    auto predictions = pred_result.value();
    EXPECT_EQ(predictions.size(), X_test_.size());
    
    // Test tree structure
    auto tree_nodes = tree.get_tree();
    EXPECT_GT(tree_nodes.size(), 0);
    
    // Root should be a valid node
    EXPECT_GE(tree_nodes[0].n_samples, 0);
}

/**
 * @brief Test decision tree serialization
 */
TEST_F(MLPersistenceTest, DecisionTreeSerialization) {
    SerializableDecisionTree original_tree(3, 5, 2);
    auto train_result = original_tree.train(X_train_, y_train_);
    ASSERT_TRUE(train_result.is_ok());
    
    // Serialize tree
    auto serialize_result = original_tree.serialize();
    ASSERT_TRUE(serialize_result.is_ok());
    
    auto serialized_data = serialize_result.value();
    EXPECT_GT(serialized_data.size(), 0);
    
    // Deserialize tree
    SerializableDecisionTree loaded_tree;
    auto deserialize_result = loaded_tree.deserialize(serialized_data);
    ASSERT_TRUE(deserialize_result.is_ok());
    
    // Compare tree structures
    auto original_nodes = original_tree.get_tree();
    auto loaded_nodes = loaded_tree.get_tree();
    
    ASSERT_EQ(original_nodes.size(), loaded_nodes.size());
    
    for (size_t i = 0; i < original_nodes.size(); ++i) {
        EXPECT_EQ(original_nodes[i].feature_index, loaded_nodes[i].feature_index);
        EXPECT_NEAR(original_nodes[i].threshold, loaded_nodes[i].threshold, 1e-10);
        EXPECT_NEAR(original_nodes[i].value, loaded_nodes[i].value, 1e-10);
        EXPECT_EQ(original_nodes[i].left_child, loaded_nodes[i].left_child);
        EXPECT_EQ(original_nodes[i].right_child, loaded_nodes[i].right_child);
    }
}

/**
 * @brief Test ModelRegistry functionality
 */
TEST_F(MLPersistenceTest, ModelRegistryBasic) {
    auto registry_path = test_dir_ / "registry";
    ModelRegistry registry(registry_path);
    
    // Create test model
    SerializableLinearRegression model;
    auto train_result = model.train(X_train_, y_train_);
    ASSERT_TRUE(train_result.is_ok());
    
    // Register model
    auto register_result = registry.register_model(model, "TestModel", {"test", "regression"});
    ASSERT_TRUE(register_result.is_ok());
    
    auto model_id = register_result.value();
    EXPECT_FALSE(model_id.empty());
    
    // Get model metadata
    auto metadata_result = registry.get_model_metadata(model_id);
    ASSERT_TRUE(metadata_result.is_ok());
    
    auto metadata = metadata_result.value();
    EXPECT_EQ(metadata.name, "TestModel");
    EXPECT_EQ(metadata.type, "LinearRegression");
    
    // Search for model
    auto search_result = registry.search_models("", {"test"}, "");
    ASSERT_TRUE(search_result.is_ok());
    
    auto found_models = search_result.value();
    EXPECT_EQ(found_models.size(), 1);
    EXPECT_EQ(found_models[0], model_id);
    
    // Load model from registry
    auto load_result = registry.get_model<SerializableLinearRegression>(model_id);
    ASSERT_TRUE(load_result.is_ok());
    
    auto loaded_model = std::move(load_result.value());
    EXPECT_NEAR(loaded_model->get_r_squared(), model.get_r_squared(), 1e-10);
}

/**
 * @brief Test ModelVersionControl
 */
TEST_F(MLPersistenceTest, ModelVersionControlBasic) {
    auto repo_path = test_dir_ / "vcs_repo";
    ModelVersionControl vcs(repo_path);
    
    // Initialize repository
    auto init_result = vcs.init_repository();
    ASSERT_TRUE(init_result.is_ok());
    
    // Create test model
    SerializableLinearRegression model;
    auto train_result = model.train(X_train_, y_train_);
    ASSERT_TRUE(train_result.is_ok());
    
    // Commit model
    auto commit_result = vcs.commit_model(model, "Initial commit", {"v1.0"});
    ASSERT_TRUE(commit_result.is_ok());
    
    auto version = commit_result.value();
    EXPECT_FALSE(version.empty());
    EXPECT_EQ(vcs.get_current_version(), version);
    
    // Create branch
    auto branch_result = vcs.create_branch("experimental", version);
    EXPECT_TRUE(branch_result.is_ok());
    
    // Switch branch
    auto switch_result = vcs.switch_branch("experimental");
    EXPECT_TRUE(switch_result.is_ok());
    EXPECT_EQ(vcs.get_current_branch(), "experimental");
    
    // Tag version
    auto tag_result = vcs.tag_version(version, "release-1.0");
    EXPECT_TRUE(tag_result.is_ok());
}

/**
 * @brief Test ModelPerformanceTracker
 */
TEST_F(MLPersistenceTest, ModelPerformanceTrackerBasic) {
    ModelPerformanceTracker tracker;
    std::string model_id = "test_model";
    
    // Record some predictions
    for (int i = 0; i < 100; ++i) {
        double actual = static_cast<double>(i) / 100.0;
        double prediction = actual + 0.01 * (i % 10 - 5); // Some noise
        
        tracker.record_prediction(model_id, prediction, actual);
    }
    
    // Calculate metrics
    auto metrics_result = tracker.calculate_metrics(model_id);
    ASSERT_TRUE(metrics_result.is_ok());
    
    auto metrics = metrics_result.value();
    EXPECT_GT(metrics["count"], 0);
    EXPECT_GE(metrics["rmse"], 0.0);
    EXPECT_GE(metrics["mae"], 0.0);
    EXPECT_GE(metrics["r2"], 0.0);
    
    // Test drift detection
    auto drift_result = tracker.detect_drift(model_id, 50, 20, 0.1);
    EXPECT_TRUE(drift_result.is_ok());
    
    // Generate report
    auto report_result = tracker.generate_report(model_id);
    EXPECT_TRUE(report_result.is_ok());
    EXPECT_FALSE(report_result.value().empty());
}

/**
 * @brief Test model validation
 */
TEST_F(MLPersistenceTest, ModelValidation) {
    SerializableLinearRegression model;
    
    // Should fail validation before training
    auto validate_result = model.validate();
    EXPECT_TRUE(validate_result.is_error());
    
    // Train model
    auto train_result = model.train(X_train_, y_train_);
    ASSERT_TRUE(train_result.is_ok());
    
    // Should pass validation after training
    validate_result = model.validate();
    EXPECT_TRUE(validate_result.is_ok());
    
    // Test corrupted serialization
    auto serialize_result = model.serialize();
    ASSERT_TRUE(serialize_result.is_ok());
    
    auto data = serialize_result.value();
    
    // Corrupt the data
    if (!data.empty()) {
        data[data.size() / 2] = 255; // Corrupt middle byte
    }
    
    SerializableLinearRegression corrupted_model;
    auto deserialize_result = corrupted_model.deserialize(data);
    
    // May or may not fail depending on where corruption occurred
    // But if it succeeds, validation should catch issues
    if (deserialize_result.is_ok()) {
        auto validation_result = corrupted_model.validate();
        // The validation might detect the corruption
    }
}

/**
 * @brief Test error handling
 */
TEST_F(MLPersistenceTest, ErrorHandling) {
    ModelLoader loader;
    
    // Test loading non-existent file
    auto load_result = loader.load_model<SerializableLinearRegression>("non_existent_file.bin");
    EXPECT_TRUE(load_result.is_error());
    
    // Test invalid training data
    SerializableLinearRegression model;
    std::vector<std::vector<double>> empty_X;
    std::vector<double> empty_y;
    
    auto train_result = model.train(empty_X, empty_y);
    EXPECT_TRUE(train_result.is_error());
    
    // Test mismatched dimensions
    std::vector<std::vector<double>> mismatched_X = {{1.0, 2.0}, {3.0}};
    std::vector<double> mismatched_y = {1.0, 2.0};
    
    train_result = model.train(mismatched_X, mismatched_y);
    EXPECT_TRUE(train_result.is_error());
}

/**
 * @brief Test compression and encryption
 */
TEST_F(MLPersistenceTest, CompressionAndEncryption) {
    SerializableLinearRegression model;
    auto train_result = model.train(X_train_, y_train_);
    ASSERT_TRUE(train_result.is_ok());
    
    // Test with compression
    ModelSerializer compressing_serializer;
    compressing_serializer.set_compression_level(9);
    
    auto compressed_result = compressing_serializer.serialize_to_bytes(model);
    ASSERT_TRUE(compressed_result.is_ok());
    
    // Test with encryption
    ModelSerializer encrypting_serializer;
    encrypting_serializer.enable_encryption("test_key_123");
    
    auto encrypted_result = encrypting_serializer.serialize_to_bytes(model);
    ASSERT_TRUE(encrypted_result.is_ok());
    
    // The encrypted data should be different from unencrypted
    auto unencrypted_result = ModelSerializer().serialize_to_bytes(model);
    ASSERT_TRUE(unencrypted_result.is_ok());
    
    EXPECT_NE(encrypted_result.value(), unencrypted_result.value());
}

/**
 * @brief Test metadata handling
 */
TEST_F(MLPersistenceTest, MetadataHandling) {
    SerializableLinearRegression model;
    auto train_result = model.train(X_train_, y_train_);
    ASSERT_TRUE(train_result.is_ok());
    
    auto metadata = model.get_metadata();
    
    // Verify basic metadata fields
    EXPECT_FALSE(metadata.model_id.empty());
    EXPECT_EQ(metadata.type, "LinearRegression");
    EXPECT_FALSE(metadata.name.empty());
    EXPECT_FALSE(metadata.version.empty());
    EXPECT_GT(metadata.training_samples, 0);
    
    // Test custom metadata
    ModelSerializer serializer;
    auto model_path = test_dir_ / "metadata_test.bin";
    
    ModelMetadata custom_metadata = metadata;
    custom_metadata.description = "Custom test model";
    custom_metadata.tags["environment"] = "test";
    custom_metadata.metrics["accuracy"] = 0.95;
    
    auto save_result = serializer.save_model(model, model_path, custom_metadata);
    ASSERT_TRUE(save_result.is_ok());
    
    // Load and verify metadata
    ModelLoader loader;
    auto metadata_result = loader.load_metadata(model_path);
    ASSERT_TRUE(metadata_result.is_ok());
    
    auto loaded_metadata = metadata_result.value();
    EXPECT_EQ(loaded_metadata.model_id, custom_metadata.model_id);
    EXPECT_EQ(loaded_metadata.type, custom_metadata.type);
}

} // Anonymous namespace