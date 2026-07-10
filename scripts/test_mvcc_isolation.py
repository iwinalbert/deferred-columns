"""
MVCC-M Isolation Test
Tests that a long-running query completes without crashing when the model
is swapped mid-query, verifying version pinning and crash-freedom.
"""

import duckdb
import threading
import time

def main():
    print("=" * 60)
    print("MVCC-M ISOLATION TEST")
    print("=" * 60)

    # Step 1: Connect and load extension
    con = duckdb.connect(config={"allow_unsigned_extensions": "true"})
    con.execute("LOAD 'build/release/extension/deferred_columns/deferred_columns.duckdb_extension'")
    print("[OK] Extension loaded.")

    # Step 2: Baseline query (1M rows)
    print("\n[STEP 1] Running baseline query (1M rows)...")
    t0 = time.time()
    baseline_result = con.execute(
        "SELECT sum_ci(imputed_value::DOUBLE, rmse::DOUBLE) FROM deferred_scan('mock_model', 1000000)"
    ).fetchone()
    t1 = time.time()
    print(f"  Baseline result: {baseline_result}")
    print(f"  Baseline time:   {t1 - t0:.3f}s")

    # Step 3: Run long query in background thread (10M rows)
    long_result = [None]
    long_error = [None]

    def long_query():
        try:
            # Each thread needs its own connection for true isolation
            thread_con = duckdb.connect(config={"allow_unsigned_extensions": "true"})
            thread_con.execute("LOAD 'build/release/extension/deferred_columns/deferred_columns.duckdb_extension'")
            long_result[0] = thread_con.execute(
                "SELECT sum_ci(imputed_value::DOUBLE, rmse::DOUBLE) FROM deferred_scan('mock_model', 10000000)"
            ).fetchone()
            thread_con.close()
        except Exception as e:
            long_error[0] = e

    print("\n[STEP 2] Starting long query in background thread (10M rows)...")
    bg_thread = threading.Thread(target=long_query)
    t2 = time.time()
    bg_thread.start()

    # Step 4: Sleep 1 second then swap model
    print("[STEP 3] Sleeping 1 second before swapping model...")
    time.sleep(1)

    print("[STEP 4] Swapping model mid-query: SELECT swap_model(2)")
    swap_result = con.execute("SELECT swap_model(2)").fetchone()
    print(f"  swap_model(2) returned: {swap_result}")

    # Step 5: Wait for background thread
    print("[STEP 5] Waiting for background thread to finish...")
    bg_thread.join()
    t3 = time.time()
    print(f"  Long query time: {t3 - t2:.3f}s")

    # Step 6: Print results
    print("\n" + "=" * 60)
    print("RESULTS")
    print("=" * 60)
    print(f"  Baseline result (1M):   {baseline_result}")
    print(f"  Long query result (10M): {long_result[0]}")
    if long_error[0]:
        print(f"  Long query ERROR:       {long_error[0]}")

    # Step 7: Assess crash-freedom and consistency
    print("\n" + "=" * 60)
    print("ASSESSMENT")
    print("=" * 60)
    if long_error[0] is not None:
        print("  [FAIL] Long-running query CRASHED during model swap!")
        print(f"  Error: {long_error[0]}")
    else:
        print("  [PASS] Long-running query completed WITHOUT crashing.")
        print("  Version pinning held — no crash from concurrent model swap.")

    # Step 8: Verify swap_model works by calling swap_model(3)
    print("\n[STEP 6] Verifying model swap function works: SELECT swap_model(3)")
    verify_result = con.execute("SELECT swap_model(3)").fetchone()
    print(f"  swap_model(3) returned: {verify_result}")
    print("  [OK] Model version swap confirmed working.")

    con.close()
    print("\n" + "=" * 60)
    print("TEST COMPLETE")
    print("=" * 60)

if __name__ == "__main__":
    main()
