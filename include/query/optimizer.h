#pragma once
#include "query/parser.h"
#include "query/catalog.h"
#include <string>
#include <vector>
#include <memory>

namespace FarhanDB {

// Execution plan types
enum class PlanType {
    FULL_SCAN,        // scan all pages (default)
    PRIMARY_KEY_SCAN, // scan using primary key equality — fast!
    JOIN_SCAN,        // nested loop join
    AGGREGATE_SCAN    // aggregate function scan
};

struct QueryPlan {
    PlanType    type;
    std::string table_name;
    std::string pk_value;      // for PRIMARY_KEY_SCAN
    std::string pk_column;     // primary key column name
    bool        has_where;
    std::string explanation;   // human readable plan description
};

class QueryOptimizer {
public:
    explicit QueryOptimizer(Catalog* catalog);

    // Analyze statement and return best execution plan
    QueryPlan   Optimize(std::shared_ptr<Statement> stmt);

    // Print the plan explanation
    std::string ExplainPlan(const QueryPlan& plan);

private:
    Catalog* catalog_;

    // Check if WHERE clause has primary key equality condition
    bool        HasPKEquality(std::shared_ptr<Statement> stmt,
                              const std::string& pk_col,
                              std::string& out_value);

    // Estimate row count for a table
    size_t      EstimateRowCount(const std::string& table_name);
};

} // namespace FarhanDB
