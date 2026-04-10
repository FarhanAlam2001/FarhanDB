#pragma once
#include <string>
#include <vector>
#include <memory>
#include <filesystem>

#include "storage/disk_manager.h"
#include "storage/buffer_pool.h"
#include "storage/wal.h"
#include "query/lexer.h"
#include "query/parser.h"
#include "query/catalog.h"
#include "query/executor.h"
#include "transaction/lock_manager.h"
#include "transaction/transaction_manager.h"

namespace FarhanDB {

struct QueryResult {
    bool                            success;
    std::string                     message;
    std::vector<std::string>        column_names;
    std::vector<std::vector<std::string>> rows;
    double                          elapsed_ms = 0.0;
};

class DBBridge {
public:
    DBBridge();
    ~DBBridge();

    // Execute any SQL query
    QueryResult         Execute(const std::string& sql);

    // Database management
    bool                CreateDatabase(const std::string& name);
    bool                UseDatabase(const std::string& name);
    bool                DropDatabase(const std::string& name);
    std::vector<std::string> ListDatabases();
    std::string         CurrentDatabase() const { return current_db_display_; }

    // Schema info for sidebar
    std::vector<std::string> ListTables();
    std::vector<std::string> ListColumns(const std::string& table);

private:
    std::unique_ptr<DiskManager>        disk_manager_;
    std::unique_ptr<BufferPoolManager>  bpm_;
    std::unique_ptr<WAL>                wal_;
    std::unique_ptr<LockManager>        lock_mgr_;
    std::unique_ptr<TransactionManager> txn_mgr_;
    std::unique_ptr<Catalog>            catalog_;
    std::unique_ptr<Executor>           executor_;

    std::string current_db_;
    std::string current_db_display_;

    void InitDatabase(const std::string& display_name);
};

} // namespace FarhanDB
