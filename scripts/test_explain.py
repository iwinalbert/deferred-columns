import duckdb

conn = duckdb.connect(config={"allow_unsigned_extensions": "true"})
conn.execute("LOAD 'build/release/extension/deferred_columns/deferred_columns.duckdb_extension';")

# Test 1: EXPLAIN with aggregation and filter
print("=" * 80)
print("TEST 1: EXPLAIN SELECT category, sum_ci(...) FROM deferred_scan WHERE date >= '2023-11-01' GROUP BY category")
print("=" * 80)
result = conn.execute("""
    EXPLAIN SELECT category, sum_ci(imputed_value::DOUBLE, rmse::DOUBLE)
    FROM deferred_scan('mock_model', 1000)
    WHERE date >= '2023-11-01'
    GROUP BY category;
""").fetchall()
for row in result:
    for col in row:
        print(col)
print()

# Test 2: Actually run the query (tests filter pushdown worst-case)
print("=" * 80)
print("TEST 2: RUN SELECT category, sum_ci(...) FROM deferred_scan WHERE date >= '2023-11-01' GROUP BY category")
print("=" * 80)
result = conn.execute("""
    SELECT category, sum_ci(imputed_value::DOUBLE, rmse::DOUBLE)
    FROM deferred_scan('mock_model', 1000)
    WHERE date >= '2023-11-01'
    GROUP BY category;
""").fetchall()
for row in result:
    print(row)
print()

# Test 3: EXPLAIN with filter on imputed column
print("=" * 80)
print("TEST 3: EXPLAIN SELECT * FROM deferred_scan WHERE imputed_value > 50.0")
print("=" * 80)
result = conn.execute("""
    EXPLAIN SELECT * FROM deferred_scan('mock_model', 1000)
    WHERE imputed_value > 50.0;
""").fetchall()
for row in result:
    for col in row:
        print(col)
print()

# Test 4: Actually run the imputed-column filter query (first 5 rows)
print("=" * 80)
print("TEST 4: RUN SELECT * FROM deferred_scan WHERE imputed_value > 50.0 (first 5 rows)")
print("=" * 80)
result = conn.execute("""
    SELECT * FROM deferred_scan('mock_model', 1000)
    WHERE imputed_value > 50.0
    LIMIT 5;
""").fetchall()
for row in result:
    print(row)
print()

print("ALL TESTS COMPLETED SUCCESSFULLY")
