# Deferred Columns: Technical Documentation

**Authors:** Thomas Albert Iwin & Shakunth

## Overview
The `deferred_columns` DuckDB extension enables native, in-database machine learning imputation for schema evolution. It provides a foundational architecture for lazy column evaluation, model versioning (MVCC-M), and Approximate Query Processing (AQP) error bounds.

## Build Instructions
This project requires a standard C++ compiler and CMake.
```bash
# Clean previous builds
make clean

# Compile the DuckDB engine and our custom extension
make release

# Run the automated SQL logic tests
make test
```

## Architectural Components

### 1. The Model Registry (`src/model_registry.cpp`)
Acts as the central catalog for all loaded ML models. It maps a model string name to a list of `ModelMetadata` structs containing `version`, `rmse`, and `is_active` flags. This registry is thread-safe and is queried during the bind phase of a SQL statement to guarantee deterministic model execution.

### 2. The Table Function (`src/deferred_scan.cpp`)
Implements `deferred_scan('model_name')`.
- **Bind Phase:** Looks up the model in the `ModelRegistry` and pins the exact active version to the query state.
- **Init Phase:** Initializes the ONNX `Ort::Env` and `Ort::Session` (mocked for this prototype).
- **Scan Phase:** Evaluates the ML model on the fly, producing `42.0f` for every requested row in vectorized DuckDB `DataChunk`s.

### 3. The Optimizer Extension (`src/deferred_optimizer.cpp`)
Implements `DeferredPreOptimize`. This hook intercepts the DuckDB logical query plan *before* it is converted into a physical execution plan. In a production environment, this hook analyzes the logical tree and injects the $C_{inf}$ (Cost of Inference) metric, forcing DuckDB to push downstream filters *above* the ML inference node to prune rows before evaluating the expensive models.

### 4. AQP Aggregates (`src/deferred_aggregates.cpp`)
Implements the `sum_ci(imputed_val, rmse)` aggregate function.
Instead of returning a simple `double`, this function returns a `VARCHAR` string containing the 95% Confidence Interval based on the model's known Root Mean Square Error (RMSE). It computes the bounds natively within DuckDB's vectorized aggregation framework without scalar overhead.

## Usage Guide
To test the extension interactively:

1. Launch DuckDB:
```bash
./build/release/duckdb
```

2. Load the extension:
```sql
LOAD deferred_columns;
```

3. Query the virtual deferred column:
```sql
SELECT * FROM deferred_scan('mock_model');
```

4. Query with AQP Error Bounds:
```sql
SELECT sum_ci(imputed_value, 0.5) FROM deferred_scan('mock_model');
```
*(Returns `210.000000 +/- 2.191347`)*
