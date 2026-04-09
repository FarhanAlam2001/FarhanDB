#include "query/optimizer.h"
#include <sstream>

namespace FarhanDB {

QueryOptimizer::QueryOptimizer(Catalog* catalog)
    : catalog_(catalog) {}

bool QueryOptimizer::HasPKEquality(std::shared_ptr<Statement> stmt,
                                    const std::string& pk_col,
                                    std::string& out_value) {
    for (const auto& cond : stmt->conditions) {
        if (cond.column == pk_col && cond.op == "=") {
            out_value = cond.value;
            return true;
        }
    }
    return false;
}

size_t QueryOptimizer::EstimateRowCount(const std::string& table_name) {
    TableSchema* schema = catalog_->GetTable(table_name);
    if (!schema) return 0;
    // Estimate: each page holds ~50 rows on average
    return schema->page_ids.size() * 50;
}

QueryPlan QueryOptimizer::Optimize(std::shared_ptr<Statement> stmt) {
    QueryPlan plan;
    plan.table_name = stmt->table_name;
    plan.has_where  = !stmt->conditions.empty();

    // Aggregate queries — always full scan
    if (!stmt->aggregate_func.empty()) {
        plan.type        = PlanType::AGGREGATE_SCAN;
        plan.explanation = "Aggregate scan on " + stmt->table_name +
                           " using " + stmt->aggregate_func;
        return plan;
    }

    // JOIN queries
    if (!stmt->join_table.empty()) {
        plan.type = PlanType::JOIN_SCAN;

        // Optimization: put smaller table on left for nested loop join
        size_t left_count  = EstimateRowCount(stmt->table_name);
        size_t right_count = EstimateRowCount(stmt->join_table);

        if (right_count < left_count) {
            plan.explanation = "Nested Loop Join: " + stmt->join_table +
                               " (smaller, " + std::to_string(right_count) +
                               " est. rows) x " + stmt->table_name +
                               " (" + std::to_string(left_count) + " est. rows)";
        } else {
            plan.explanation = "Nested Loop Join: " + stmt->table_name +
                               " (" + std::to_string(left_count) +
                               " est. rows) x " + stmt->join_table +
                               " (" + std::to_string(right_count) + " est. rows)";
        }
        return plan;
    }

    // SELECT/DELETE/UPDATE — check for primary key optimization
    TableSchema* schema = catalog_->GetTable(stmt->table_name);
    if (schema) {
        // Find primary key column
        std::string pk_col = "";
        for (auto& col : schema->columns)
            if (col.is_primary_key) { pk_col = col.name; break; }

        // Check if WHERE uses primary key with equality
        std::string pk_value;
        if (!pk_col.empty() && HasPKEquality(stmt, pk_col, pk_value)) {
            plan.type        = PlanType::PRIMARY_KEY_SCAN;
            plan.pk_column   = pk_col;
            plan.pk_value    = pk_value;
            plan.explanation = "Primary key scan on " + stmt->table_name +
                               " WHERE " + pk_col + " = " + pk_value +
                               " (skips full table scan)";
            return plan;
        }
    }

    // Default — full table scan
    plan.type        = PlanType::FULL_SCAN;
    plan.explanation = "Full table scan on " + stmt->table_name;
    if (plan.has_where)
        plan.explanation += " with filter on WHERE clause";

    return plan;
}

std::string QueryOptimizer::ExplainPlan(const QueryPlan& plan) {
    std::ostringstream ss;
    ss << "\n  [Query Plan]\n";
    ss << "  Type       : ";
    switch (plan.type) {
        case PlanType::FULL_SCAN:        ss << "Full Table Scan"; break;
        case PlanType::PRIMARY_KEY_SCAN: ss << "Primary Key Scan (Optimized)"; break;
        case PlanType::JOIN_SCAN:        ss << "Nested Loop Join"; break;
        case PlanType::AGGREGATE_SCAN:   ss << "Aggregate Scan"; break;
    }
    ss << "\n";
    ss << "  Details    : " << plan.explanation << "\n";
    return ss.str();
}

} // namespace FarhanDB
