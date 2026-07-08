#include "duckdb/function/table_function.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/common/types/value.hpp"
#include <onnxruntime_cxx_api.h>
#include "model_registry.hpp"

namespace duckdb {

struct DeferredScanBindData : public TableFunctionData {
	string model_name;
	int version;
};

struct DeferredScanGlobalState : public GlobalTableFunctionState {
	bool finished = false;
	unique_ptr<Ort::Env> env;
	unique_ptr<Ort::Session> session;
};

unique_ptr<FunctionData> DeferredScanBind(ClientContext &context, TableFunctionBindInput &input,
                                          vector<LogicalType> &return_types, vector<string> &names) {
	auto result = make_uniq<DeferredScanBindData>();
	result->model_name = StringValue::Get(input.inputs[0]);

	auto model_meta = ModelRegistry::GetInstance().GetModel(result->model_name);
	result->version = model_meta.version;

	return_types.push_back(LogicalType::FLOAT);
	names.push_back("imputed_value");
	return std::move(result);
}

unique_ptr<GlobalTableFunctionState> DeferredScanInit(ClientContext &context, TableFunctionInitInput &input) {
	auto state = make_uniq<DeferredScanGlobalState>();
	state->env = make_uniq<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "DeferredColumns");
	return std::move(state);
}

void DeferredScanFunction(ClientContext &context, TableFunctionInput &data, DataChunk &output) {
	auto &state = data.global_state->Cast<DeferredScanGlobalState>();

	if (state.finished) {
		output.SetCardinality(0);
		return;
	}

	idx_t mock_row_count = 5;
	output.SetCardinality(mock_row_count);

	auto result_data = FlatVector::GetData<float>(output.data[0]);
	for (idx_t i = 0; i < mock_row_count; i++) {
		if (state.env) {
			result_data[i] = 42.0f;
		} else {
			result_data[i] = 0.0f;
		}
	}

	state.finished = true;
}

} // namespace duckdb
