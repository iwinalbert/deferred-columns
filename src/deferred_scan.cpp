#include "duckdb/function/table_function.hpp"
#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/common/vector.hpp"
#include "duckdb/common/types/value.hpp"
#include <onnxruntime_cxx_api.h>
#include "model_registry.hpp"
#include <random>
#include "duckdb/common/types/selection_vector.hpp"
#include "duckdb/planner/table_filter.hpp"
#include "duckdb/planner/filter/constant_filter.hpp"
#include <mutex>
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/query_result.hpp"
#include "duckdb/main/connection.hpp"
#include "duckdb/main/database.hpp"

#include <chrono>
#include <atomic>

static std::atomic<long long> total_onnx_time{0};

namespace duckdb {

constexpr idx_t INFERENCE_BATCH_SIZE = 2048;

struct DeferredScanBindData : public TableFunctionData {
	string model_name;
	int version;
    idx_t total_rows;
};

struct DeferredScanGlobalState : public GlobalTableFunctionState {
    ~DeferredScanGlobalState() {
        printf("Total ONNX Time: %lld us\n", total_onnx_time.exchange(0));
    }
    idx_t MaxThreads() const override {
        return GlobalTableFunctionState::MAX_THREADS; // Allow DuckDB to use all available worker threads
    }
    std::mutex fetch_lock;
	idx_t rows_produced = 0;
	unique_ptr<Ort::Env> env;
	unique_ptr<Ort::Session> session;
    string model_path;
    int version;
    string model_name;
    unique_ptr<QueryResult> query_result;
    unique_ptr<TableFilterSet> filters;
};

unique_ptr<FunctionData> DeferredScanBind(ClientContext &context, TableFunctionBindInput &input,
                                          vector<LogicalType> &return_types, vector<string> &names) {
	auto result = make_uniq<DeferredScanBindData>();
	result->model_name = StringValue::Get(input.inputs[0]);
	result->total_rows = input.inputs.size() > 1 ? BigIntValue::Get(input.inputs[1]) : 5;

	auto model_meta = ModelRegistry::GetInstance().GetModel(result->model_name);
	result->version = model_meta.version;

    return_types.push_back(LogicalType::VARCHAR); names.push_back("category");
    return_types.push_back(LogicalType::VARCHAR); names.push_back("date");
    return_types.push_back(LogicalType::FLOAT);   names.push_back("f1");
    return_types.push_back(LogicalType::FLOAT);   names.push_back("f2");
    return_types.push_back(LogicalType::FLOAT);   names.push_back("f3");
    return_types.push_back(LogicalType::FLOAT);   names.push_back("rmse");
	return_types.push_back(LogicalType::FLOAT);   names.push_back("imputed_value");
    
	return std::move(result);
}

unique_ptr<GlobalTableFunctionState> DeferredScanInit(ClientContext &context, TableFunctionInitInput &input) {
	auto state = make_uniq<DeferredScanGlobalState>();
	state->env = make_uniq<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "DeferredColumns");
    
    auto bind_data = input.bind_data->Cast<DeferredScanBindData>();
    auto model_meta = ModelRegistry::GetInstance().GetModel(bind_data.model_name, bind_data.version);
    
    state->model_path = model_meta.file_path;
    state->version = bind_data.version;
    state->model_name = bind_data.model_name;
    
    // Copy filters if present
    if (input.filters) {
        state->filters = make_uniq<TableFilterSet>();
        for (auto &pair : input.filters->filters) {
            idx_t col_idx = pair.first;
            idx_t absolute_col_idx = input.column_ids[col_idx];
            state->filters->filters[absolute_col_idx] = pair.second->Copy();
        }
    }
    
