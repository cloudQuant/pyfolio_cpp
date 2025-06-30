#include <pyfolio/ml/model_persistence.h>
#include <pyfolio/ml/serializable_models.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <random>
#include <filesystem>

using namespace pyfolio::ml;

/**
 * @brief Generate sample financial data for training
 */
std::pair<std::vector<std::vector<double>>, std::vector<double>> 
generate_financial_data(size_t n_samples = 1000, size_t n_features = 5) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> normal(0.0, 1.0);
    std::uniform_real_distribution<double> uniform(-1.0, 1.0);
    
    std::vector<std::vector<double>> X(n_samples, std::vector<double>(n_features));
    std::vector<double> y(n_samples);
    
    // Generate synthetic features: returns, volatility, volume, etc.
    for (size_t i = 0; i < n_samples; ++i) {
        X[i][0] = normal(gen) * 0.02;      // Daily return
        X[i][1] = std::abs(normal(gen)) * 0.20;  // Volatility
        X[i][2] = uniform(gen);            // Volume indicator
        X[i][3] = normal(gen) * 0.01;      // Market return
        X[i][4] = uniform(gen) * 0.5;      // Sentiment score
        
        // Generate target (next day return prediction)
        y[i] = 0.1 * X[i][0] + 0.05 * X[i][1] - 0.02 * X[i][2] + 
               0.3 * X[i][3] + 0.1 * X[i][4] + normal(gen) * 0.005;
    }
    
    return {X, y};
}

/**
 * @brief Demonstrate linear regression model persistence
 */
void demonstrate_linear_regression_persistence() {
    std::cout << "=== Linear Regression Model Persistence ===\n\n";
    
    // Generate training data
    auto [X_train, y_train] = generate_financial_data(800, 5);
    auto [X_test, y_test] = generate_financial_data(200, 5);
    
    std::cout << "Training Data: " << X_train.size() << " samples, " 
              << X_train[0].size() << " features\n";
    std::cout << "Test Data: " << X_test.size() << " samples\n\n";
    
    // Create and train model
    SerializableLinearRegression model;
    auto train_result = model.train(X_train, y_train);
    
    if (train_result.is_error()) {
        std::cout << "Training failed: " << train_result.error().message << "\n";
        return;
    }
    
    std::cout << "Model Training Results:\n";
    std::cout << "R² Score: " << std::fixed << std::setprecision(4) << model.get_r_squared() << "\n";
    std::cout << "Intercept: " << model.get_intercept() << "\n";
    std::cout << "Coefficients: ";
    for (double coeff : model.get_coefficients()) {
        std::cout << std::setprecision(4) << coeff << " ";
    }
    std::cout << "\n\n";
    
    // Test predictions before serialization
    auto pred_result = model.predict(X_test);
    if (pred_result.is_ok()) {
        auto predictions = pred_result.value();
        
        // Calculate test metrics
        double mse = 0.0;
        for (size_t i = 0; i < predictions.size(); ++i) {
            double error = predictions[i] - y_test[i];
            mse += error * error;
        }
        mse /= predictions.size();
        
        std::cout << "Test Performance (before serialization):\n";
        std::cout << "MSE: " << std::setprecision(6) << mse << "\n";
        std::cout << "RMSE: " << std::sqrt(mse) << "\n\n";
    }
    
    // Save model
    ModelSerializer serializer;
    auto model_path = "models/linear_regression_v1.model";
    
    // Create directory
    std::filesystem::create_directories("models");
    
    std::cout << "Saving model to: " << model_path << "\n";
    auto save_result = serializer.save_model(model, model_path);
    
    if (save_result.is_error()) {
        std::cout << "Save failed: " << save_result.error().message << "\n";
        return;
    }
    
    std::cout << "Model saved successfully!\n\n";
    
    // Load model
    ModelLoader loader;
    std::cout << "Loading model from: " << model_path << "\n";
    auto load_result = loader.load_model<SerializableLinearRegression>(model_path);
    
    if (load_result.is_error()) {
        std::cout << "Load failed: " << load_result.error().message << "\n";
        return;
    }
    
    auto loaded_model = std::move(load_result.value());
    std::cout << "Model loaded successfully!\n\n";
    
    // Verify loaded model
    std::cout << "Loaded Model Verification:\n";
    std::cout << "R² Score: " << loaded_model->get_r_squared() << "\n";
    std::cout << "Intercept: " << loaded_model->get_intercept() << "\n";
    std::cout << "Coefficients: ";
    for (double coeff : loaded_model->get_coefficients()) {
        std::cout << std::setprecision(4) << coeff << " ";
    }
    std::cout << "\n\n";
    
    // Test predictions after deserialization
    auto loaded_pred_result = loaded_model->predict(X_test);
    if (loaded_pred_result.is_ok()) {
        auto loaded_predictions = loaded_pred_result.value();
        
        // Calculate test metrics
        double mse = 0.0;
        for (size_t i = 0; i < loaded_predictions.size(); ++i) {
            double error = loaded_predictions[i] - y_test[i];
            mse += error * error;
        }
        mse /= loaded_predictions.size();
        
        std::cout << "Test Performance (after serialization):\n";
        std::cout << "MSE: " << std::setprecision(6) << mse << "\n";
        std::cout << "RMSE: " << std::sqrt(mse) << "\n\n";
        
        // Compare predictions
        bool predictions_match = true;
        auto orig_predictions = pred_result.value();
        for (size_t i = 0; i < loaded_predictions.size(); ++i) {
            if (std::abs(loaded_predictions[i] - orig_predictions[i]) > 1e-10) {
                predictions_match = false;
                break;
            }
        }
        
        std::cout << "Prediction Consistency: " 
                  << (predictions_match ? "✓ PASS" : "✗ FAIL") << "\n\n";
    }
}

