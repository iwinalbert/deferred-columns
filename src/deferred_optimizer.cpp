#include "duckdb/optimizer/optimizer_extension.hpp"
#include "duckdb/planner/logical_operator.hpp"
#include "duckdb/planner/operator/logical_get.hpp"
#include "duckdb/planner/operator/logical_filter.hpp"
#include "duckdb/planner/expression/bound_columnref_expression.hpp"
#include "duckdb/planner/expression/bound_comparison_expression.hpp"
#include "duckdb/planner/expression/bound_constant_expression.hpp"
#include "duckdb/planner/expression/bound_cast_expression.hpp"
#include "duckdb/planner/filter/constant_filter.hpp"
#include "duckdb/planner/expression_iterator.hpp"

namespace duckdb {

static bool ReferencesImputedColumn(Expression &expr, const vector<ColumnIndex> &column_ids, idx_t imputed_col_idx) {
	if (expr.type == ExpressionType::BOUND_COLUMN_REF) {
		auto &colref = expr.Cast<BoundColumnRefExpression>();
		if (colref.binding.column_index < column_ids.size()) {
			idx_t actual_col_idx = column_ids[colref.binding.column_index].GetPrimaryIndex();
			if (actual_col_idx == imputed_col_idx) {
				return true;
			}
		}
	}
	bool references = false;
	ExpressionIterator::EnumerateChildren(expr, [&](Expression &child) {
		if (ReferencesImputedColumn(child, column_ids, imputed_col_idx)) {
			references = true;
		}
	});
	return references;
}

void DeferredPreOptimize(OptimizerExtensionInput &input, unique_ptr<LogicalOperator> &plan) {
	// Traverse the logical query plan tree.
	if (plan->type == LogicalOperatorType::LOGICAL_FILTER) {
		auto &filter = plan->Cast<LogicalFilter>();
		if (filter.children[0]->type == LogicalOperatorType::LOGICAL_GET) {
			auto &get = filter.children[0]->Cast<LogicalGet>();
			if (get.function.name == "deferred_scan") {
				// Find imputed_value column index (it's the last one, index 6)
				idx_t imputed_idx = 6;
				auto &col_ids = get.GetColumnIds();

				vector<unique_ptr<Expression>> remaining_filters;

				for (auto &expr : filter.expressions) {
					bool pushed = false;
					if (!ReferencesImputedColumn(*expr, col_ids, imputed_idx)) {
						// Try to push down
						if (expr->expression_class == ExpressionClass::BOUND_COMPARISON) {
							auto &comp = expr->Cast<BoundComparisonExpression>();
							Expression *right_expr = comp.right.get();
							if (right_expr->expression_class == ExpressionClass::BOUND_CAST) {
								auto &cast = right_expr->Cast<BoundCastExpression>();
								right_expr = cast.child.get();
							}

							if (comp.left->expression_class == ExpressionClass::BOUND_COLUMN_REF &&
							    right_expr->expression_class == ExpressionClass::BOUND_CONSTANT) {
								auto &colref = comp.left->Cast<BoundColumnRefExpression>();
								auto &const_expr = right_expr->Cast<BoundConstantExpression>();

								idx_t actual_col_idx = col_ids[colref.binding.column_index].GetPrimaryIndex();
								get.table_filters.filters[actual_col_idx] =
								    make_uniq<ConstantFilter>(comp.type, const_expr.value);
								pushed = true;
							}
						}
					}

					if (!pushed) {
						remaining_filters.push_back(std::move(expr));
					}
				}

				if (remaining_filters.empty()) {
					// Remove LogicalFilter by replacing it with its child (LogicalGet)
					plan = std::move(plan->children[0]);
					return;
				} else {
					filter.expressions.clear();
					for (auto &expr : remaining_filters) {
						filter.expressions.push_back(std::move(expr));
					}
				}
			}
		}
	}

	// Recursively traverse
	for (auto &child : plan->children) {
		DeferredPreOptimize(input, child);
	}
}

} // namespace duckdb
