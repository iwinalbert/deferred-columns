import duckdb
import pyarrow as pa
import numpy as np
import time
import onnxruntime as ort

print("Starting Static Backfill Pipeline...")
conn = duckdb.connect(config={'allow_unsigned_extensions': 'true'})
conn.execute("LOAD 'build/release/extension/deferred_columns/deferred_columns.duckdb_extension'")

# 1. Generate Base Data
print("Generating 10M rows of base data...")
conn.execute("""
CREATE TABLE stress_data AS 
SELECT 
    chr((random() * 26 + 65)::INT) as category,
    '2023-11-0' || (random() * 9 + 1)::INT as date,
    (random() * 100)::FLOAT as f1,
    (random() * 100)::FLOAT as f2,
    (random() * 100)::FLOAT as f3,
    0.5::FLOAT as rmse
FROM range(10000000);
""")

# 2. Python Data Export (Zero-copy PyArrow)
print("Step 1: Python Data Export (fetch_arrow)...")
t0 = time.time()
table = conn.execute("SELECT f1, f2, f3 FROM stress_data").fetch_arrow_table()
export_time = time.time() - t0

# 3. External ML Inference
print("Step 2: External ML Inference (onnxruntime)...")
t1 = time.time()
session = ort.InferenceSession("model.onnx", providers=['CPUExecutionProvider'])
# Process in batches of 2048 to match C++
f1_arr = table.column('f1').to_numpy()
f2_arr = table.column('f2').to_numpy()
f3_arr = table.column('f3').to_numpy()

X = np.stack([f1_arr, f2_arr, f3_arr], axis=1).astype(np.float32)
# Zero-fill NULLs (our generated data doesn't have true NULLs here to simplify script, but it matches C++ zero-fill)
X[np.isnan(X)] = 0.0

batch_size = 2048
results = []
for i in range(0, len(X), batch_size):
    batch = X[i:i+batch_size]
    res = session.run(["variable"], {"float_input": batch})[0]
    results.append(res.flatten())

imputed_values = np.concatenate(results)
inference_time = time.time() - t1

# 4. Bulk UPDATE & Commit
print("Step 3: Bulk UPDATE & Commit (Appender + Atomic Swap)...")
t2 = time.time()
# Create temporary table with same schema + imputed_value
conn.execute("""
CREATE TABLE stress_data_temp (
    category VARCHAR,
    date VARCHAR,
    f1 FLOAT,
    f2 FLOAT,
    f3 FLOAT,
    rmse FLOAT,
    imputed_value FLOAT
)
""")

# For appending efficiently, we create an arrow table and insert it
full_table = conn.execute("SELECT * FROM stress_data").fetch_arrow_table()
full_table = full_table.append_column("imputed_value", pa.array(imputed_values))

conn.execute("INSERT INTO stress_data_temp SELECT * FROM full_table")
# Atomic swap
conn.execute("DROP TABLE stress_data")
conn.execute("ALTER TABLE stress_data_temp RENAME TO stress_data")
commit_time = time.time() - t2

total_time = export_time + inference_time + commit_time

print(f"\nStatic Backfill Pipeline Results:")
print(f"Python Data Export:    {export_time*1000:.1f} ms")
print(f"External ML Inference: {inference_time*1000:.1f} ms")
print(f"Bulk UPDATE & Commit:  {commit_time*1000:.1f} ms")
print(f"Total Latency:         {total_time*1000:.1f} ms")