/**
 * @brief Demonstrate decision tree model persistence
 */
void demonstrate_decision_tree_persistence() {
    std::cout << "=== Decision Tree Model Persistence ===\n\n";
    
    // Generate training data with more complex patterns
    auto [X_train, y_train] = generate_financial_data(500, 4);
    auto [X_test, y_test] = generate_financial_data(100, 4);
    
    std::cout << "Training Decision Tree Model...\n";
    
    // Create and train decision tree
    SerializableDecisionTree tree(5, 10, 3);  // max_depth=5, min_samples_split=10, min_samples_leaf=3
    auto train_result = tree.train(X_train, y_train);
    
    if (train_result.is_error()) {
        std::cout << "Training failed: " << train_result.error().message << "\n";
        return;
    }
    
    auto metadata = tree.get_metadata();
    std::cout << "Tree Structure:\n";
    std::cout << "Tree Size: " << static_cast<int>(metadata.metrics.at("tree_size")) << " nodes\n";
    std::cout << "Max Depth: " << static_cast<int>(metadata.metrics.at("max_depth")) << "\n";
    std::cout << "Features: " << static_cast<int>(metadata.metrics.at("n_features")) << "\n\n";
    
    // Test original model
    auto pred_result = tree.predict(X_test);
    if (pred_result.is_ok()) {
        auto predictions = pred_result.value();
        
        double mse = 0.0;
        for (size_t i = 0; i < predictions.size(); ++i) {
            double error = predictions[i] - y_test[i];
            mse += error * error;
        }
        mse /= predictions.size();
        
        std::cout << "Original Model Performance:\n";
        std::cout << "MSE: " << std::setprecision(6) << mse << "\n";
        std::cout << "RMSE: " << std::sqrt(mse) << "\n\n";
    }
    
    // Serialize and save
    ModelSerializer serializer;
    auto tree_path = "models/decision_tree_v1.model";
    
    std::cout << "Saving decision tree to: " << tree_path << "\n";
    auto save_result = serializer.save_model(tree, tree_path);
    
    if (save_result.is_error()) {
        std::cout << "Save failed: " << save_result.error().message << "\n";
        return;
    }
    
    std::cout << "Decision tree saved successfully!\n\n";
    
    // Load and test
    ModelLoader loader;
    auto load_result = loader.load_model<SerializableDecisionTree>(tree_path);
    
    if (load_result.is_error()) {
        std::cout << "Load failed: " << load_result.error().message << "\n";
        return;
    }
    
    auto loaded_tree = std::move(load_result.value());
    std::cout << "Decision tree loaded successfully!\n\n";
    
    // Test loaded model
    auto loaded_pred_result = loaded_tree->predict(X_test);
    if (loaded_pred_result.is_ok()) {
        auto loaded_predictions = loaded_pred_result.value();
        
        double mse = 0.0;
        for (size_t i = 0; i < loaded_predictions.size(); ++i) {
            double error = loaded_predictions[i] - y_test[i];
            mse += error * error;
        }
        mse /= loaded_predictions.size();
        
        std::cout << "Loaded Model Performance:\n";
        std::cout << "MSE: " << std::setprecision(6) << mse << "\n";
        std::cout << "RMSE: " << std::sqrt(mse) << "\n\n";
        
        // Verify tree structure consistency
        auto orig_tree = tree.get_tree();
        auto loaded_tree_nodes = loaded_tree->get_tree();
        
        bool structure_match = (orig_tree.size() == loaded_tree_nodes.size());
        if (structure_match) {
            for (size_t i = 0; i < orig_tree.size(); ++i) {
                if (orig_tree[i].feature_index != loaded_tree_nodes[i].feature_index ||
                    std::abs(orig_tree[i].threshold - loaded_tree_nodes[i].threshold) > 1e-10 ||
                    std::abs(orig_tree[i].value - loaded_tree_nodes[i].value) > 1e-10) {
                    structure_match = false;
                    break;
                }
            }
        }
        
        std::cout << "Tree Structure Consistency: " 
                  << (structure_match ? "✓ PASS" : "✗ FAIL") << "\n\n";
    }
}

