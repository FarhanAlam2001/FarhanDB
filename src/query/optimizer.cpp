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
    return schema->page_ids.size() * 50; // ~50 rows per page estimate
}

QueryPlan QueryOptimizer::Optimize(std::shared_ptr<Statement> stmt) {
    QueryPlan plan;
    plan.table_name = stmt->table_name;
    plan.has_where  = !stmt->conditions.empty();

    // ── Aggregate: always full scan ──────────────────────────────────────────
    if (!stmt->aggregate_func.empty()) {
        plan.type        = PlanType::AGGREGATE_SCAN;
        plan.explanation = "Aggregate scan on " + stmt->table_name +
                           " using " + stmt->aggregate_func;
        return plan;
    }

    // ── JOIN: nested loop with size estimate ─────────────────────────────────
    if (!stmt->join_table.empty()) {
        plan.type = PlanType::JOIN_SCAN;
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

    TableSchema* schema = catalog_->GetTable(stmt->table_name);
    if (schema) {
        // ── Priority 1: Primary key equality scan ────────────────────────────
        std::string pk_col;
        for (auto& col : schema->columns)
            if (col.is_primary_key) { pk_col = col.name; break; }

        std::string pk_value;
        if (!pk_col.empty() && HasPKEquality(stmt, pk_col, pk_value)) {
            plan.type        = PlanType::PRIMARY_KEY_SCAN;
            plan.pk_column   = pk_col;
            plan.pk_value    = pk_value;
            plan.explanation = "Primary key scan on " + stmt->table_name +
                               " WHERE " + pk_col + " = " + pk_value +
                               " (early exit on match)";
            return plan;
        }

        // ── Priority 2: B+ Tree index scan ──────────────────────────────────
        // Check each WHERE equality condition against known indexes
        for (const auto& cond : stmt->conditions) {
            if (cond.op != "=") continue; // index only helps with equality
            for (const auto& idx : schema->indexes) {
                if (idx.column_name == cond.column &&
                    idx.root_page_id != UINT32_MAX) {
                    plan.type             = PlanType::INDEX_SCAN;
                    plan.index_col        = cond.column;
                    plan.index_value      = cond.value;
                    plan.index_root_page  = idx.root_page_id;
                    plan.explanation      = "B+ Tree index scan on " +
                                           stmt->table_name + " using index on '" +
                                           cond.column + "'" +
                                           " WHERE " + cond.column +
                                           " = " + cond.value +
                                           " (O(log n) lookup)";
                    return plan;
                }
            }
        }
    }

    // ── Default: full table scan ─────────────────────────────────────────────
    plan.type        = PlanType::FULL_SCAN;
    plan.explanation = "Full table scan on " + stmt->table_name;
    if (plan.has_where)
        plan.explanation += " with WHERE filter";
    return plan;
}

std::string QueryOptimizer::ExplainPlan(const QueryPlan& plan) {
    std::ostringstream ss;
    ss << "\n  [Query Plan]\n";
    ss << "  Type       : ";
    switch (plan.type) {
        case PlanType::FULL_SCAN:        ss << "Full Table Scan";               break;
        case PlanType::PRIMARY_KEY_SCAN: ss << "Primary Key Scan (Optimized)";  break;
        case PlanType::INDEX_SCAN:       ss << "B+ Tree Index Scan (O(log n))"; break;
        case PlanType::JOIN_SCAN:        ss << "Nested Loop Join";               break;
        case PlanType::AGGREGATE_SCAN:   ss << "Aggregate Scan";                break;
    }
    ss << "\n  Details    : " << plan.explanation << "\n";
    return ss.str();
}

} // namespace FarhanDB
