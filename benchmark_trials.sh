export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(pwd)/onnxruntime-linux-x64-1.17.1/lib
for i in {1..10}; do
  echo "Trial $i"
  build/release/duckdb -unsigned -c ".timer on" -c "LOAD 'build/release/extension/deferred_columns/deferred_columns.duckdb_extension';" -c "SELECT category, sum_ci(imputed_value, rmse) FROM deferred_scan('mock_model', 10000000) WHERE date >= '2023-11-01' GROUP BY category;" | grep "Run Time"
done
