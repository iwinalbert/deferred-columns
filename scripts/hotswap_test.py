import duckdb
import threading
import time

conn = duckdb.connect(config={'allow_unsigned_extensions': 'true'})
conn.execute("LOAD 'build/release/extension/deferred_columns/deferred_columns.duckdb_extension'")

print("Executing long running query over 100M rows in background...")

results = []

def run_query():
    cursor = conn.cursor()
    # Takes ~13-20 seconds
    res = cursor.execute("SELECT sum_ci(imputed_value, rmse) FROM deferred_scan('mock_model', 100000000)").fetchall()
    results.append(("Query Result", res))

t = threading.Thread(target=run_query)
t.start()

print("Sleeping 2 seconds, then hot-swapping model version...")
time.sleep(2)
cursor2 = conn.cursor()
res2 = cursor2.execute("SELECT swap_model(2)").fetchall()
results.append(("Swap Result", res2))
print("Model swapped to version 2.")

t.join()
print("All finished!")
for r in results:
    print(r)
