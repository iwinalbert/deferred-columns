#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace duckdb {

struct ModelMetadata {
	std::string name;
	int version;
	double rmse;
	bool is_active;
};

class ModelRegistry {
public:
	static ModelRegistry &GetInstance();

	void RegisterModel(const std::string &name, int version, double rmse, bool is_active = true);
	ModelMetadata GetModel(const std::string &name, int version = -1); // -1 means latest active

private:
	ModelRegistry() = default;
	std::mutex lock_;
	std::unordered_map<std::string, std::vector<ModelMetadata>> models_;
};

} // namespace duckdb
