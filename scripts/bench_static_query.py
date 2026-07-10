import duckdb
import time
import statistics
import os

# Connect with unsigned extensions allowed
con = duckdb.connect(config={"allow_unsigned_extensions": "true"})

# Load the extension
ext_path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build", "release", "extension", "deferred_columns", "deferred_columns.duckdb_extension")
con.execute(f"LOAD '{ext_path}'")

# Create the table
print("Creating stress_data table with 10M rows...")
con.execute("""
    CREATE TABLE stress_data AS
    SELECT
        chr((random() * 26 + 65)::INT) as category,
        '2023-11-0' || (random() * 9 + 1)::INT as date,
        (random() * 100)::FLOAT as imputed_value,
        0.5::FLOAT as rmse
    FROM range(10000000)
""")
print("Table created.")

# Run query 10 times with timing
times = []
query = """
    SELECT category, sum_ci(imputed_value::DOUBLE, rmse::DOUBLE)
    FROM stress_data
    WHERE date >= '2023-11-01'
    GROUP BY category
"""

for i in range(1, 11):
    start = time.time()
    con.execute(query).fetchall()
    elapsed = time.time() - start
    times.append(elapsed)
    print(f"Trial {i}: {elapsed:.4f}s")

mean_t = statistics.mean(times)
std_t = statistics.stdev(times)
print(f"\nMean: {mean_t:.4f}s")
print(f"Std Dev: {std_t:.4f}s")
