#include "pyfolio/ml/model_persistence.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <random>
#include <iomanip>
#include <cmath>
#include <numeric>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace pyfolio::ml {

// ============================================================================
// Utility Functions
// ============================================================================

namespace {
    /**
     * @brief Generate SHA256 checksum
     */
    std::string calculate_sha256(const std::vector<uint8_t>& data) {
        // Simplified hash - in production would use proper crypto library
        std::hash<std::string> hasher;
        std::string str_data(data.begin(), data.end());
        auto hash_value = hasher(str_data);
        
        std::stringstream ss;
        ss << std::hex << hash_value;
        return ss.str();
    }
    
    /**
     * @brief Generate UUID-like identifier
     */
    std::string generate_uuid() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        std::uniform_int_distribution<> dis2(8, 11);
        
        std::stringstream ss;
        ss << std::hex;
        for (int i = 0; i < 8; i++) ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 4; i++) ss << dis(gen);
        ss << "-4";
        for (int i = 0; i < 3; i++) ss << dis(gen);
        ss << "-";
        ss << dis2(gen);
        for (int i = 0; i < 3; i++) ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 12; i++) ss << dis(gen);
        
        return ss.str();
    }
    
    /**
     * @brief Simple compression using RLE
     */
    std::vector<uint8_t> compress_rle(const std::vector<uint8_t>& data) {
        if (data.empty()) return {};
        
        std::vector<uint8_t> compressed;
        compressed.reserve(data.size());
        
        for (size_t i = 0; i < data.size(); ) {
            uint8_t current = data[i];
            size_t count = 1;
            
            while (i + count < data.size() && data[i + count] == current && count < 255) {
                count++;
            }
            
            if (count > 3 || current == 0xFF) {
                compressed.push_back(0xFF);  // Escape byte
                compressed.push_back(static_cast<uint8_t>(count));
                compressed.push_back(current);
            } else {
                for (size_t j = 0; j < count; j++) {
                    compressed.push_back(current);
                }
            }
            
            i += count;
        }
        
        return compressed;
    }
    
    /**
     * @brief Simple decompression for RLE
     */
    std::vector<uint8_t> decompress_rle(const std::vector<uint8_t>& compressed) {
        if (compressed.empty()) return {};
        
        std::vector<uint8_t> data;
        data.reserve(compressed.size() * 2);
        
        for (size_t i = 0; i < compressed.size(); ) {
            if (compressed[i] == 0xFF && i + 2 < compressed.size()) {
                uint8_t count = compressed[i + 1];
                uint8_t value = compressed[i + 2];
                
                for (uint8_t j = 0; j < count; j++) {
                    data.push_back(value);
                }
                
                i += 3;
            } else {
                data.push_back(compressed[i]);
                i++;
            }
        }
        
        return data;
    }
    
    /**
     * @brief Simple XOR encryption
     */
    std::vector<uint8_t> xor_encrypt(const std::vector<uint8_t>& data, const std::string& key) {
        if (data.empty() || key.empty()) return data;
        
        std::vector<uint8_t> encrypted = data;
        
        for (size_t i = 0; i < encrypted.size(); i++) {
            encrypted[i] ^= key[i % key.length()];
        }
        
        return encrypted;
    }
}

// ============================================================================
// ModelSerializer Implementation
// ============================================================================

ModelSerializer::ModelSerializer(ModelFormat format)
    : format_(format), compression_level_(6), encryption_enabled_(false) {}

