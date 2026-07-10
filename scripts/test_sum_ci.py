import duckdb
import math

# Connect with unsigned extensions allowed
con = duckdb.connect(config={"allow_unsigned_extensions": "true"})

# Load the extension
con.execute("LOAD 'build/release/extension/deferred_columns/deferred_columns.duckdb_extension'")

# Create table and insert heterogeneous (val, rmse) pairs
con.execute("CREATE TABLE test_ci (val DOUBLE, rmse DOUBLE)")
con.execute("INSERT INTO test_ci VALUES (10.0, 1.0), (20.0, 2.0), (30.0, 3.0), (40.0, 4.0)")

# Run sum_ci
result = con.execute("SELECT sum_ci(val::DOUBLE, rmse::DOUBLE) FROM test_ci").fetchone()
print("Raw result:", result[0])

# Expected correct answer (root-sum-of-squares formula)
sum_val = 10 + 20 + 30 + 40  # = 100
sum_rmse_sq = 1**2 + 2**2 + 3**2 + 4**2  # = 30
correct_ci = 1.96 * math.sqrt(sum_rmse_sq)
print(f"\nExpected sum: {sum_val}")
print(f"Expected CI (correct, root-sum-of-squares): 1.96 * sqrt({sum_rmse_sq}) = {correct_ci:.6f}")
print(f"Expected output: '{sum_val:.6f} +/- {correct_ci:.6f}'")

# Naive wrong formula: 1.96 * sqrt(N) * mean(rmse)
n = 4
mean_rmse = (1 + 2 + 3 + 4) / 4
naive_ci = 1.96 * math.sqrt(n) * mean_rmse
print(f"\nNaive CI (wrong, 1.96*sqrt(N)*mean_rmse): 1.96 * sqrt({n}) * {mean_rmse} = {naive_ci:.6f}")

# Check which formula the result matches
raw = result[0]
# Parse the CI from the result string
if '+/-' in str(raw):
    parts = str(raw).split('+/-')
    result_sum = float(parts[0].strip())
    result_ci = float(parts[1].strip())
    print(f"\nParsed sum: {result_sum}")
    print(f"Parsed CI:  {result_ci}")
    if abs(result_ci - correct_ci) < 0.01:
        print("\n==> RESULT MATCHES THE CORRECT (root-sum-of-squares) FORMULA")
    elif abs(result_ci - naive_ci) < 0.01:
        print("\n==> RESULT MATCHES THE NAIVE (WRONG) FORMULA")
    else:
        print(f"\n==> RESULT MATCHES NEITHER FORMULA (correct={correct_ci:.6f}, naive={naive_ci:.6f})")
else:
    print("\nCould not parse result format")
