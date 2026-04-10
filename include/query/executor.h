#pragma once
#include "query/parser.h"
#include "query/catalog.h"
#include "query/optimizer.h"
#include "storage/buffer_pool.h"
#include "transaction/transaction_manager.h"
#include "transaction/lock_manager.h"
#include <vector>
#include <string>
#include <set>

namespace FarhanDB {

using Row    = std::vector<std::string>;
using Result = std::vector<Row>;

struct ExecutionResult {
    bool        success;
    std::string message;
    Result      rows;
    std::vector<std::string> column_names;
    std::string query_plan;
};

class Executor {
public:
    Executor(BufferPoolManager* bpm,
             Catalog*           catalog,
             TransactionManager* txn_mgr,
             LockManager*       lock_mgr);

    ExecutionResult Execute(std::shared_ptr<Statement> stmt);
    std::string     Explain(std::shared_ptr<Statement> stmt);

private:
    BufferPoolManager*  bpm_;
    Catalog*            catalog_;
    TransactionManager* txn_mgr_;
    LockManager*        lock_mgr_;
    QueryOptimizer      optimizer_;
    txn_id_t            current_txn_id_;

    ExecutionResult ExecCreateTable(std::shared_ptr<Statement> stmt);
    ExecutionResult ExecDropTable(std::shared_ptr<Statement> stmt);
    ExecutionResult ExecCreateIndex(std::shared_ptr<Statement> stmt);
    ExecutionResult ExecInsert(std::shared_ptr<Statement> stmt);
    ExecutionResult ExecSelect(std::shared_ptr<Statement> stmt, const QueryPlan& plan);
    ExecutionResult ExecDelete(std::shared_ptr<Statement> stmt, const QueryPlan& plan);
    ExecutionResult ExecUpdate(std::shared_ptr<Statement> stmt, const QueryPlan& plan);
    ExecutionResult ExecAggregate(std::shared_ptr<Statement> stmt);
    ExecutionResult ExecJoin(std::shared_ptr<Statement> stmt);

    std::string     SerializeRow(const TableSchema& schema,
                                 const std::vector<std::string>& values);
    Row             DeserializeRow(const TableSchema& schema,
                                   const char* data, uint16_t length);
    bool            MatchesConditions(const Row& row,
                                      const TableSchema& schema,
                                      const std::vector<Condition>& conditions);
    Result          ScanTable(TableSchema* schema);
    Result          PKScan(TableSchema* schema,
                           const std::string& pk_col,
                           const std::string& pk_value);

    // Subquery helper — collect all values from first column of result
    std::set<std::string> ExecuteSubquery(std::shared_ptr<Statement> subq);

    // Foreign key validation
    bool            ValidateForeignKey(const std::string& ref_table,
                                       const std::string& ref_col,
                                       const std::string& value);
};

} // namespace FarhanDB
