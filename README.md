# Deferred Columns

**A DuckDB Extension for Native ML Imputation via Virtual Partitioning**

This extension natively hooks into DuckDB to dynamically compute missing column values using Machine Learning models. Instead of performing expensive, full-table backfills when a schema evolves, this extension imputes missing values on-demand at scan time.

## Architecture & Documentation

Please refer to the following documents for technical details and research context:
- [Technical Whitepaper](WHITEPAPER.md): Discusses the theoretical background, Confidence-Gated ML Backfilling, MVCC-M (Multi-Version Concurrency Control for ML Models), and AQP (Approximate Query Processing).
- [Technical Documentation](TECHNICAL_DOCS.md): Explains the C++ implementation details, specifically how `TableFunction` and `OptimizerExtension` are utilized within the DuckDB engine.

## Interactive Demonstration

To test the extension without a local development environment, you can run our interactive cloud notebook:
- [Google Colab Demo](colab_demo.ipynb)

## Compiling from Source

This extension is built using the standard DuckDB extension template. A standard C++ compiler and CMake are required.

```bash
# Clone the repository
git clone https://github.com/iwinalbert/deferred-columns.git
cd deferred-columns

# Compile the extension in release mode
make release

# Run the automated SQL logic tests
make test
```

## Performance Highlights
Native C++ approximate aggregations (`sum_ci`) executed over 10,000,000 rows yield a 95% Confidence Interval in roughly `0.0245 seconds`, adding only ~15 milliseconds of overhead compared to DuckDB's standard `SUM()` function.

## Authors
- Thomas Albert Iwin
- Shakunth

See [AUTHORS.md](AUTHORS.md) for full attribution.
