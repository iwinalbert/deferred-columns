import duckdb
import threading
import time

conn = duckdb.connect(config={"allow_unsigned_extensions": "true"})
conn.execute("LOAD 'build/release/extension/deferred_columns/deferred_columns.duckdb_extension'")

# Register model 1
conn.execute("SELECT swap_model(1, 'model_v1.onnx')")

# Test 1: V1
v1_res = conn.execute("SELECT sum_ci(imputed_value::DOUBLE, rmse::DOUBLE) FROM deferred_scan('mock_model', 1000)").fetchall()

print(f"V1 result (1K rows): {v1_res[0]}")

# Function to run long query pinned to V1
long_res = None
def run_long_query():
    global long_res
    con2 = duckdb.connect(config={"allow_unsigned_extensions": "true"})
    con2.execute("LOAD 'build/release/extension/deferred_columns/deferred_columns.duckdb_extension'")
    t0 = time.time()
    # Runs slowly enough that we can swap mid-flight
    long_res = con2.execute("SELECT sum_ci(imputed_value::DOUBLE, rmse::DOUBLE) FROM deferred_scan('mock_model', 10000000)").fetchall()
    t1 = time.time()
    print(f"Long query finished in {t1-t0:.2f}s")

# Start long query thread
t = threading.Thread(target=run_long_query)
t.start()

time.sleep(1) # wait for query to start and pin version 1

print("Swapping to V2 mid-flight...")
conn.execute("SELECT swap_model(2, 'model_v2.onnx')")

# Test 2: V2
v2_res = conn.execute("SELECT sum_ci(imputed_value::DOUBLE, rmse::DOUBLE) FROM deferred_scan('mock_model', 1000)").fetchall()
print(f"V2 result (1K rows) [new query]: {v2_res[0]}")

t.join()
print(f"Long query result (10M rows) [pinned to V1]: {long_res[0]}")

