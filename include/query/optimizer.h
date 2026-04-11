#pragma once
#include "query/parser.h"
#include "query/catalog.h"
#include <string>
#include <vector>
#include <memory>

namespace FarhanDB {

enum class PlanType {
    FULL_SCAN,          // scan all pages (default)
    PRIMARY_KEY_SCAN,   // equality on primary key — O(n) but early exit
    INDEX_SCAN,         // equality on indexed column via B+ Tree — O(log n)
    JOIN_SCAN,          // nested loop join
    AGGREGATE_SCAN      // aggregate function scan
};

struct QueryPlan {
    PlanType    type;
    std::string table_name;

    // PRIMARY_KEY_SCAN fields
    std::string pk_value;
    std::string pk_column;

    // INDEX_SCAN fields
    std::string index_col;          // column that has the index
    std::string index_value;        // equality value from WHERE
    uint32_t    index_root_page  = UINT32_MAX; // B+ Tree root page id

    bool        has_where;
    std::string explanation;
};

class QueryOptimizer {
public:
    explicit QueryOptimizer(Catalog* catalog);

    QueryPlan   Optimize(std::shared_ptr<Statement> stmt);
    std::string ExplainPlan(const QueryPlan& plan);

private:
    Catalog* catalog_;

    bool    HasPKEquality(std::shared_ptr<Statement> stmt,
                          const std::string& pk_col,
                          std::string& out_value);

    size_t  EstimateRowCount(const std::string& table_name);
};

} // namespace FarhanDB