    // Try to query the real table if it exists (using a separate Connection to avoid deadlocks)
    string sql = "SELECT category, date, f1, f2, f3, rmse FROM stress_data";
    if (input.filters) {
        string where_clause;
        bool first = true;
        for (auto &pair : input.filters->filters) {
            idx_t col_idx = pair.first;
            idx_t absolute_col_idx = input.column_ids[col_idx];
            if (absolute_col_idx == 6) {
                continue;
            }
            auto &filter = *pair.second;
            string col_name;
            if (absolute_col_idx == 0) col_name = "category";
            else if (absolute_col_idx == 1) col_name = "date";
            else if (absolute_col_idx == 2) col_name = "f1";
            else if (absolute_col_idx == 3) col_name = "f2";
            else if (absolute_col_idx == 4) col_name = "f3";
            else if (absolute_col_idx == 5) col_name = "rmse";
            
            if (!first) {
                where_clause += " AND ";
            }
            first = false;
            where_clause += filter.ToString(col_name);
        }
        if (!where_clause.empty()) {
            sql += " WHERE " + where_clause;
        }
    }
    
    try {
        if (context.db) {
            Connection temp_con(*(context.db));
            auto res = temp_con.Query(sql);
            if (res && !res->HasError()) {
                
                state->query_result = std::move(res);
            }
        }
        } catch (const Ort::Exception& e) {
            throw duckdb::InvalidInputException(std::string("ORT Load Exception: ") + e.what());
        } catch (...) {
        // Fallback to synthetic
    }
    
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(1);
    session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    
    if (!state->model_path.empty()) {
        try {
            state->session = make_uniq<Ort::Session>(*(state->env), state->model_path.c_str(), session_options);
        } catch (const Ort::Exception& e) {
            throw duckdb::InvalidInputException(std::string("ORT Load Exception: ") + e.what());
        } catch (...) {
            // If file doesn't exist, session remains null
        }
    }
    if (state->session == nullptr) {
        printf("ONNX Total Time so far: %lld us\n", total_onnx_time.load());
    }
    
	return std::move(state);
}

struct DeferredScanLocalState : public LocalTableFunctionState {
    bool done = false;
};

unique_ptr<LocalTableFunctionState> DeferredScanInitLocal(ExecutionContext &context, TableFunctionInitInput &input, GlobalTableFunctionState *global_state) {
    return make_uniq<DeferredScanLocalState>();
}

static bool RowMatchesFilters(TableFilterSet &filter_set, DataChunk &chunk, idx_t row_idx) {
    for (auto &pair : filter_set.filters) {
        idx_t col_idx = pair.first;
        auto &filter = *pair.second;
        
        Value val = chunk.GetValue(col_idx, row_idx);
        
        if (filter.filter_type == TableFilterType::CONSTANT_COMPARISON) {
            auto &cf = filter.Cast<ConstantFilter>();
            bool match = false;
            switch (cf.comparison_type) {
                case ExpressionType::COMPARE_EQUAL:
                    match = (val == cf.constant);
                    break;
                case ExpressionType::COMPARE_NOTEQUAL:
                    match = (val != cf.constant);
                    break;
                case ExpressionType::COMPARE_LESSTHAN:
                    match = (val < cf.constant);
                    break;
                case ExpressionType::COMPARE_GREATERTHAN:
                    match = (val > cf.constant);
                    break;
                case ExpressionType::COMPARE_LESSTHANOREQUALTO:
                    match = (val <= cf.constant);
                    break;
                case ExpressionType::COMPARE_GREATERTHANOREQUALTO:
                    match = (val >= cf.constant);
                    break;
                default:
                    match = true;
                    break;
            }
            if (!match) return false;
        } else if (filter.filter_type == TableFilterType::IS_NULL) {
            if (!val.IsNull()) return false;
        } else if (filter.filter_type == TableFilterType::IS_NOT_NULL) {
            if (val.IsNull()) return false;
        }
    }
    return true;
}

