import duckdb
import time

con = duckdb.connect()
con.execute("LOAD 'build/release/extension/deferred_columns/deferred_columns.duckdb_extension'")

print("Generating 10 million rows of test data...")
con.execute("CREATE TABLE test_data AS SELECT (random() * 100)::FLOAT AS val FROM range(10000000)")

print("\n--- Benchmarking Standard Aggregation ---")
t0 = time.time()
res1 = con.execute("SELECT sum(val) FROM test_data").fetchall()
t1 = time.time()
print(f"Result: {res1[0][0]}")
print(f"Execution Time: {(t1-t0)*1000:.2f} ms")

print("\n--- Benchmarking Native AQP (sum_ci) ---")
t0 = time.time()
res2 = con.execute("SELECT sum_ci(val, 0.5) FROM test_data").fetchall()
t1 = time.time()
print(f"Result: {res2[0][0]}")
print(f"Execution Time: {(t1-t0)*1000:.2f} ms")
