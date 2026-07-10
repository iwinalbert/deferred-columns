import resource, os, duckdb

# Connect to DuckDB with allow_unsigned_extensions=true
conn = duckdb.connect(config={"allow_unsigned_extensions": "true"})

# Load the extension
conn.execute("LOAD 'build/release/extension/deferred_columns/deferred_columns.duckdb_extension'")

# Record initial RSS
initial_rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
print(f"Initial RSS = {initial_rss} KB")

# Loop from version 2 to 51 (50 iterations)
for version in range(2, 52):
    conn.execute(f"SELECT swap_model({version})")
    if (version - 1) % 10 == 0:
        rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        print(f"After {version-1} retrains: RSS = {rss} KB")

# Record final RSS
final_rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
print(f"\n--- Summary ---")
print(f"Initial RSS: {initial_rss} KB")
print(f"Final RSS:   {final_rss} KB")
print(f"Delta:       {final_rss - initial_rss} KB")
print(f"\nThe paper claims there is NO garbage collection of old model versions.")
print(f"If RSS grows, old versions are retained (no GC) - consistent with paper claim.")
print(f"If RSS stays flat, GC exists and the paper's limitation claim is stale.")
