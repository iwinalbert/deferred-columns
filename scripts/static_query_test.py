import duckdb
import time

conn = duckdb.connect(config={'allow_unsigned_extensions': 'true'})
conn.execute("LOAD 'build/release/extension/deferred_columns/deferred_columns.duckdb_extension'")
conn.execute("CREATE TABLE stress_data AS SELECT chr((random() * 26 + 65)::INT) as category, '2023-11-0' || (random() * 9 + 1)::INT as date, (random() * 100)::FLOAT as imputed_value, 0.5::FLOAT as rmse FROM range(10000000)")

t0 = time.time()
conn.execute("SELECT category, sum_ci(imputed_value, rmse) FROM stress_data WHERE date >= '2023-11-01' GROUP BY category").fetchall()
t1 = time.time()
print(f"Static Query Time: {(t1-t0)*1000:.1f} ms")
