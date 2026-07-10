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
extern unique_ptr<LocalTableFunctionState> DeferredScanInitLocal(ExecutionContext &context, TableFunctionInitInput &input, GlobalTableFunctionState *global_state);
extern void DeferredPreOptimize(OptimizerExtensionInput &input, unique_ptr<LogicalOperator> &plan);

static void SwapModelFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &version_vector = args.data[0];
    auto version_data = FlatVector::GetData<int32_t>(version_vector);
    
    std::string file_path = "model.onnx";
    if (args.ColumnCount() > 1) {
        auto &path_vector = args.data[1];
        auto path_data = FlatVector::GetData<string_t>(path_vector);
        file_path = path_data[0].GetString();
    }
    
    // Swap model to new version
    ModelRegistry::GetInstance().RegisterModel("mock_model", version_data[0], 0.5, file_path);
    
    auto result_data = FlatVector::GetData<bool>(result);
    result_data[0] = true;
    result.SetVectorType(VectorType::CONSTANT_VECTOR);
}

static void LoadInternal(ExtensionLoader &loader) {
	// 1. MVCC-M: Register a real model into the ModelRegistry
	ModelRegistry::GetInstance().RegisterModel("mock_model", 1, 0.5, "model.onnx"); // Version 1, RMSE 0.5

	// 2. Register TableFunction overloads (1-arg and 2-arg)
	TableFunctionSet deferred_scan("deferred_scan");
	
	TableFunction tf_1({LogicalType::VARCHAR}, DeferredScanFunction, DeferredScanBind, DeferredScanInit, DeferredScanInitLocal);
	tf_1.filter_pushdown = true;
	tf_1.filter_prune = true;
	deferred_scan.AddFunction(tf_1);
	
	TableFunction tf_2({LogicalType::VARCHAR, LogicalType::BIGINT}, DeferredScanFunction, DeferredScanBind, DeferredScanInit, DeferredScanInitLocal);
	tf_2.filter_pushdown = true;
	tf_2.filter_prune = true;
	deferred_scan.AddFunction(tf_2);
	
	loader.RegisterFunction(deferred_scan);

	// 3. Register AQP Aggregates
	loader.RegisterFunction(GetSumCIAggregate());

    // 4. Register Swap Model function
    ScalarFunctionSet swap_model_set("swap_model");
    swap_model_set.AddFunction(ScalarFunction({LogicalType::INTEGER}, LogicalType::BOOLEAN, SwapModelFunction));
    swap_model_set.AddFunction(ScalarFunction({LogicalType::INTEGER, LogicalType::VARCHAR}, LogicalType::BOOLEAN, SwapModelFunction));
    loader.RegisterFunction(swap_model_set);

	// 5. Register Optimizer Extension
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
