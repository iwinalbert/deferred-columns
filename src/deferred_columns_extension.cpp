#define DUCKDB_EXTENSION_MAIN

#include "deferred_columns_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/optimizer/optimizer_extension.hpp"
#include "duckdb/main/config.hpp"
#include "duckdb/main/extension/extension_loader.hpp"
#include "model_registry.hpp"
#include "deferred_aggregates.hpp"

namespace duckdb {

extern void DeferredScanFunction(ClientContext &context, TableFunctionInput &data, DataChunk &output);
extern unique_ptr<FunctionData> DeferredScanBind(ClientContext &context, TableFunctionBindInput &input,
                                                 vector<LogicalType> &return_types, vector<string> &names);
extern unique_ptr<GlobalTableFunctionState> DeferredScanInit(ClientContext &context, TableFunctionInitInput &input);
extern void DeferredPreOptimize(OptimizerExtensionInput &input, unique_ptr<LogicalOperator> &plan);

static void LoadInternal(ExtensionLoader &loader) {
	// 1. MVCC-M: Register a mock model into the ModelRegistry
	ModelRegistry::GetInstance().RegisterModel("mock_model", 1, 0.5); // Version 1, RMSE 0.5

	// 2. Register TableFunction
	TableFunction deferred_scan("deferred_scan", {LogicalType::VARCHAR}, DeferredScanFunction, DeferredScanBind,
	                            DeferredScanInit);
	loader.RegisterFunction(deferred_scan);

	// 3. Register AQP Aggregates
	loader.RegisterFunction(GetSumCIAggregate());

	// 4. Register Optimizer Extension
	OptimizerExtension deferred_opt;
	deferred_opt.pre_optimize_function = DeferredPreOptimize;

	auto &config = DBConfig::GetConfig(loader.GetDatabaseInstance());
	OptimizerExtension::Register(config, deferred_opt);
}

void DeferredColumnsExtension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}
std::string DeferredColumnsExtension::Name() {
	return "deferred_columns";
}

std::string DeferredColumnsExtension::Version() const {
#ifdef EXT_VERSION_DEFERRED_COLUMNS
	return EXT_VERSION_DEFERRED_COLUMNS;
#else
	return "";
#endif
}

} // namespace duckdb

extern "C" {
DUCKDB_CPP_EXTENSION_ENTRY(deferred_columns, loader) {
	duckdb::LoadInternal(loader);
}
}
