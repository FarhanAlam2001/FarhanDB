#pragma once
#include "query/parser.h"
#include "query/catalog.h"
#include "storage/buffer_pool.h"
#include "transaction/transaction_manager.h"
#include "transaction/lock_manager.h"
#include <vector>
#include <string>

namespace FarhanDB {

using Row    = std::vector<std::string>;
using Result = std::vector<Row>;

struct ExecutionResult {
    bool        success;
    std::string message;
    Result      rows;
    std::vector<std::string> column_names;
};

class Executor {
public:
    Executor(BufferPoolManager* bpm,
             Catalog*           catalog,
             TransactionManager* txn_mgr,
             LockManager*       lock_mgr);

    ExecutionResult Execute(std::shared_ptr<Statement> stmt);

private:
    BufferPoolManager*  bpm_;
    Catalog*            catalog_;
    TransactionManager* txn_mgr_;
    LockManager*        lock_mgr_;
    txn_id_t            current_txn_id_;

    ExecutionResult ExecCreateTable(std::shared_ptr<Statement> stmt);
    ExecutionResult ExecDropTable(std::shared_ptr<Statement> stmt);
    ExecutionResult ExecInsert(std::shared_ptr<Statement> stmt);
    ExecutionResult ExecSelect(std::shared_ptr<Statement> stmt);
    ExecutionResult ExecDelete(std::shared_ptr<Statement> stmt);
    ExecutionResult ExecUpdate(std::shared_ptr<Statement> stmt);
    ExecutionResult ExecAggregate(std::shared_ptr<Statement> stmt);
    ExecutionResult ExecJoin(std::shared_ptr<Statement> stmt); // ✅ NEW

    std::string     SerializeRow(const TableSchema& schema,
                                 const std::vector<std::string>& values);
    Row             DeserializeRow(const TableSchema& schema,
                                   const char* data, uint16_t length);
    bool            MatchesConditions(const Row& row,
                                      const TableSchema& schema,
                                      const std::vector<Condition>& conditions);

    // Helper to scan all rows from a table
    Result          ScanTable(TableSchema* schema);
};

} // namespace FarhanDB
