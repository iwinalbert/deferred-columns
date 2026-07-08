![Deferred Columns Banner](assets/banner.jpg)

# Deferred Columns: Native ML Imputation in DuckDB

[![Open In Colab](https://colab.research.google.com/assets/colab-badge.svg)](https://colab.research.google.com/github/iwinalbert/deferred-columns/blob/main/colab_demo.ipynb)

When a schema adds a new column to a table with pre-existing rows, those historical rows have no value for it. This repository proposes an alternative to expensive full-table backfills: computing the missing value **on demand**, using a lightweight ML model natively integrated into the **DuckDB** database engine.

By hooking into DuckDB's `TableFunction` and `OptimizerExtension` interfaces, this extension demonstrates lazy materialization of ML columns. Furthermore, we natively solve two critical challenges in in-database ML: 
1. **MVCC-M (Multi-Version Concurrency Control for ML Models)** to guarantee deterministic reads during model retraining.
2. **AQP (Approximate Query Processing)** to provide statistical confidence intervals over imputed data.

---

## 🚀 Quickstart & Interactive Demo

The easiest way to test the extension without installing anything locally is to use our interactive cloud notebook! Click the **Open in Colab** badge at the top of this page to compile and run the extension natively in your browser.

## 📚 Documentation
Please read our official publications in this repository for a deep dive into the architecture:
- [Technical Whitepaper](WHITEPAPER.md)
- [Technical Documentation](TECHNICAL_DOCS.md)

## 🛠️ Building Locally

This project requires a standard C++ compiler and CMake.

```bash
# Clone the repository
git clone https://github.com/iwinalbert/deferred-columns.git
cd deferred-columns

# Compile the DuckDB engine and our custom extension
make release

# Run the automated SQL logic tests
make test
```

## 📊 Performance Benchmarks
We ran a performance profiling test using DuckDB's internal query profiler to prove the speed of our custom native C++ components aggregating **10,000,000 rows**:

1. **Standard `SUM()` Baseline:** 
   * Execution time: **0.0093 seconds**
2. **Native `sum_ci()` (Approximate Query Processing):** 
   * Execution time: **0.0245 seconds**

By pushing the statistical error-bounding math directly into the DuckDB C++ execution engine, we can compute 95% Confidence Intervals for 10 million rows with a negligible overhead of just ~15 milliseconds.

## 🤝 Authors
Developed as a scoped undergraduate research project by:
- **Thomas Albert Iwin**
- **Shakunth**

See [AUTHORS.md](AUTHORS.md) for more details.
