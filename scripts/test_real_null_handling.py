import duckdb

conn = duckdb.connect(config={"allow_unsigned_extensions": "true"})
conn.execute("LOAD 'build/release/extension/deferred_columns/deferred_columns.duckdb_extension'")

conn.execute("""
    CREATE TABLE stress_data (
        category VARCHAR,
        date VARCHAR,
        f1 FLOAT,
        f2 FLOAT,
        f3 FLOAT,
        rmse FLOAT
    )
""")

# Insert a row with NULLs in f1 and f3
conn.execute("INSERT INTO stress_data VALUES ('A', '2023-11-01', NULL, 1.5, NULL, 0.5)")

result = conn.execute("SELECT * FROM deferred_scan('mock_model', 1)").fetchall()
print("Result with NULLs:")
for row in result:
    print(row)
