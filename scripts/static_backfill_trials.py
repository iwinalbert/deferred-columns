import duckdb
import pyarrow as pa
import numpy as np
import time
import onnxruntime as ort

print("Starting Static Backfill Pipeline (10 Trials)...")
conn = duckdb.connect()

# 1. Generate Base Data
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

export_times = []
inference_times = []
commit_times = []
total_times = []

for trial in range(10):
    t0 = time.time()
    table = conn.execute("SELECT f1, f2, f3 FROM stress_data").fetch_arrow_table()
    export_time = time.time() - t0
    
    t1 = time.time()
    session = ort.InferenceSession("model.onnx", providers=['CPUExecutionProvider'])
    f1_arr = table.column('f1').to_numpy()
    f2_arr = table.column('f2').to_numpy()
    f3_arr = table.column('f3').to_numpy()
    X = np.stack([f1_arr, f2_arr, f3_arr], axis=1).astype(np.float32)
    
    batch_size = 2048
    results = []
    for i in range(0, len(X), batch_size):
        batch = X[i:i+batch_size]
        res = session.run(["variable"], {"float_input": batch})[0]
        results.append(res.flatten())
    
    imputed_values = np.concatenate(results)
    inference_time = time.time() - t1
    
    t2 = time.time()
    conn.execute("""
    CREATE TABLE stress_data_temp (
        category VARCHAR, date VARCHAR, f1 FLOAT, f2 FLOAT, f3 FLOAT, rmse FLOAT, imputed_value FLOAT
    )
    """)
    full_table = conn.execute("SELECT * FROM stress_data").fetch_arrow_table()
    full_table = full_table.append_column("imputed_value", pa.array(imputed_values))
    conn.execute("INSERT INTO stress_data_temp SELECT * FROM full_table")
    conn.execute("DROP TABLE stress_data_temp") # Avoid actual swap loop state explosion
    commit_time = time.time() - t2
    
    total_time = export_time + inference_time + commit_time
    
    export_times.append(export_time * 1000)
    inference_times.append(inference_time * 1000)
    commit_times.append(commit_time * 1000)
    total_times.append(total_time * 1000)
    
    print(f"Trial {trial+1}: Total {total_time*1000:.1f}ms")

print(f"\nStatic Backfill Pipeline Results (10 Trials):")
print(f"Python Data Export:    {np.mean(export_times):.1f} ms (±{np.std(export_times):.1f})")
print(f"External ML Inference: {np.mean(inference_times):.1f} ms (±{np.std(inference_times):.1f})")
print(f"Bulk UPDATE & Commit:  {np.mean(commit_times):.1f} ms (±{np.std(commit_times):.1f})")
print(f"Total Latency:         {np.mean(total_times):.1f} ms (±{np.std(total_times):.1f})")
