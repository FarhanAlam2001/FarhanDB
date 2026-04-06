#include "query/executor.h"
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <iomanip>

namespace FarhanDB {

Executor::Executor(BufferPoolManager* bpm, Catalog* catalog,
                   TransactionManager* txn_mgr, LockManager* lock_mgr)
    : bpm_(bpm), catalog_(catalog),
      txn_mgr_(txn_mgr), lock_mgr_(lock_mgr),
      current_txn_id_(INVALID_TXN_ID) {}

ExecutionResult Executor::Execute(std::shared_ptr<Statement> stmt) {
    try {
        switch (stmt->type) {
            case StatementType::CREATE_TABLE:  return ExecCreateTable(stmt);
            case StatementType::DROP_TABLE:    return ExecDropTable(stmt);
            case StatementType::INSERT:        return ExecInsert(stmt);
            case StatementType::SELECT:        return ExecSelect(stmt);
            case StatementType::DELETE:        return ExecDelete(stmt);
            case StatementType::UPDATE:        return ExecUpdate(stmt);
            case StatementType::BEGIN_TXN: {
                auto* txn = txn_mgr_->Begin();
                current_txn_id_ = txn->id;
                return {true, "Transaction started (id=" +
                        std::to_string(txn->id) + ")", {}, {}};
            }
            case StatementType::COMMIT_TXN: {
                txn_mgr_->Commit(current_txn_id_);
                lock_mgr_->ReleaseAllLocks(current_txn_id_);
                current_txn_id_ = INVALID_TXN_ID;
                return {true, "Transaction committed.", {}, {}};
            }
            case StatementType::ROLLBACK_TXN: {
                txn_mgr_->Abort(current_txn_id_);
                lock_mgr_->ReleaseAllLocks(current_txn_id_);
                current_txn_id_ = INVALID_TXN_ID;
                return {true, "Transaction rolled back.", {}, {}};
            }
            default:
                return {false, "Unknown statement type.", {}, {}};
        }
    } catch (const std::exception& e) {
        return {false, std::string("Error: ") + e.what(), {}, {}};
    }
}

ExecutionResult Executor::ExecCreateTable(std::shared_ptr<Statement> stmt) {
    if (catalog_->TableExists(stmt->table_name))
        return {false, "Table '" + stmt->table_name + "' already exists.", {}, {}};

    TableSchema schema;
    schema.table_name = stmt->table_name;
    schema.root_page_id = INVALID_PAGE_ID;

    for (auto& cd : stmt->column_defs) {
        Column col;
        col.name = cd.name;
        col.type = (cd.type == "INT") ? DataType::INT : DataType::VARCHAR;
        col.size = cd.size;
        col.is_primary_key = cd.is_primary_key;
        schema.columns.push_back(col);
    }

    // Allocate first data page
    page_id_t pid;
    Page* p = bpm_->NewPage(pid);
    if (!p) return {false, "Could not allocate page for table.", {}, {}};
    schema.root_page_id = pid;
    bpm_->UnpinPage(pid, true);

    catalog_->CreateTable(schema);
    return {true, "Table '" + stmt->table_name + "' created.", {}, {}};
}

ExecutionResult Executor::ExecDropTable(std::shared_ptr<Statement> stmt) {
    if (!catalog_->TableExists(stmt->table_name))
        return {false, "Table '" + stmt->table_name + "' does not exist.", {}, {}};
    catalog_->DropTable(stmt->table_name);
    return {true, "Table '" + stmt->table_name + "' dropped.", {}, {}};
}

std::string Executor::SerializeRow(const TableSchema& schema,
                                    const std::vector<std::string>& values) {
    std::string data;
    for (size_t i = 0; i < schema.columns.size() && i < values.size(); i++) {
        const auto& col = schema.columns[i];
        if (col.type == DataType::INT) {
            int32_t val = std::stoi(values[i]);
            data.append(reinterpret_cast<char*>(&val), sizeof(int32_t));
        } else {
            // VARCHAR: fixed size with null padding
            std::string s = values[i];
            s.resize(col.size, '\0');
            data.append(s);
        }
    }
    return data;
}

Row Executor::DeserializeRow(const TableSchema& schema,
                              const char* data, uint16_t length) {
    Row row;
    size_t offset = 0;
    for (const auto& col : schema.columns) {
        if (offset >= length) { row.push_back(""); continue; }
        if (col.type == DataType::INT) {
            int32_t val = 0;
            std::memcpy(&val, data + offset, sizeof(int32_t));
            row.push_back(std::to_string(val));
            offset += sizeof(int32_t);
        } else {
            std::string s(data + offset, col.size);
            // trim nulls
            s.erase(std::find(s.begin(), s.end(), '\0'), s.end());
            row.push_back(s);
            offset += col.size;
        }
    }
    return row;
}

bool Executor::MatchesConditions(const Row& row, const TableSchema& schema,
                                  const std::vector<Condition>& conditions) {
    for (const auto& cond : conditions) {
        // Find column index
        int col_idx = -1;
        for (size_t i = 0; i < schema.columns.size(); i++) {
            if (schema.columns[i].name == cond.column) { col_idx = i; break; }
        }
        if (col_idx < 0 || col_idx >= (int)row.size()) return false;

        const std::string& cell = row[col_idx];
        const std::string& val  = cond.value;

        bool match = false;
        if (cond.op == "=")       match = (cell == val);
        else if (cond.op == "!=") match = (cell != val);
        else if (cond.op == ">")  match = (std::stod(cell) > std::stod(val));
        else if (cond.op == "<")  match = (std::stod(cell) < std::stod(val));
        else if (cond.op == ">=") match = (std::stod(cell) >= std::stod(val));
        else if (cond.op == "<=") match = (std::stod(cell) <= std::stod(val));

        if (!match) return false;
    }
    return true;
}

ExecutionResult Executor::ExecInsert(std::shared_ptr<Statement> stmt) {
    TableSchema* schema = catalog_->GetTable(stmt->table_name);
    if (!schema)
        return {false, "Table '" + stmt->table_name + "' does not exist.", {}, {}};

    if (stmt->values.size() != schema->columns.size())
        return {false, "Column count mismatch.", {}, {}};

    std::string data = SerializeRow(*schema, stmt->values);

    // Find a page with free space
    page_id_t pid = schema->root_page_id;
    Page* page = bpm_->FetchPage(pid);
    if (!page) return {false, "Cannot fetch page.", {}, {}};

    slot_id_t slot = page->InsertRecord(data.c_str(), (uint16_t)data.size());
    if (slot == UINT16_MAX) {
        // Page full — allocate new page
        bpm_->UnpinPage(pid, false);
        page = bpm_->NewPage(pid);
        if (!page) return {false, "Out of space.", {}, {}};
        slot = page->InsertRecord(data.c_str(), (uint16_t)data.size());
    }

    bpm_->UnpinPage(pid, true);
    return {true, "1 row inserted.", {}, {}};
}

ExecutionResult Executor::ExecSelect(std::shared_ptr<Statement> stmt) {
    TableSchema* schema = catalog_->GetTable(stmt->table_name);
    if (!schema)
        return {false, "Table '" + stmt->table_name + "' does not exist.", {}, {}};

    // Determine which columns to show
    std::vector<std::string> col_names;
    std::vector<int> col_indices;
    bool select_all = (stmt->columns.size() == 1 && stmt->columns[0] == "*");

    if (select_all) {
        for (size_t i = 0; i < schema->columns.size(); i++) {
            col_names.push_back(schema->columns[i].name);
            col_indices.push_back(i);
        }
    } else {
        for (const auto& cname : stmt->columns) {
            for (size_t i = 0; i < schema->columns.size(); i++) {
                if (schema->columns[i].name == cname) {
                    col_names.push_back(cname);
                    col_indices.push_back(i);
                }
            }
        }
    }

    Result rows;
    // Scan all pages starting from root
    page_id_t pid = schema->root_page_id;
    while (pid != INVALID_PAGE_ID) {
        Page* page = bpm_->FetchPage(pid);
        if (!page) break;

        uint16_t slot_count = *reinterpret_cast<uint16_t*>(page->GetData() + 6);
        for (uint16_t s = 0; s < slot_count; s++) {
            char buf[PAGE_SIZE];
            uint16_t len = 0;
            if (page->GetRecord(s, buf, len)) {
                Row full_row = DeserializeRow(*schema, buf, len);
                if (MatchesConditions(full_row, *schema, stmt->conditions)) {
                    Row selected;
                    for (int idx : col_indices)
                        selected.push_back(full_row[idx]);
                    rows.push_back(selected);
                }
            }
        }
        bpm_->UnpinPage(pid, false);
        break; // single page scan for now
    }

    return {true, std::to_string(rows.size()) + " row(s) returned.", rows, col_names};
}

ExecutionResult Executor::ExecDelete(std::shared_ptr<Statement> stmt) {
    TableSchema* schema = catalog_->GetTable(stmt->table_name);
    if (!schema)
        return {false, "Table '" + stmt->table_name + "' does not exist.", {}, {}};

    page_id_t pid = schema->root_page_id;
    Page* page = bpm_->FetchPage(pid);
    if (!page) return {false, "Cannot fetch page.", {}, {}};

    int deleted = 0;
    uint16_t slot_count = *reinterpret_cast<uint16_t*>(page->GetData() + 6);
    for (uint16_t s = 0; s < slot_count; s++) {
        char buf[PAGE_SIZE];
        uint16_t len = 0;
        if (page->GetRecord(s, buf, len)) {
            Row row = DeserializeRow(*schema, buf, len);
            if (MatchesConditions(row, *schema, stmt->conditions)) {
                page->DeleteRecord(s);
                deleted++;
            }
        }
    }
    bpm_->UnpinPage(pid, true);
    return {true, std::to_string(deleted) + " row(s) deleted.", {}, {}};
}

ExecutionResult Executor::ExecUpdate(std::shared_ptr<Statement> stmt) {
    TableSchema* schema = catalog_->GetTable(stmt->table_name);
    if (!schema)
        return {false, "Table '" + stmt->table_name + "' does not exist.", {}, {}};

    // Find SET column index
    int set_col_idx = -1;
    for (size_t i = 0; i < schema->columns.size(); i++) {
        if (schema->columns[i].name == stmt->set_column) {
            set_col_idx = i; break;
        }
    }
    if (set_col_idx < 0)
        return {false, "Column '" + stmt->set_column + "' not found.", {}, {}};

    page_id_t pid = schema->root_page_id;
    Page* page = bpm_->FetchPage(pid);
    if (!page) return {false, "Cannot fetch page.", {}, {}};

    int updated = 0;
    uint16_t slot_count = *reinterpret_cast<uint16_t*>(page->GetData() + 6);
    for (uint16_t s = 0; s < slot_count; s++) {
        char buf[PAGE_SIZE];
        uint16_t len = 0;
        if (page->GetRecord(s, buf, len)) {
            Row row = DeserializeRow(*schema, buf, len);
            if (MatchesConditions(row, *schema, stmt->conditions)) {
                row[set_col_idx] = stmt->set_value;
                page->DeleteRecord(s);
                std::string new_data = SerializeRow(*schema, row);
                page->InsertRecord(new_data.c_str(), (uint16_t)new_data.size());
                updated++;
            }
        }
    }
    bpm_->UnpinPage(pid, true);
    return {true, std::to_string(updated) + " row(s) updated.", {}, {}};
}

} // namespace FarhanDB
