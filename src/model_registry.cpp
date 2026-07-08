#include "model_registry.hpp"
#include "duckdb/common/exception.hpp"

namespace duckdb {

ModelRegistry& ModelRegistry::GetInstance() {
    static ModelRegistry instance;
    return instance;
}

void ModelRegistry::RegisterModel(const std::string& name, int version, double rmse, bool is_active) {
    std::lock_guard<std::mutex> guard(lock_);
    models_[name].push_back({name, version, rmse, is_active});
}

ModelMetadata ModelRegistry::GetModel(const std::string& name, int version) {
    std::lock_guard<std::mutex> guard(lock_);
    auto it = models_.find(name);
    if (it == models_.end() || it->second.empty()) {
        throw InvalidInputException("Model not found in registry: " + name);
    }
    
    if (version == -1) {
        // Return latest active
        for (auto rit = it->second.rbegin(); rit != it->second.rend(); ++rit) {
            if (rit->is_active) {
                return *rit;
            }
        }
        throw InvalidInputException("No active versions found for model: " + name);
    } else {
        for (const auto& model : it->second) {
            if (model.version == version) {
                return model;
            }
        }
        throw InvalidInputException("Specific version not found for model: " + name);
    }
}

} // namespace duckdb