void DeferredScanFunction(ClientContext &context, TableFunctionInput &data, DataChunk &output) {
	auto &state = data.global_state->Cast<DeferredScanGlobalState>();
    auto &bind_data = data.bind_data->Cast<DeferredScanBindData>();

    DataChunk temp_chunk;
    vector<LogicalType> types;
    for (idx_t i = 0; i < 7; i++) {
        if (i == 0 || i == 1) types.push_back(LogicalType::VARCHAR);
        else if (i == 6) types.push_back(LogicalType::FLOAT);
        else types.push_back(LogicalType::FLOAT);
    }
    temp_chunk.Initialize(context, types);

    idx_t fetched_count = 0;
    unique_ptr<DataChunk> db_chunk;

    {
        std::unique_lock<std::mutex> guard(state.fetch_lock);

        if (state.rows_produced >= bind_data.total_rows) {
            output.SetCardinality(0);
            return;
        }

        if (state.query_result) {
            db_chunk = state.query_result->Fetch();
            if (!db_chunk || db_chunk->size() == 0) {
                output.SetCardinality(0);
                return;
            }
            fetched_count = db_chunk->size();
            if (fetched_count > INFERENCE_BATCH_SIZE) {
                fetched_count = INFERENCE_BATCH_SIZE; // Cap to batch size
            }
            for (idx_t col_idx = 0; col_idx < 6; col_idx++) {
                temp_chunk.data[col_idx].Reference(db_chunk->data[col_idx]);
            }
            temp_chunk.SetCardinality(fetched_count);
        } else {
            fetched_count = std::min(INFERENCE_BATCH_SIZE, bind_data.total_rows - state.rows_produced);
            state.rows_produced += fetched_count;
            temp_chunk.SetCardinality(fetched_count);
        }
    } // End fetch_lock
    
    if (!db_chunk) {
        auto cat_data = FlatVector::GetData<string_t>(temp_chunk.data[0]);
        auto date_data = FlatVector::GetData<string_t>(temp_chunk.data[1]);
        auto f1_data = FlatVector::GetData<float>(temp_chunk.data[2]);
        auto f2_data = FlatVector::GetData<float>(temp_chunk.data[3]);
        auto f3_data = FlatVector::GetData<float>(temp_chunk.data[4]);
        auto rmse_data = FlatVector::GetData<float>(temp_chunk.data[5]);
        
        static const std::vector<std::string> categories = []() {
            std::vector<std::string> cats;
            for (int i = 0; i < 26; i++) {
                cats.push_back(std::string(1, 'A' + i));
            }
            return cats;
        }();
        
        static const std::vector<std::string> dates = []() {
            std::vector<std::string> ds;
            for (int day = 1; day <= 100; day++) {
                if (day > 5) {
                    ds.push_back("2023-10-" + (day > 31 ? std::to_string(day % 28 + 1) : std::to_string(day)));
                } else {
                    ds.push_back("2023-11-0" + std::to_string(day));
                }
            }
            return ds;
        }();
        
        static thread_local uint32_t rx = 123456789, ry = 362436069, rz = 521288629, rw = 88675123;
        auto fast_rand = [&]() {
            uint32_t t = rx ^ (rx << 11);
            rx = ry; ry = rz; rz = rw;
            return rw = rw ^ (rw >> 19) ^ (t ^ (t >> 8));
        };
        
        for (idx_t i = 0; i < fetched_count; i++) {
            int cat_idx = fast_rand() % 26;
            cat_data[i] = StringVector::AddString(temp_chunk.data[0], categories[cat_idx]);
            
            int day_idx = fast_rand() % 100;
            date_data[i] = StringVector::AddString(temp_chunk.data[1], dates[day_idx]);
            
            bool is_null = (fast_rand() % 100) > 90;
            float f1 = is_null ? 0.0f : (float)(fast_rand() % 100);
            float f2 = is_null ? 0.0f : (float)(fast_rand() % 100);
            float f3 = is_null ? 0.0f : (float)(fast_rand() % 100);
            
            f1_data[i] = f1;
            f2_data[i] = f2;
            f3_data[i] = f3;
            rmse_data[i] = 0.5f;
        }
    }
    
    // MVCC-M: Acquire read lock
    SharedLockGuard<SharedMutex> model_read_lock(ModelRegistry::GetInstance().GetMutex());


    TableFilterSet non_imputed_filters;
    bool has_non_imputed_filters = false;
    if (state.filters) {
        for (auto &pair : state.filters->filters) {
            if (pair.first != 6) {
                non_imputed_filters.filters[pair.first] = pair.second->Copy();
                has_non_imputed_filters = true;
            }
        }
    }

    std::vector<float> input_tensor_values;
    std::vector<idx_t> valid_indices;
    
    UnifiedVectorFormat f1_vdata, f2_vdata, f3_vdata;
    temp_chunk.data[2].ToUnifiedFormat(fetched_count, f1_vdata);
    temp_chunk.data[3].ToUnifiedFormat(fetched_count, f2_vdata);
    temp_chunk.data[4].ToUnifiedFormat(fetched_count, f3_vdata);
    auto f1_udata = UnifiedVectorFormat::GetData<float>(f1_vdata);
    auto f2_udata = UnifiedVectorFormat::GetData<float>(f2_vdata);
    auto f3_udata = UnifiedVectorFormat::GetData<float>(f3_vdata);
    
    auto result_data = FlatVector::GetData<float>(temp_chunk.data[6]);
    
    for (idx_t i = 0; i < fetched_count; i++) {
        result_data[i] = 42.0f;
        
        bool matches = true;
        if (has_non_imputed_filters) {
            matches = RowMatchesFilters(non_imputed_filters, temp_chunk, i);
        }
        
        if (matches) {
            auto idx1 = f1_vdata.sel->get_index(i);
            float val1 = f1_vdata.validity.RowIsValid(idx1) ? f1_udata[idx1] : 0.0f;
            auto idx2 = f2_vdata.sel->get_index(i);
            float val2 = f2_vdata.validity.RowIsValid(idx2) ? f2_udata[idx2] : 0.0f;
            auto idx3 = f3_vdata.sel->get_index(i);
            float val3 = f3_vdata.validity.RowIsValid(idx3) ? f3_udata[idx3] : 0.0f;

            input_tensor_values.push_back(val1);
            input_tensor_values.push_back(val2);
            input_tensor_values.push_back(val3);
            valid_indices.push_back(i);
        }
    }
    
    if (state.session && !valid_indices.empty()) {
        Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::vector<int64_t> input_shape = { (int64_t)valid_indices.size(), 3 };
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info, input_tensor_values.data(), input_tensor_values.size(), input_shape.data(), input_shape.size());

        const char* input_names[] = {"float_input"};
        const char* output_names[] = {"variable"};

        auto start = std::chrono::high_resolution_clock::now();
        auto output_tensors = state.session->Run(Ort::RunOptions{nullptr}, input_names, &input_tensor, 1, output_names, 1);
        auto end = std::chrono::high_resolution_clock::now();
        total_onnx_time += std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        float* floatarr = output_tensors.front().GetTensorMutableData<float>();
        for (size_t k = 0; k < valid_indices.size(); k++) {
            result_data[valid_indices[k]] = floatarr[k];
        }
    }
    
    idx_t match_count = 0;
    SelectionVector sel(fetched_count);
    
    for (idx_t i = 0; i < fetched_count; i++) {
        bool matches = true;
        if (state.filters) {
            matches = RowMatchesFilters(*(state.filters), temp_chunk, i);
        }
        if (matches) {
            sel.set_index(match_count++, i);
        }
    }
    
    if (match_count == 0) {
        output.SetCardinality(0);
    } else {
        output.Slice(temp_chunk, sel, match_count);
    }
    
    }

} // namespace duckdb

