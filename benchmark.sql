-- Load the custom extension
LOAD 'build/release/extension/deferred_columns/deferred_columns.duckdb_extension';

-- 1. Setup Phase: Generate 10 Million rows of complex data
-- We inject 10% NULLs into the values, 5% NULLs into RMSEs, and group by random Categories
CREATE TABLE stress_data AS 
SELECT 
    chr((random() * 26 + 65)::INT) as category,
    CASE WHEN random() > 0.9 THEN NULL ELSE (random() * 100)::FLOAT END as val,
    CASE WHEN random() > 0.95 THEN NULL ELSE 0.5::FLOAT END as rmse
FROM range(10000000);

-- Enable query profiling to measure exact execution time
PRAGMA enable_profiling;

-- 2. Baseline Benchmark: Standard Aggregation
SELECT sum(val) FROM stress_data;

-- 3. AQP Benchmark: Native Approximate Query Processing (sum_ci)
SELECT sum_ci(val, rmse) FROM stress_data;

-- 4. Edge-Case Stress Test: GROUP BY with NULLs handling
SELECT category, sum_ci(val, rmse) 
FROM stress_data 
GROUP BY category 
ORDER BY category 
LIMIT 10;