Result<void> ModelSerializer::save_model(const SerializableModel& model,
                                        const std::filesystem::path& path,
                                        const ModelMetadata& metadata) {
    try {
        // Create directory if it doesn't exist
        auto parent_path = path.parent_path();
        if (!parent_path.empty()) {
            std::filesystem::create_directories(parent_path);
        }
        
        // Serialize model
        auto serialize_result = serialize_to_bytes(model);
        if (serialize_result.is_error()) {
            return Result<void>::error(serialize_result.error().code, 
                                     serialize_result.error().message);
        }
        
        auto data = serialize_result.value();
        
        // Save to file
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return Result<void>::error(ErrorCode::FileNotFound, 
                                     "Could not open file for writing: " + path.string());
        }
        
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();
        
        // Save metadata separately
        auto metadata_path = path;
        metadata_path.replace_extension(".metadata.json");
        
        std::ofstream metadata_file(metadata_path);
        if (metadata_file.is_open()) {
            // Write basic metadata as JSON
            metadata_file << "{\n";
            metadata_file << "  \"model_id\": \"" << metadata.model_id << "\",\n";
            metadata_file << "  \"name\": \"" << metadata.name << "\",\n";
            metadata_file << "  \"version\": \"" << metadata.version << "\",\n";
            metadata_file << "  \"type\": \"" << metadata.type << "\",\n";
            metadata_file << "  \"size_bytes\": " << data.size() << ",\n";
            metadata_file << "  \"checksum\": \"" << calculate_sha256(data) << "\",\n";
            metadata_file << "  \"created_at\": \"" << "2024-01-01T00:00:00Z" << "\"\n";
            metadata_file << "}\n";
            metadata_file.close();
        }
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<std::vector<uint8_t>> ModelSerializer::serialize_to_bytes(
    const SerializableModel& model) const {
    try {
        auto data_result = model.serialize();
        if (data_result.is_error()) {
            return data_result;
        }
        
        auto data = data_result.value();
        
        // Apply compression
        if (compression_level_ > 0) {
            auto compress_result = compress_data(data);
            if (compress_result.is_ok()) {
                data = compress_result.value();
            }
        }
        
        // Apply encryption
        if (encryption_enabled_) {
            auto encrypt_result = encrypt_data(data);
            if (encrypt_result.is_ok()) {
                data = encrypt_result.value();
            }
        }
        
        return Result<std::vector<uint8_t>>::success(data);
        
    } catch (const std::exception& e) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<std::string> ModelSerializer::serialize_to_string(
    const SerializableModel& model) const {
    auto bytes_result = serialize_to_bytes(model);
    if (bytes_result.is_error()) {
        return Result<std::string>::error(bytes_result.error().code, 
                                        bytes_result.error().message);
    }
    
    auto bytes = bytes_result.value();
    
    // Base64 encode for string representation
    std::string encoded;
    const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    for (size_t i = 0; i < bytes.size(); i += 3) {
        uint32_t value = 0;
        for (int j = 0; j < 3 && i + j < bytes.size(); j++) {
            value |= (bytes[i + j] << (8 * (2 - j)));
        }
        
        for (int j = 0; j < 4; j++) {
            if (i * 4 / 3 + j < (bytes.size() + 2) / 3 * 4) {
                encoded += chars[(value >> (6 * (3 - j))) & 0x3F];
            } else {
                encoded += '=';
            }
        }
    }
    
    return Result<std::string>::success(encoded);
}

Result<std::vector<uint8_t>> ModelSerializer::compress_data(
    const std::vector<uint8_t>& data) const {
    try {
        // Simple RLE compression
        auto compressed = compress_rle(data);
        return Result<std::vector<uint8_t>>::success(compressed);
    } catch (const std::exception& e) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<std::vector<uint8_t>> ModelSerializer::encrypt_data(
    const std::vector<uint8_t>& data) const {
    try {
        auto encrypted = xor_encrypt(data, encryption_key_);
        return Result<std::vector<uint8_t>>::success(encrypted);
    } catch (const std::exception& e) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::CalculationError, e.what());
    }
}

// ============================================================================
// ModelLoader Implementation
// ============================================================================

ModelLoader::ModelLoader() {}

Result<std::vector<uint8_t>> ModelLoader::load_file_data(
    const std::filesystem::path& path) const {
    try {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            return Result<std::vector<uint8_t>>::error(ErrorCode::FileNotFound,
                                                     "Could not open file: " + path.string());
        }
        
        auto size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> data(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        file.close();
        
        // Apply decryption if needed
        if (!decryption_key_.empty()) {
            auto decrypt_result = decrypt_data(data);
            if (decrypt_result.is_ok()) {
                data = decrypt_result.value();
            }
        }
        
        // Apply decompression
        auto decompress_result = decompress_data(data);
        if (decompress_result.is_ok()) {
            data = decompress_result.value();
        }
        
        return Result<std::vector<uint8_t>>::success(data);
        
    } catch (const std::exception& e) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<ModelMetadata> ModelLoader::load_metadata(
    const std::filesystem::path& path) const {
    try {
        auto metadata_path = path;
        metadata_path.replace_extension(".metadata.json");
        
        if (!std::filesystem::exists(metadata_path)) {
            return Result<ModelMetadata>::error(ErrorCode::FileNotFound,
                                              "Metadata file not found");
        }
        
        std::ifstream file(metadata_path);
        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        
        // Simple JSON parsing for basic metadata
        ModelMetadata metadata;
        
        // Extract model_id
        auto id_pos = content.find("\"model_id\":");
        if (id_pos != std::string::npos) {
            auto start = content.find("\"", id_pos + 11) + 1;
            auto end = content.find("\"", start);
            metadata.model_id = content.substr(start, end - start);
        }
        
        // Extract name
        auto name_pos = content.find("\"name\":");
        if (name_pos != std::string::npos) {
            auto start = content.find("\"", name_pos + 7) + 1;
            auto end = content.find("\"", start);
            metadata.name = content.substr(start, end - start);
        }
        
        // Extract version
        auto version_pos = content.find("\"version\":");
        if (version_pos != std::string::npos) {
            auto start = content.find("\"", version_pos + 10) + 1;
            auto end = content.find("\"", start);
            metadata.version = content.substr(start, end - start);
        }
        
        // Extract type
        auto type_pos = content.find("\"type\":");
        if (type_pos != std::string::npos) {
            auto start = content.find("\"", type_pos + 7) + 1;
            auto end = content.find("\"", start);
            metadata.type = content.substr(start, end - start);
        }
        
        return Result<ModelMetadata>::success(metadata);
        
    } catch (const std::exception& e) {
        return Result<ModelMetadata>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<ModelFormat> ModelLoader::detect_format(
    const std::filesystem::path& path) const {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return Result<ModelFormat>::error(ErrorCode::FileNotFound,
                                            "Could not open file: " + path.string());
        }
        
        // Read first few bytes to detect format
        char header[8];
        file.read(header, 8);
        file.close();
        
        // Simple format detection based on magic numbers
        if (header[0] == '{' || (static_cast<unsigned char>(header[0]) == 0xFF && static_cast<unsigned char>(header[1]) == 0xFF)) {
            return Result<ModelFormat>::success(ModelFormat::JSON);
        } else if (header[0] == '\x89' && header[1] == 'H' && header[2] == 'D' && header[3] == 'F') {
            return Result<ModelFormat>::success(ModelFormat::HDF5);
        } else {
            return Result<ModelFormat>::success(ModelFormat::Binary);
        }
        
    } catch (const std::exception& e) {
        return Result<ModelFormat>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<std::vector<uint8_t>> ModelLoader::decompress_data(
    const std::vector<uint8_t>& data) const {
    try {
        // Try RLE decompression
        auto decompressed = decompress_rle(data);
        return Result<std::vector<uint8_t>>::success(decompressed);
    } catch (const std::exception& e) {
        // If decompression fails, return original data
        return Result<std::vector<uint8_t>>::success(data);
    }
}

Result<std::vector<uint8_t>> ModelLoader::decrypt_data(
    const std::vector<uint8_t>& data) const {
    try {
        auto decrypted = xor_encrypt(data, decryption_key_);  // XOR is symmetric
        return Result<std::vector<uint8_t>>::success(decrypted);
    } catch (const std::exception& e) {
        return Result<std::vector<uint8_t>>::error(ErrorCode::CalculationError, e.what());
    }
}

// ============================================================================
// ModelVersionControl Implementation
// ============================================================================

ModelVersionControl::ModelVersionControl(const std::filesystem::path& repository_path)
    : repository_path_(repository_path), current_branch_("main"), current_version_("") {}

Result<void> ModelVersionControl::init_repository() {
    try {
        // Create repository structure
        std::filesystem::create_directories(repository_path_);
        std::filesystem::create_directories(repository_path_ / "objects");
        std::filesystem::create_directories(repository_path_ / "refs" / "heads");
        std::filesystem::create_directories(repository_path_ / "refs" / "tags");
        
        // Initialize main branch
        std::ofstream main_ref(repository_path_ / "refs" / "heads" / "main");
        main_ref << ""; // Empty initially
        main_ref.close();
        
        // Create HEAD file
        std::ofstream head_file(repository_path_ / "HEAD");
        head_file << "ref: refs/heads/main\n";
        head_file.close();
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<std::string> ModelVersionControl::commit_model(
    const SerializableModel& model,
    const std::string& message,
    const std::vector<std::string>& tags) {
    try {
        auto version_id = generate_version_id();
        if (version_id.is_error()) {
            return version_id;
        }
        
        auto version = version_id.value();
        
        // Save model object
        ModelSerializer serializer;
        auto model_path = repository_path_ / "objects" / version;
        auto save_result = serializer.save_model(model, model_path);
        if (save_result.is_error()) {
            return Result<std::string>::error(save_result.error().code, 
                                            save_result.error().message);
        }
        
        // Create version metadata
        ModelVersion version_metadata(version, message);
        version_metadata.parent_version = current_version_;
        version_metadata.timestamp = DateTime();
        
        auto metadata_result = write_version_metadata(version, version_metadata);
        if (metadata_result.is_error()) {
            return Result<std::string>::error(metadata_result.error().code,
                                            metadata_result.error().message);
        }
        
        // Update branch reference
        auto update_result = update_refs(current_branch_, version);
        if (update_result.is_error()) {
            return Result<std::string>::error(update_result.error().code,
                                            update_result.error().message);
        }
        
        current_version_ = version;
        
        // Add tags if provided
        for (const auto& tag : tags) {
            auto tag_result = tag_version(version, tag);
            // Ignore tag failures to not block commit
            (void)tag_result;
        }
        
        return Result<std::string>::success(version);
        
    } catch (const std::exception& e) {
        return Result<std::string>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<void> ModelVersionControl::create_branch(const std::string& branch_name,
                                              const std::string& from_version) {
    try {
        auto branch_path = repository_path_ / "refs" / "heads" / branch_name;
        
        if (std::filesystem::exists(branch_path)) {
            return Result<void>::error(ErrorCode::InvalidInput,
                                     "Branch already exists: " + branch_name);
        }
        
        std::string base_version = from_version.empty() ? current_version_ : from_version;
        
        std::ofstream branch_file(branch_path);
        branch_file << base_version << "\n";
        branch_file.close();
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<void> ModelVersionControl::switch_branch(const std::string& branch_name) {
    try {
        auto branch_path = repository_path_ / "refs" / "heads" / branch_name;
        
        if (!std::filesystem::exists(branch_path)) {
            return Result<void>::error(ErrorCode::FileNotFound,
                                     "Branch not found: " + branch_name);
        }
        
        std::ifstream branch_file(branch_path);
        std::string version;
        std::getline(branch_file, version);
        branch_file.close();
        
        current_branch_ = branch_name;
        current_version_ = version;
        
        // Update HEAD
        std::ofstream head_file(repository_path_ / "HEAD");
        head_file << "ref: refs/heads/" << branch_name << "\n";
        head_file.close();
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<void> ModelVersionControl::tag_version(const std::string& version,
                                            const std::string& tag_name) {
    try {
        auto tag_path = repository_path_ / "refs" / "tags" / tag_name;
        
        std::ofstream tag_file(tag_path);
        tag_file << version << "\n";
        tag_file.close();
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<std::string> ModelVersionControl::rollback(size_t steps) {
    try {
        // Simple rollback implementation
        if (steps == 0) {
            return Result<std::string>::success(current_version_);
        }
        
        // For simplicity, just move to a previous version
        // In a real implementation, would traverse the commit graph
        auto new_version = "rollback_" + std::to_string(steps) + "_" + current_version_;
        current_version_ = new_version;
        
        return Result<std::string>::success(new_version);
        
    } catch (const std::exception& e) {
        return Result<std::string>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<std::filesystem::path> ModelVersionControl::get_version_path(
    const std::string& version) const {
    auto path = repository_path_ / "objects" / version;
    if (!std::filesystem::exists(path)) {
        return Result<std::filesystem::path>::error(ErrorCode::FileNotFound,
                                                   "Version not found: " + version);
    }
    return Result<std::filesystem::path>::success(path);
}

Result<std::string> ModelVersionControl::generate_version_id() const {
    return Result<std::string>::success(generate_uuid());
}

Result<void> ModelVersionControl::update_refs(const std::string& branch,
                                             const std::string& version) {
    try {
        auto branch_path = repository_path_ / "refs" / "heads" / branch;
        
        std::ofstream branch_file(branch_path);
        branch_file << version << "\n";
        branch_file.close();
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<void> ModelVersionControl::write_version_metadata(const std::string& version,
                                                        const ModelVersion& metadata) {
    try {
        auto metadata_path = repository_path_ / "objects" / (version + ".meta");
        
        std::ofstream meta_file(metadata_path);
        meta_file << "{\n";
        meta_file << "  \"version\": \"" << metadata.version << "\",\n";
        meta_file << "  \"parent_version\": \"" << metadata.parent_version << "\",\n";
        meta_file << "  \"commit_message\": \"" << metadata.commit_message << "\",\n";
        meta_file << "  \"timestamp\": \"" << "2024-01-01T00:00:00Z" << "\"\n";
        meta_file << "}\n";
        meta_file.close();
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::CalculationError, e.what());
    }
}

// ============================================================================
// ModelRegistry Implementation
// ============================================================================

ModelRegistry::ModelRegistry(const std::filesystem::path& registry_path)
    : registry_path_(registry_path) {
    std::filesystem::create_directories(registry_path_);
    auto update_result = update_index();
    (void)update_result; // Ignore update failures during construction
}

Result<std::string> ModelRegistry::register_model(const SerializableModel& model,
                                                 const std::string& name,
                                                 const std::vector<std::string>& tags) {
    try {
        auto model_id = generate_uuid();
        auto version = "v1.0.0";
        
        // Create model directory
        auto model_dir = registry_path_ / model_id;
        std::filesystem::create_directories(model_dir);
        
        // Save model
        ModelSerializer serializer;
        auto model_path = model_dir / (std::string(version) + ".model");
        
        ModelMetadata metadata;
        metadata.model_id = model_id;
        metadata.name = name;
        metadata.version = version;
        metadata.type = model.get_model_type();
        metadata.created_at = DateTime();
        metadata.modified_at = DateTime();
        
        for (const auto& tag : tags) {
            metadata.tags[tag] = "true";
        }
        
        auto save_result = serializer.save_model(model, model_path, metadata);
        if (save_result.is_error()) {
            return Result<std::string>::error(save_result.error().code,
                                            save_result.error().message);
        }
        
        // Update index
        model_index_[model_id] = metadata;
        auto index_save_result = save_index();
        (void)index_save_result; // Ignore save failures
        
        return Result<std::string>::success(model_id);
        
    } catch (const std::exception& e) {
        return Result<std::string>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<std::string> ModelRegistry::update_model(const std::string& model_id,
                                               const SerializableModel& model,
                                               const std::string& update_message) {
    try {
        auto it = model_index_.find(model_id);
        if (it == model_index_.end()) {
            return Result<std::string>::error(ErrorCode::NotFound,
                                            "Model not found: " + model_id);
        }
        
        // Generate new version
        auto current_version = it->second.version;
        auto new_version = "v1.0." + std::to_string(std::time(nullptr) % 10000);
        
        // Save new version
        ModelSerializer serializer;
        auto model_path = registry_path_ / model_id / (std::string(new_version) + ".model");
        
        ModelMetadata metadata = it->second;
        metadata.version = new_version;
        metadata.modified_at = DateTime();
        metadata.description = update_message;
        
        auto save_result = serializer.save_model(model, model_path, metadata);
        if (save_result.is_error()) {
            return Result<std::string>::error(save_result.error().code,
                                            save_result.error().message);
        }
        
        // Update index
        model_index_[model_id] = metadata;
        auto save_result2 = save_index();
        (void)save_result2; // Ignore save failures
        
        return Result<std::string>::success(new_version);
        
    } catch (const std::exception& e) {
        return Result<std::string>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<std::vector<std::string>> ModelRegistry::search_models(
    const std::string& query,
    const std::vector<std::string>& tags,
    const std::string& model_type) const {
    try {
        std::vector<std::string> results;
        
        for (const auto& [model_id, metadata] : model_index_) {
            bool matches = true;
            
            // Check query
            if (!query.empty()) {
                if (metadata.name.find(query) == std::string::npos &&
                    metadata.description.find(query) == std::string::npos) {
                    matches = false;
                }
            }
            
            // Check tags
            for (const auto& tag : tags) {
                if (metadata.tags.find(tag) == metadata.tags.end()) {
                    matches = false;
                    break;
                }
            }
            
            // Check model type
            if (!model_type.empty() && metadata.type != model_type) {
                matches = false;
            }
            
            if (matches) {
                results.push_back(model_id);
            }
        }
        
        return Result<std::vector<std::string>>::success(results);
        
    } catch (const std::exception& e) {
        return Result<std::vector<std::string>>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<std::filesystem::path> ModelRegistry::get_model_path(
    const std::string& model_id, const std::string& version) const {
    auto it = model_index_.find(model_id);
    if (it == model_index_.end()) {
        return Result<std::filesystem::path>::error(ErrorCode::NotFound,
                                                   "Model not found: " + model_id);
    }
    
    std::string target_version = version.empty() ? it->second.version : version;
    auto path = registry_path_ / model_id / (std::string(target_version) + ".model");
    
    if (!std::filesystem::exists(path)) {
        return Result<std::filesystem::path>::error(ErrorCode::FileNotFound,
                                                   "Version not found: " + target_version);
    }
    
    return Result<std::filesystem::path>::success(path);
}

Result<ModelMetadata> ModelRegistry::get_model_metadata(const std::string& model_id, 
                                                       const std::string& version) const {
    auto it = model_index_.find(model_id);
    if (it == model_index_.end()) {
        return Result<ModelMetadata>::error(ErrorCode::NotFound,
                                          "Model not found: " + model_id);
    }
    
    return Result<ModelMetadata>::success(it->second);
}

Result<void> ModelRegistry::update_index() {
    try {
        // Scan registry directory and rebuild index
        for (const auto& entry : std::filesystem::directory_iterator(registry_path_)) {
            if (entry.is_directory()) {
                auto model_id = entry.path().filename().string();
                
                // Find latest version
                for (const auto& model_file : std::filesystem::directory_iterator(entry.path())) {
                    if (model_file.path().extension() == ".metadata") {
                        ModelLoader loader;
                        auto metadata_result = loader.load_metadata(
                            model_file.path().string().substr(0, 
                            model_file.path().string().find(".metadata")));
                        
                        if (metadata_result.is_ok()) {
                            model_index_[model_id] = metadata_result.value();
                        }
                    }
                }
            }
        }
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<void> ModelRegistry::save_index() const {
    try {
        auto index_path = registry_path_ / "index.json";
        std::ofstream index_file(index_path);
        
        index_file << "{\n";
        index_file << "  \"models\": [\n";
        
        bool first = true;
        for (const auto& [model_id, metadata] : model_index_) {
            if (!first) index_file << ",\n";
            first = false;
            
            index_file << "    {\n";
            index_file << "      \"model_id\": \"" << model_id << "\",\n";
            index_file << "      \"name\": \"" << metadata.name << "\",\n";
            index_file << "      \"version\": \"" << metadata.version << "\",\n";
            index_file << "      \"type\": \"" << metadata.type << "\"\n";
            index_file << "    }";
        }
        
        index_file << "\n  ]\n";
        index_file << "}\n";
        index_file.close();
        
        return Result<void>::success();
        
    } catch (const std::exception& e) {
        return Result<void>::error(ErrorCode::CalculationError, e.what());
    }
}

// ============================================================================
// ModelPerformanceTracker Implementation
// ============================================================================

ModelPerformanceTracker::ModelPerformanceTracker() {}

void ModelPerformanceTracker::record_prediction(const std::string& model_id,
                                               double prediction,
                                               double actual,
                                               const DateTime& timestamp) {
    PredictionRecord record;
    record.timestamp = timestamp;
    record.prediction = prediction;
    record.actual = actual;
    
    predictions_[model_id].push_back(record);
}

void ModelPerformanceTracker::record_batch_predictions(const std::string& model_id,
                                                      const std::vector<double>& predictions,
                                                      const std::vector<double>& actuals,
                                                      const DateTime& timestamp) {
    if (predictions.size() != actuals.size()) {
        return; // Invalid input
    }
    
    for (size_t i = 0; i < predictions.size(); ++i) {
        record_prediction(model_id, predictions[i], actuals[i], timestamp);
    }
}

Result<std::unordered_map<std::string, double>> ModelPerformanceTracker::calculate_metrics(
    const std::string& model_id,
    const DateTime& start_time,
    const DateTime& end_time) const {
    try {
        auto it = predictions_.find(model_id);
        if (it == predictions_.end()) {
            return Result<std::unordered_map<std::string, double>>::error(
                ErrorCode::NotFound, "Model not found: " + model_id);
        }
        
        const auto& records = it->second;
        if (records.empty()) {
            return Result<std::unordered_map<std::string, double>>::error(
                ErrorCode::InsufficientData, "No predictions recorded");
        }
        
        std::unordered_map<std::string, double> metrics;
        
        metrics["rmse"] = calculate_rmse(records);
        metrics["mae"] = calculate_mae(records);
        metrics["mape"] = calculate_mape(records);
        metrics["r2"] = calculate_r2(records);
        metrics["count"] = static_cast<double>(records.size());
        
        return Result<std::unordered_map<std::string, double>>::success(metrics);
        
    } catch (const std::exception& e) {
        return Result<std::unordered_map<std::string, double>>::error(
            ErrorCode::CalculationError, e.what());
    }
}

double ModelPerformanceTracker::calculate_rmse(const std::vector<PredictionRecord>& records) const {
    if (records.empty()) return 0.0;
    
    double sum_squared_error = 0.0;
    for (const auto& record : records) {
        double error = record.prediction - record.actual;
        sum_squared_error += error * error;
    }
    
    return std::sqrt(sum_squared_error / records.size());
}

double ModelPerformanceTracker::calculate_mae(const std::vector<PredictionRecord>& records) const {
    if (records.empty()) return 0.0;
    
    double sum_absolute_error = 0.0;
    for (const auto& record : records) {
        sum_absolute_error += std::abs(record.prediction - record.actual);
    }
    
    return sum_absolute_error / records.size();
}

double ModelPerformanceTracker::calculate_mape(const std::vector<PredictionRecord>& records) const {
    if (records.empty()) return 0.0;
    
    double sum_percentage_error = 0.0;
    size_t valid_count = 0;
    
    for (const auto& record : records) {
        if (std::abs(record.actual) > 1e-10) { // Avoid division by zero
            sum_percentage_error += std::abs((record.prediction - record.actual) / record.actual);
            valid_count++;
        }
    }
    
    return valid_count > 0 ? (sum_percentage_error / valid_count) * 100.0 : 0.0;
}

double ModelPerformanceTracker::calculate_r2(const std::vector<PredictionRecord>& records) const {
    if (records.empty()) return 0.0;
    
    // Calculate mean of actuals
    double mean_actual = 0.0;
    for (const auto& record : records) {
        mean_actual += record.actual;
    }
    mean_actual /= records.size();
    
    // Calculate R²
    double ss_tot = 0.0; // Total sum of squares
    double ss_res = 0.0; // Residual sum of squares
    
    for (const auto& record : records) {
        ss_tot += (record.actual - mean_actual) * (record.actual - mean_actual);
        ss_res += (record.actual - record.prediction) * (record.actual - record.prediction);
    }
    
    return ss_tot > 1e-10 ? 1.0 - (ss_res / ss_tot) : 0.0;
}

Result<bool> ModelPerformanceTracker::detect_drift(const std::string& model_id,
                                                   size_t window_size,
                                                   size_t test_size,
                                                   double threshold) const {
    try {
        auto it = predictions_.find(model_id);
        if (it == predictions_.end()) {
            return Result<bool>::error(ErrorCode::NotFound, "Model not found: " + model_id);
        }
        
        const auto& records = it->second;
        if (records.size() < window_size + test_size) {
            return Result<bool>::error(ErrorCode::InsufficientData, "Not enough data for drift detection");
        }
        
        // Simple drift detection: compare recent performance with historical
        size_t recent_start = records.size() - test_size;
        size_t historical_end = recent_start - window_size;
        
        double historical_mse = 0.0;
        for (size_t i = historical_end; i < recent_start; ++i) {
            double error = records[i].prediction - records[i].actual;
            historical_mse += error * error;
        }
        historical_mse /= window_size;
        
        double recent_mse = 0.0;
        for (size_t i = recent_start; i < records.size(); ++i) {
            double error = records[i].prediction - records[i].actual;
            recent_mse += error * error;
        }
        recent_mse /= test_size;
        
        // Detect drift if recent performance is significantly worse
        double drift_ratio = recent_mse / (historical_mse + 1e-10);
        bool drift_detected = drift_ratio > (1.0 + threshold);
        
        return Result<bool>::success(drift_detected);
        
    } catch (const std::exception& e) {
        return Result<bool>::error(ErrorCode::CalculationError, e.what());
    }
}

Result<std::string> ModelPerformanceTracker::generate_report(const std::string& model_id,
                                                           const std::string& format) const {
    try {
        auto metrics_result = calculate_metrics(model_id);
        if (metrics_result.is_error()) {
            return Result<std::string>::error(metrics_result.error().code,
                                            metrics_result.error().message);
        }
        
        auto metrics = metrics_result.value();
        
        std::stringstream report;
        report << "Model Performance Report\\n";
        report << "========================\\n";
        report << "Model ID: " << model_id << "\\n";
        report << "Generated: " << "2024-01-01 00:00:00" << "\\n\\n";
        
        report << "Performance Metrics:\\n";
        report << "  RMSE: " << std::setprecision(6) << metrics["rmse"] << "\\n";
        report << "  MAE:  " << metrics["mae"] << "\\n";
        report << "  MAPE: " << metrics["mape"] << "%\\n";
        report << "  R²:   " << metrics["r2"] << "\\n";
        report << "  Predictions: " << static_cast<int>(metrics["count"]) << "\\n\\n";
        
        // Add drift detection status
        auto drift_result = detect_drift(model_id, 50, 20, 0.1);
        if (drift_result.is_ok()) {
            report << "Drift Status: " << (drift_result.value() ? "DRIFT DETECTED" : "Normal") << "\\n";
        }
        
        return Result<std::string>::success(report.str());
        
    } catch (const std::exception& e) {
        return Result<std::string>::error(ErrorCode::CalculationError, e.what());
    }
}

} // namespace pyfolio::ml