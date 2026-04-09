#include "query/executor.h"
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <iomanip>
#include <climits>
#include <cfloat>

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
            case StatementType::SELECT:
                // ✅ Route to aggregate if function present
                if (!stmt->aggregate_func.empty())
                    return ExecAggregate(stmt);
                return ExecSelect(stmt);
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

    page_id_t pid;
    Page* p = bpm_->NewPage(pid);
    if (!p) return {false, "Could not allocate page for table.", {}, {}};
    schema.root_page_id = pid;
    schema.page_ids.push_back(pid);
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

    for (auto pid : schema->page_ids) {
        Page* page = bpm_->FetchPage(pid);
        if (!page) continue;

        slot_id_t slot = page->InsertRecord(data.c_str(), (uint16_t)data.size());
        if (slot != UINT16_MAX) {
            bpm_->UnpinPage(pid, true);
            catalog_->Save();
            return {true, "1 row inserted.", {}, {}};
        }
        bpm_->UnpinPage(pid, false);
    }

    page_id_t new_pid;
    Page* new_page = bpm_->NewPage(new_pid);
    if (!new_page) return {false, "Out of space.", {}, {}};

    slot_id_t slot = new_page->InsertRecord(data.c_str(), (uint16_t)data.size());
    bpm_->UnpinPage(new_pid, true);

    if (slot == UINT16_MAX)
        return {false, "Record too large for page.", {}, {}};

    schema->page_ids.push_back(new_pid);
    catalog_->Save();

    return {true, "1 row inserted.", {}, {}};
}

ExecutionResult Executor::ExecSelect(std::shared_ptr<Statement> stmt) {
    TableSchema* schema = catalog_->GetTable(stmt->table_name);
    if (!schema)
        return {false, "Table '" + stmt->table_name + "' does not exist.", {}, {}};

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
    for (auto pid : schema->page_ids) {
        Page* page = bpm_->FetchPage(pid);
        if (!page) continue;

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
    }

    return {true, std::to_string(rows.size()) + " row(s) returned.", rows, col_names};
}

// ✅ NEW — Aggregate function executor
ExecutionResult Executor::ExecAggregate(std::shared_ptr<Statement> stmt) {
    TableSchema* schema = catalog_->GetTable(stmt->table_name);
    if (!schema)
        return {false, "Table '" + stmt->table_name + "' does not exist.", {}, {}};

    const std::string& func = stmt->aggregate_func;
    const std::string& col  = stmt->aggregate_col;

    // Find column index for non-COUNT functions
    int col_idx = -1;
    if (col != "*") {
        for (size_t i = 0; i < schema->columns.size(); i++) {
            if (schema->columns[i].name == col) { col_idx = i; break; }
        }
        if (col_idx < 0)
            return {false, "Column '" + col + "' not found.", {}, {}};
    }

    int     count  = 0;
    double  sum    = 0.0;
    double  maxval = -DBL_MAX;
    double  minval =  DBL_MAX;

    // Scan all pages
    for (auto pid : schema->page_ids) {
        Page* page = bpm_->FetchPage(pid);
        if (!page) continue;

        uint16_t slot_count = *reinterpret_cast<uint16_t*>(page->GetData() + 6);
        for (uint16_t s = 0; s < slot_count; s++) {
            char buf[PAGE_SIZE];
            uint16_t len = 0;
            if (page->GetRecord(s, buf, len)) {
                Row row = DeserializeRow(*schema, buf, len);
                if (!MatchesConditions(row, *schema, stmt->conditions)) continue;

                count++;
                if (col_idx >= 0 && col_idx < (int)row.size()) {
                    double val = std::stod(row[col_idx]);
                    sum    += val;
                    maxval  = std::max(maxval, val);
                    minval  = std::min(minval, val);
                }
            }
        }
        bpm_->UnpinPage(pid, false);
    }

    // Calculate result
    std::string result_label;
    std::string result_value;

    if (func == "COUNT") {
        result_label = "COUNT(*)";
        result_value = std::to_string(count);
    } else if (func == "SUM") {
        result_label = "SUM(" + col + ")";
        result_value = std::to_string((long long)sum);
    } else if (func == "AVG") {
        result_label = "AVG(" + col + ")";
        result_value = count > 0
            ? std::to_string(sum / count).substr(0, 6)
            : "0";
    } else if (func == "MAX") {
        result_label = "MAX(" + col + ")";
        result_value = count > 0 ? std::to_string((long long)maxval) : "NULL";
    } else if (func == "MIN") {
        result_label = "MIN(" + col + ")";
        result_value = count > 0 ? std::to_string((long long)minval) : "NULL";
    } else {
        return {false, "Unknown aggregate function: " + func, {}, {}};
    }

    return {true, "1 row returned.",
            {{result_value}},
            {result_label}};
}

ExecutionResult Executor::ExecDelete(std::shared_ptr<Statement> stmt) {
    TableSchema* schema = catalog_->GetTable(stmt->table_name);
    if (!schema)
        return {false, "Table '" + stmt->table_name + "' does not exist.", {}, {}};

    int deleted = 0;
    for (auto pid : schema->page_ids) {
        Page* page = bpm_->FetchPage(pid);
        if (!page) continue;

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
    }

    return {true, std::to_string(deleted) + " row(s) deleted.", {}, {}};
}

ExecutionResult Executor::ExecUpdate(std::shared_ptr<Statement> stmt) {
    TableSchema* schema = catalog_->GetTable(stmt->table_name);
    if (!schema)
        return {false, "Table '" + stmt->table_name + "' does not exist.", {}, {}};

    int set_col_idx = -1;
    for (size_t i = 0; i < schema->columns.size(); i++) {
        if (schema->columns[i].name == stmt->set_column) {
            set_col_idx = i; break;
        }
    }
    if (set_col_idx < 0)
        return {false, "Column '" + stmt->set_column + "' not found.", {}, {}};

    int updated = 0;
    for (auto pid : schema->page_ids) {
        Page* page = bpm_->FetchPage(pid);
        if (!page) continue;

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
    }

    return {true, std::to_string(updated) + " row(s) updated.", {}, {}};
}

} // namespace FarhanDB
