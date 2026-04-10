#include "bridge/DBBridge.h"
#include <fstream>
#include <chrono>
#include <algorithm>
#include <sstream>

namespace FarhanDB {

DBBridge::DBBridge() {
    InitDatabase("farhandb");
}

DBBridge::~DBBridge() {
    if (bpm_) bpm_->FlushAllPages();
    if (wal_) wal_->Flush();
}

void DBBridge::InitDatabase(const std::string& display_name) {
    if (bpm_) bpm_->FlushAllPages();
    if (wal_) wal_->Flush();

    current_db_display_ = display_name;
    std::string file_name = display_name;
    std::replace(file_name.begin(), file_name.end(), ' ', '_');
    current_db_ = file_name;

    disk_manager_ = std::make_unique<DiskManager>(file_name + ".db");
    bpm_          = std::make_unique<BufferPoolManager>(256, disk_manager_.get());
    wal_          = std::make_unique<WAL>(file_name + ".wal");
    lock_mgr_     = std::make_unique<LockManager>();
    txn_mgr_      = std::make_unique<TransactionManager>(wal_.get());
    catalog_      = std::make_unique<Catalog>(file_name + ".catalog");
    executor_     = std::make_unique<Executor>(
                        bpm_.get(), catalog_.get(),
                        txn_mgr_.get(), lock_mgr_.get());
}

QueryResult DBBridge::Execute(const std::string& sql) {
    QueryResult result;

    // Handle database commands
    std::string trimmed = sql;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r;") + 1);

    std::string upper = trimmed;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    if (upper.substr(0, 15) == "CREATE DATABASE") {
        std::string name = trimmed.substr(15);
        name.erase(0, name.find_first_not_of(" \t"));
        name.erase(name.find_last_not_of(" \t") + 1);
        CreateDatabase(name);
        result.success = true;
        result.message = "Database '" + name + "' created and selected.";
        return result;
    }

    if (upper.substr(0, 3) == "USE") {
        std::string name = trimmed.substr(3);
        name.erase(0, name.find_first_not_of(" \t"));
        name.erase(name.find_last_not_of(" \t") + 1);
        UseDatabase(name);
        result.success = true;
        result.message = "Switched to database '" + name + "'.";
        return result;
    }

    if (upper.find("SHOW DATABASES") != std::string::npos) {
        auto dbs = ListDatabases();
        result.success      = true;
        result.message      = std::to_string(dbs.size()) + " database(s)";
        result.column_names = {"Database", "Status"};
        for (auto& db : dbs) {
            result.rows.push_back({db, db == current_db_ ? "current" : ""});
        }
        return result;
    }

    if (upper.substr(0, 13) == "DROP DATABASE") {
        std::string name = trimmed.substr(13);
        name.erase(0, name.find_first_not_of(" \t"));
        name.erase(name.find_last_not_of(" \t") + 1);
        std::string file_name = name;
        std::replace(file_name.begin(), file_name.end(), ' ', '_');
        if (file_name == current_db_) {
            result.success = false;
            result.message = "Cannot drop current database!";
            return result;
        }
        DropDatabase(name);
        result.success = true;
        result.message = "Database '" + name + "' dropped.";
        return result;
    }

    // Time the query
    auto start = std::chrono::high_resolution_clock::now();

    try {
        Lexer  lexer(sql);
        auto   tokens = lexer.Tokenize();
        Parser parser(tokens);
        auto   stmt   = parser.Parse();
        auto   res    = executor_->Execute(stmt);

        auto end = std::chrono::high_resolution_clock::now();
        result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

        result.success      = res.success;
        result.message      = res.message;
        result.column_names = res.column_names;
        result.rows         = res.rows;

    } catch (const std::exception& e) {
        result.success = false;
        result.message = std::string("Error: ") + e.what();
    }

    return result;
}

bool DBBridge::CreateDatabase(const std::string& name) {
    InitDatabase(name);
    return true;
}

bool DBBridge::UseDatabase(const std::string& name) {
    InitDatabase(name);
    return true;
}

bool DBBridge::DropDatabase(const std::string& name) {
    std::string file_name = name;
    std::replace(file_name.begin(), file_name.end(), ' ', '_');
    try {
        std::filesystem::remove(file_name + ".db");
        std::filesystem::remove(file_name + ".wal");
        std::filesystem::remove(file_name + ".catalog");
        return true;
    } catch (...) { return false; }
}

std::vector<std::string> DBBridge::ListDatabases() {
    std::vector<std::string> dbs;
    try {
        for (auto& entry : std::filesystem::directory_iterator(".")) {
            std::string fname = entry.path().filename().string();
            if (fname.size() > 8 && fname.substr(fname.size()-8) == ".catalog")
                dbs.push_back(fname.substr(0, fname.size()-8));
        }
    } catch (...) {}
    std::sort(dbs.begin(), dbs.end());
    return dbs;
}

std::vector<std::string> DBBridge::ListTables() {
    std::vector<std::string> tables;
    if (!catalog_) return tables;
    auto result = Execute("SELECT * FROM sqlite_master;");
    // Just scan catalog directly
    // We'll return via a simple approach
    return tables;
}

std::vector<std::string> DBBridge::ListColumns(const std::string& table) {
    std::vector<std::string> cols;
    if (!catalog_) return cols;
    auto* schema = catalog_->GetTable(table);
    if (!schema) return cols;
    for (auto& col : schema->columns)
        cols.push_back(col.name);
    return cols;
}

} // namespace FarhanDB