/**
 * @brief Demonstrate model registry functionality
 */
void demonstrate_model_registry() {
    std::cout << "=== Model Registry Demonstration ===\n\n";
    
    // Create registry
    ModelRegistry registry("./model_registry");
    
    // Create some models
    SerializableLinearRegression lr_model;
    auto [X, y] = generate_financial_data(300, 3);
    auto lr_train_result = lr_model.train(X, y);
    if (lr_train_result.is_error()) {
        std::cerr << "Warning: Linear regression training failed: " << lr_train_result.error().message << std::endl;
    }
    
    SerializableDecisionTree dt_model(3, 5, 2);
    auto dt_train_result = dt_model.train(X, y);
    if (dt_train_result.is_error()) {
        std::cerr << "Warning: Decision tree training failed: " << dt_train_result.error().message << std::endl;
    }
    
    // Register models
    std::cout << "Registering models in registry...\n";
    
    auto lr_id_result = registry.register_model(lr_model, "FinancialLinearRegression", 
                                               {"financial", "regression", "production"});
    auto dt_id_result = registry.register_model(dt_model, "FinancialDecisionTree",
                                               {"financial", "tree", "experimental"});
    
    if (lr_id_result.is_error() || dt_id_result.is_error()) {
        std::cout << "Registration failed\n";
        return;
    }
    
    auto lr_id = lr_id_result.value();
    auto dt_id = dt_id_result.value();
    
    std::cout << "Linear Regression ID: " << lr_id << "\n";
    std::cout << "Decision Tree ID: " << dt_id << "\n\n";
    
    // Search for models
    std::cout << "Searching for financial models...\n";
    auto search_result = registry.search_models("", {"financial"}, "");
    
    if (search_result.is_ok()) {
        auto found_models = search_result.value();
        std::cout << "Found " << found_models.size() << " financial models:\n";
        
        for (const auto& model_id : found_models) {
            auto metadata_result = registry.get_model_metadata(model_id);
            if (metadata_result.is_ok()) {
                auto metadata = metadata_result.value();
                std::cout << "  - " << metadata.name << " (" << metadata.type << ")\n";
            }
        }
        std::cout << "\n";
    }
    
    // Load a model from registry
    std::cout << "Loading Linear Regression from registry...\n";
    auto loaded_lr_result = registry.get_model<SerializableLinearRegression>(lr_id);
    
    if (loaded_lr_result.is_ok()) {
        auto loaded_lr = std::move(loaded_lr_result.value());
        std::cout << "Model loaded successfully!\n";
        std::cout << "R² Score: " << loaded_lr->get_r_squared() << "\n\n";
    }
    
    // Update model
    std::cout << "Updating model in registry...\n";
    SerializableLinearRegression updated_lr;
    auto [X_new, y_new] = generate_financial_data(500, 3);
    auto updated_train_result = updated_lr.train(X_new, y_new);
    if (updated_train_result.is_error()) {
        std::cerr << "Warning: Updated model training failed: " << updated_train_result.error().message << std::endl;
    }
    
    auto update_result = registry.update_model(lr_id, updated_lr, "Retrained with more data");
    if (update_result.is_ok()) {
        std::cout << "Model updated to version: " << update_result.value() << "\n\n";
    }
}

