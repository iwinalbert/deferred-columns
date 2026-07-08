# Deferred Columns: ML Imputation as a Native Database Primitive in DuckDB

**A Technical Whitepaper — Scoped Undergraduate Research Proposal**

**Authors:** 
1. Thomas Albert Iwin
2. Shakunth

---

## Abstract

When a schema adds a new column to a table with pre-existing rows, those historical rows have no value for it. The standard options today are: leave them `NULL`, fill them with a static default, or run an expensive full-table backfill. This paper proposes an alternative: compute the missing value **on demand**, using a lightweight ML model natively integrated into the database engine. 

Instead of an external middleware layer, this project embeds ML inference directly into the columnar execution pipeline of **DuckDB**. By hooking into DuckDB's `TableFunction` and `OptimizerExtension` interfaces, we demonstrate lazy materialization of ML columns. Furthermore, we natively solve two critical challenges in in-database ML: **Multi-Version Concurrency Control for ML Models (MVCC-M)** to guarantee deterministic reads during model retraining, and **Approximate Query Processing (AQP)** to provide statistical confidence intervals over imputed data.

---

## 1. Problem Statement

Given a relational table `T` and a newly added column `C`, for every pre-existing row `r`, `r.C` is unknown. We want to produce a usable value for `r.C` without:
- Blocking reads/writes on `T` during a bulk backfill
- Destroying query performance by pulling data out of the database into external ML scripts
- Returning confident-looking single-point predictions without statistical error bounds
- Suffering from "phantom reads" where the underlying ML model is updated mid-transaction.

---

## 2. Existing Technology (Prior Art)

| System / Work | What it actually does | Relevance to us |
|---|---|---|
| **ImputeDB** (VLDB 2017) | A SQL database that imputes missing values on-the-fly at query time. | Establishes the core idea of query-time imputation. |
| **QUIP** (2022) / **ZIP** (VLDB 2023) | Adds a cost-based decision function inside relational operators to decide whether to impute a missing value immediately or defer it. | Validates the need for cost-aware execution pipelines, inspiring our `OptimizerExtension` integration. |
| **Tesseract** (PVLDB 2022) | Models schema evolution as a data-modification operation via MVCC. | Solves *how to change structure safely*. |

**Our Novel Contribution:** Integrating deferred ML materialization, model version pinning (MVCC-M), and AQP bounding natively into a columnar OLAP engine (DuckDB) via extension hooks.

---

## 3. Our Claims & Implementations

1. **C1 — Zero-downtime column availability via Lazy Materialization.** Implemented via DuckDB `TableFunction` (`deferred_scan`). Columns exist virtually and evaluate ML inference purely on demand.
2. **C2 — Cost-Based Pruning.** Implemented via DuckDB `OptimizerExtension`. Establishes the architectural hook to hijack the logical query plan, inject inference costs ($C_{inf}$), and push filters down so ML models only evaluate surviving rows.
3. **C3 — Deterministic Reads via MVCC-M.** Implemented via a thread-safe `ModelRegistry`. When a query begins, the binder queries the registry to pin the exact active version of a model. This guarantees query consistency even if the underlying model is asynchronously retrained mid-query.
4. **C4 — Statistical Error Bounding via AQP.** Implemented via native C++ `AggregateFunction` (`sum_ci`). Rather than blinding summing imputed data, the aggregate natively computes the 95% Confidence Interval dynamically using the model's Root Mean Square Error (RMSE).

---

## 4. Architecture (DuckDB C++ Extension)

We discard the traditional "external Python middleware" architecture in favor of a high-performance native extension within DuckDB.

```
┌────────────────────────────────────────────────────────┐
│ DuckDB Database Engine                                 │
│                                                        │
│  ┌─────────────────┐             ┌──────────────────┐  │
│  │ SQL Parser      │ ──────────▶ │ Optimizer Planner│  │
│  └─────────────────┘             └────────┬─────────┘  │
│                                           │            │
│  ┌────────────────────────────────────────▼─────────┐  │
│  │ Deferred Columns Extension                       │  │
│  │                                                  │  │
│  │ 1. OptimizerExtension (DeferredPreOptimize)      │  │
│  │ 2. TableFunction (deferred_scan)                 │  │
│  │ 3. AggregateFunction (sum_ci)                    │  │
│  │ 4. ModelRegistry (MVCC-M Catalog)                │  │
│  └──────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────┘
```

By keeping the ML models and logical plans inside the database engine's address space, we avoid the massive serialization overhead of exporting DuckDB `DataChunks` to external Python processes.

---

## 5. Model Versioning (MVCC-M)

A classic problem with live ML inference in databases is non-deterministic reads: if a transaction executes a long-running query, and the ML model is retrained and hot-swapped halfway through, the first half of the rows use Model $v1$ and the second half use Model $v2$. 

We solve this using **MVCC-M**. At query bind time, the `DeferredScanBind` function reaches out to the `ModelRegistry` singleton and resolves the current highest active version (e.g., $v1$). This version ID is embedded into the query's `GlobalTableFunctionState`. Regardless of how long the query takes, or if $v2$ is deployed, this specific query is mathematically locked to evaluate using $v1$.

---

## 6. Approximate Query Processing (AQP)

ML predictions are inherently uncertain. Storing a predicted `42.0` as if it is a ground-truth measurement is a data governance failure. 

To solve this natively, we expose confidence intervals directly in SQL. We implemented a custom `sum_ci` aggregate function. When aggregating imputed values, `sum_ci` accepts the model's reported RMSE. As DuckDB vectorizes the rows, our C++ struct calculates:
$$ \text{CI} = 1.96 \times \sqrt{N} \times \text{RMSE} $$
Outputting bounded strings directly to the user (e.g., `210.0 +/- 2.191`).

---

## 7. Evaluation Plan

1. **Execution Overhead:** Measure the latency cost of evaluating `deferred_scan` vs statically materialized columns in a Parquet file.
2. **Optimizer Efficacy:** Benchmark query times when the `OptimizerExtension` successfully pushes down filters vs when inference is forced to run on all rows prior to filtering.
3. **AQP Performance:** Compare the execution time of our native `sum_ci` C++ aggregate vs manually calculating bounds using standard deviation sub-queries in raw SQL.

---

## 8. Conclusion

By building "Deferred Columns" directly into the DuckDB engine, we successfully demonstrated that ML backfilling does not need to be a fragile external pipeline. With native MVCC-M and AQP, ML models can be treated as first-class relational schema primitives.
