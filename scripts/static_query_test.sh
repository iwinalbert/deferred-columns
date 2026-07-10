build/release/duckdb -c "
CREATE TABLE stress_data AS SELECT chr((random() * 26 + 65)::INT) as category, '2023-11-0' || (random() * 9 + 1)::INT as date, (random() * 100)::FLOAT as imputed_value, 0.5::FLOAT as rmse FROM range(10000000);
.timer on
SELECT category, sum_ci(imputed_value, rmse) FROM stress_data WHERE date >= '2023-11-01' GROUP BY category;"