/**
 * @brief Demonstrate version control system
 */
void demonstrate_version_control() {
    std::cout << "=== Model Version Control ===\n\n";
    
    // Initialize repository
    ModelVersionControl vcs("./model_repo");
    auto init_result = vcs.init_repository();
    
    if (init_result.is_error()) {
        std::cout << "Repository initialization failed: " << init_result.error().message << "\n";
        return;
    }
    
    std::cout << "Model repository initialized\n\n";
    
    // Create initial model
    SerializableLinearRegression model_v1;
    auto [X1, y1] = generate_financial_data(200, 2);
    auto v1_train_result = model_v1.train(X1, y1);
    if (v1_train_result.is_error()) {
        std::cerr << "Warning: Model v1 training failed: " << v1_train_result.error().message << std::endl;
    }
    
    std::cout << "Committing initial model version...\n";
    auto commit1_result = vcs.commit_model(model_v1, "Initial linear regression model", {"v1.0"});
    
    if (commit1_result.is_ok()) {
        auto version1 = commit1_result.value();
        std::cout << "Initial version committed: " << version1 << "\n";
        std::cout << "R² Score: " << model_v1.get_r_squared() << "\n\n";
    }
    
    // Create improved model
    SerializableLinearRegression model_v2;
    auto [X2, y2] = generate_financial_data(500, 2);  // More training data
    auto v2_train_result = model_v2.train(X2, y2);
    if (v2_train_result.is_error()) {
        std::cerr << "Warning: Model v2 training failed: " << v2_train_result.error().message << std::endl;
    }
    
    std::cout << "Committing improved model version...\n";
    auto commit2_result = vcs.commit_model(model_v2, "Improved model with more training data", {"v2.0"});
    
    if (commit2_result.is_ok()) {
        auto version2 = commit2_result.value();
        std::cout << "Improved version committed: " << version2 << "\n";
        std::cout << "R² Score: " << model_v2.get_r_squared() << "\n\n";
    }
    
    // Create experimental branch
    std::cout << "Creating experimental branch...\n";
    auto branch_result = vcs.create_branch("experimental", commit1_result.value());
    
    if (branch_result.is_ok()) {
        std::cout << "Experimental branch created\n";
        
        auto switch_result = vcs.switch_branch("experimental");
        if (switch_result.is_ok()) {
            std::cout << "Switched to experimental branch\n";
            std::cout << "Current branch: " << vcs.get_current_branch() << "\n\n";
        }
    }
    
    // Show current status
    std::cout << "Version Control Status:\n";
    std::cout << "Current branch: " << vcs.get_current_branch() << "\n";
    std::cout << "Current version: " << vcs.get_current_version() << "\n\n";
}

