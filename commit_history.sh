git add Makefile CMakeLists.txt extension_config.cmake .github/workflows/MainDistributionPipeline.yml
git commit -m "chore: bootstrap deferred_columns extension from duckdb template

Co-authored-by: Shakunth <shakunth@example.com>"

git add src/include/deferred_columns_extension.hpp src/deferred_columns_extension.cpp src/deferred_scan.cpp
git commit -m "feat: implement deferred_scan TableFunction for lazy ML materialization

Co-authored-by: Shakunth <shakunth@example.com>"

git add src/deferred_optimizer.cpp
git commit -m "feat: implement OptimizerExtension for cost-based inference pruning

Co-authored-by: Shakunth <shakunth@example.com>"

git add src/include/model_registry.hpp src/model_registry.cpp src/include/onnxruntime_cxx_api.h
git commit -m "feat: implement ModelRegistry for MVCC-M model versioning

Co-authored-by: Shakunth <shakunth@example.com>"

git add src/include/deferred_aggregates.hpp src/deferred_aggregates.cpp
git commit -m "feat: implement sum_ci aggregate for Approximate Query Processing

Co-authored-by: Shakunth <shakunth@example.com>"

git add test/sql/deferred_columns.test benchmark.py
git commit -m "test: add SQL logic tests and python benchmark script

Co-authored-by: Shakunth <shakunth@example.com>"

git add README.md WHITEPAPER.md TECHNICAL_DOCS.md AUTHORS.md
git commit -m "docs: add whitepaper and technical documentation

Co-authored-by: Shakunth <shakunth@example.com>"

git add colab_demo.ipynb generate_colab.py
git commit -m "docs: add Google Colab demonstration notebook

Co-authored-by: Shakunth <shakunth@example.com>"
