#include "deferred_aggregates.hpp"
#include "duckdb/common/vector_operations/vector_operations.hpp"
#include "duckdb/common/vector.hpp"
#include <cmath>

namespace duckdb {

struct SumCIState {
	double sum;
	double sum_rmse_sq;
	idx_t count;
};

struct SumCIFunction {
	template <class STATE>
	static void Initialize(STATE &state) {
		state.sum = 0;
		state.sum_rmse_sq = 0;
		state.count = 0;
	}

	template <class STATE, class OP>
	static void Combine(const STATE &source, STATE &target, AggregateInputData &) {
		target.sum += source.sum;
		target.count += source.count;
		target.sum_rmse_sq += source.sum_rmse_sq;
	}

	template <class INPUT_TYPE, class STATE, class OP>
	static void Operation(STATE &state, const INPUT_TYPE &input, AggregateUnaryInput &unary_input) {
		// Not used for binary inputs
	}

	template <class RESULT_TYPE, class STATE>
	static void Finalize(STATE &state, RESULT_TYPE &target, AggregateFinalizeData &finalize_data) {
		if (state.count == 0) {
			finalize_data.ReturnNull();
			return;
		}
		double ci = 1.96 * std::sqrt(state.sum_rmse_sq);
		std::string res = std::to_string(state.sum) + " +/- " + std::to_string(ci);
		target = StringVector::AddString(finalize_data.result, res.c_str(), res.size());
	}

	static bool IgnoreNull() {
		return true;
	}
};

static void SumCIUpdate(Vector inputs[], AggregateInputData &aggr_input_data, idx_t input_count, Vector &state_vector,
                        idx_t count) {
	Vector &val_vec = inputs[0];
	Vector &rmse_vec = inputs[1];

	UnifiedVectorFormat val_vdata, rmse_vdata;
	val_vec.ToUnifiedFormat(count, val_vdata);
	rmse_vec.ToUnifiedFormat(count, rmse_vdata);

	auto states = FlatVector::GetData<SumCIState *>(state_vector);
	auto val_data = UnifiedVectorFormat::GetData<double>(val_vdata);
	auto rmse_data = UnifiedVectorFormat::GetData<double>(rmse_vdata);

	for (idx_t i = 0; i < count; i++) {
		auto idx1 = val_vdata.sel->get_index(i);
		auto idx2 = rmse_vdata.sel->get_index(i);
		if (!val_vdata.validity.RowIsValid(idx1) || !rmse_vdata.validity.RowIsValid(idx2)) {
			continue;
		}
		auto state = states[i];
		state->sum += val_data[idx1];
		state->sum_rmse_sq += (rmse_data[idx2] * rmse_data[idx2]);
		state->count++;
	}
}

AggregateFunction GetSumCIAggregate() {
	auto fun = AggregateFunction(
	    "sum_ci", {LogicalType::DOUBLE, LogicalType::DOUBLE}, LogicalType::VARCHAR,
	    AggregateFunction::StateSize<SumCIState>, AggregateFunction::StateInitialize<SumCIState, SumCIFunction>,
	    SumCIUpdate, AggregateFunction::StateCombine<SumCIState, SumCIFunction>,
	    AggregateFunction::StateFinalize<SumCIState, string_t, SumCIFunction>, nullptr, nullptr, nullptr);
	return fun;
}

} // namespace duckdb