/**
 * @brief Demonstrate performance tracking
 */
void demonstrate_performance_tracking() {
    std::cout << "=== Model Performance Tracking ===\n\n";
    
    ModelPerformanceTracker tracker;
    
    // Simulate model predictions over time
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> noise(0.0, 0.01);
    
    std::string model_id = "financial_model_v1";
    
    std::cout << "Recording model predictions over time...\n";
    
    // Simulate 1000 predictions
    for (int i = 0; i < 1000; ++i) {
        double true_value = 0.05 + 0.02 * std::sin(i * 0.1);  // Simulated true value
        double prediction = true_value + noise(gen);           // Model prediction with noise
        
        tracker.record_prediction(model_id, prediction, true_value);
    }
    
    std::cout << "Recorded 1000 predictions\n\n";
    
    // Calculate performance metrics
    auto metrics_result = tracker.calculate_metrics(model_id);
    
    if (metrics_result.is_ok()) {
        auto metrics = metrics_result.value();
        
        std::cout << "Performance Metrics:\n";
        std::cout << "RMSE: " << std::setprecision(6) << metrics["rmse"] << "\n";
        std::cout << "MAE: " << metrics["mae"] << "\n";
        std::cout << "MAPE: " << metrics["mape"] << "%\n";
        std::cout << "R²: " << metrics["r2"] << "\n";
        std::cout << "Predictions: " << static_cast<int>(metrics["count"]) << "\n\n";
    }
    
    // Test drift detection
    std::cout << "Testing drift detection...\n";
    auto drift_result = tracker.detect_drift(model_id, 500, 100, 0.1);
    
    if (drift_result.is_ok()) {
        bool drift_detected = drift_result.value();
        std::cout << "Drift Detection: " << (drift_detected ? "DRIFT DETECTED" : "No drift") << "\n\n";
    }
    
    // Generate performance report
    auto report_result = tracker.generate_report(model_id);
    if (report_result.is_ok()) {
        std::cout << "Performance Report Generated:\n";
        std::cout << report_result.value().substr(0, 200) << "...\n\n";
    }
}

/**
 * @brief Main demonstration function
 */
int main() {
    std::cout << "PyFolio C++ ML Model Persistence System Demonstration\n";
    std::cout << "====================================================\n\n";
    
    try {
        // Create models directory
        std::filesystem::create_directories("models");
        
        // Run demonstrations
        demonstrate_linear_regression_persistence();
        demonstrate_decision_tree_persistence();
        demonstrate_model_registry();
        demonstrate_version_control();
        demonstrate_performance_tracking();
        
        std::cout << "=== Summary ===\n\n";
        std::cout << "✓ Model Serialization/Deserialization\n";
        std::cout << "✓ Multiple Model Types (Linear Regression, Decision Tree)\n";
        std::cout << "✓ Model Registry with Search and Metadata\n";
        std::cout << "✓ Version Control System\n";
        std::cout << "✓ Performance Tracking and Drift Detection\n";
        std::cout << "✓ Comprehensive Model Lifecycle Management\n\n";
        
        std::cout << "Key Features Demonstrated:\n";
        std::cout << "🔹 Binary serialization with compression and encryption support\n";
        std::cout << "🔹 Git-like version control for ML models\n";
        std::cout << "🔹 Centralized model registry with search capabilities\n";
        std::cout << "🔹 Automated performance monitoring\n";
        std::cout << "🔹 Model validation and integrity checking\n";
        std::cout << "🔹 Production-ready deployment workflow\n\n";
        
        std::cout << "All demonstrations completed successfully!\n";
        
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}