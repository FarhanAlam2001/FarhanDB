#include "query/executor.h"
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <iomanip>
#include <climits>
#include <cfloat>
#include <unordered_map>
#include <functional>
#include <set>

namespace FarhanDB {

Executor::Executor(BufferPoolManager* bpm, Catalog* catalog,
                   TransactionManager* txn_mgr, LockManager* lock_mgr)
    : bpm_(bpm), catalog_(catalog),
      txn_mgr_(txn_mgr), lock_mgr_(lock_mgr),
      optimizer_(catalog),
      current_txn_id_(INVALID_TXN_ID) {}

std::string Executor::Explain(std::shared_ptr<Statement> stmt) {
    QueryPlan plan = optimizer_.Optimize(stmt);
    return optimizer_.ExplainPlan(plan);
}

ExecutionResult Executor::Execute(std::shared_ptr<Statement> stmt) {
    try {
        switch (stmt->type) {
            case StatementType::CREATE_TABLE: return ExecCreateTable(stmt);
            case StatementType::CREATE_INDEX: return ExecCreateIndex(stmt);
            case StatementType::ALTER_TABLE:  return ExecAlterTable(stmt);
            case StatementType::DROP_TABLE:   return ExecDropTable(stmt);
            case StatementType::INSERT:       return ExecInsert(stmt);
            case StatementType::SELECT: {
                if (!stmt->aggregate_func.empty()) return ExecAggregate(stmt);
                if (!stmt->join_table.empty())     return ExecJoin(stmt);
                QueryPlan plan = optimizer_.Optimize(stmt);
                auto result = ExecSelect(stmt, plan);
                result.query_plan = optimizer_.ExplainPlan(plan);
                return result;
            }
            case StatementType::DELETE: {
                QueryPlan plan = optimizer_.Optimize(stmt);
                auto result = ExecDelete(stmt, plan);
                result.query_plan = optimizer_.ExplainPlan(plan);
                return result;
            }
            case StatementType::UPDATE: {
                QueryPlan plan = optimizer_.Optimize(stmt);
                auto result = ExecUpdate(stmt, plan);
                result.query_plan = optimizer_.ExplainPlan(plan);
                return result;
            }
            case StatementType::BEGIN_TXN: {
                auto* txn = txn_mgr_->Begin();
                current_txn_id_ = txn->id;
                return {true, "Transaction started (id=" +
                        std::to_string(txn->id) + ")", {}, {}, ""};
            }
            case StatementType::COMMIT_TXN: {
                txn_mgr_->Commit(current_txn_id_);
                lock_mgr_->ReleaseAllLocks(current_txn_id_);
                current_txn_id_ = INVALID_TXN_ID;
                return {true, "Transaction committed.", {}, {}, ""};
            }
            case StatementType::ROLLBACK_TXN: {
                txn_mgr_->Abort(current_txn_id_);
                lock_mgr_->ReleaseAllLocks(current_txn_id_);
                current_txn_id_ = INVALID_TXN_ID;
                return {true, "Transaction rolled back.", {}, {}, ""};
            }
            default:
                return {false, "Unknown statement type.", {}, {}, ""};
        }
    } catch (const std::exception& e) {
        return {false, std::string("Error: ") + e.what(), {}, {}, ""};
    }
}

// ─── CREATE TABLE ─────────────────────────────────────────────────────────────
ExecutionResult Executor::ExecCreateTable(std::shared_ptr<Statement> stmt) {
    if (catalog_->TableExists(stmt->table_name))
        return {false, "Table '" + stmt->table_name + "' already exists.", {}, {}, ""};

    TableSchema schema;
    schema.table_name   = stmt->table_name;
    schema.root_page_id = INVALID_PAGE_ID;

    for (auto& cd : stmt->column_defs) {
        Column col;
        col.name           = cd.name;
        col.type           = (cd.type == "INT") ? DataType::INT : DataType::VARCHAR;
        col.size           = cd.size;
        col.is_primary_key = cd.is_primary_key;
        col.not_null       = cd.not_null;
        col.has_default    = cd.has_default;
        col.default_value  = cd.default_value;
        col.is_foreign_key = cd.is_foreign_key;
        col.fk_ref_table   = cd.fk_ref_table;
        col.fk_ref_column  = cd.fk_ref_column;
        schema.columns.push_back(col);
    }

    page_id_t pid;
    Page* p = bpm_->NewPage(pid);
    if (!p) return {false, "Could not allocate page for table.", {}, {}, ""};
    schema.root_page_id = pid;
    schema.page_ids.push_back(pid);
    bpm_->UnpinPage(pid, true);

    catalog_->CreateTable(schema);
    return {true, "Table '" + stmt->table_name + "' created.", {}, {}, ""};
}

// ─── DROP TABLE ───────────────────────────────────────────────────────────────
ExecutionResult Executor::ExecDropTable(std::shared_ptr<Statement> stmt) {
    if (!catalog_->TableExists(stmt->table_name))
        return {false, "Table '" + stmt->table_name + "' does not exist.", {}, {}, ""};
    catalog_->DropTable(stmt->table_name);
    return {true, "Table '" + stmt->table_name + "' dropped.", {}, {}, ""};
}

// ─── CREATE INDEX — now actually builds the B+ Tree ──────────────────────────
ExecutionResult Executor::ExecCreateIndex(std::shared_ptr<Statement> stmt) {
    if (!catalog_->TableExists(stmt->table_name))
        return {false, "Table '" + stmt->table_name + "' does not exist.", {}, {}, ""};

    TableSchema* schema = catalog_->GetTable(stmt->table_name);

    // Find column and validate it is INT (BTree uses int32_t keys)
    int col_idx = -1;
    for (size_t i = 0; i < schema->columns.size(); i++) {
        if (schema->columns[i].name == stmt->index_col) { col_idx = i; break; }
    }
    if (col_idx < 0)
        return {false, "Column '" + stmt->index_col + "' not found.", {}, {}, ""};
    if (schema->columns[col_idx].type != DataType::INT)
        return {false, "Index currently supported on INT columns only.", {}, {}, ""};

    // Build the BTree and populate it from existing rows
    std::string tree_key = stmt->table_name + "." + stmt->index_col;
    auto tree = std::make_unique<BTree>(tree_key, bpm_); // fresh root

    int rows_indexed = 0;
    for (auto pid : schema->page_ids) {
        Page* page = bpm_->FetchPage(pid);
        if (!page) continue;
        uint16_t slot_count = *reinterpret_cast<uint16_t*>(page->GetData() + 6);
        for (uint16_t s = 0; s < slot_count; s++) {
            char buf[PAGE_SIZE];
            uint16_t len = 0;
            if (page->GetRecord(s, buf, len)) {
                Row row = DeserializeRow(*schema, buf, len);
                if (col_idx < (int)row.size()) {
                    try {
                        int32_t key = std::stoi(row[col_idx]);
                        RID rid     = {pid, s};
                        tree->Insert(key, rid);
                        rows_indexed++;
                    } catch (...) {}
                }
            }
        }
        bpm_->UnpinPage(pid, false);
    }

    // Persist root_page_id in IndexInfo so optimizer and executor can find it
    IndexInfo idx;
    idx.index_name   = stmt->index_name;
    idx.table_name   = stmt->table_name;
    idx.column_name  = stmt->index_col;
    idx.root_page_id = tree->GetRootPageId();

    index_trees_[tree_key] = std::move(tree);
    catalog_->CreateIndex(idx);
    catalog_->Save();

    return {true, "Index '" + stmt->index_name + "' created on " +
            stmt->table_name + "(" + stmt->index_col + ") — " +
            std::to_string(rows_indexed) + " rows indexed.", {}, {}, ""};
}

// ─── Serialize / Deserialize ─────────────────────────────────────────────────
std::string Executor::SerializeRow(const TableSchema& schema,
                                    const std::vector<std::string>& values) {
    std::string data;
    for (size_t i = 0; i < schema.columns.size() && i < values.size(); i++) {
        const auto& col = schema.columns[i];
        if (col.type == DataType::INT) {
            int32_t val = 0;
            try { val = std::stoi(values[i]); } catch (...) {}
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

// ─── Condition matching ───────────────────────────────────────────────────────
bool Executor::MatchesConditions(const Row& row, const TableSchema& schema,
                                  const std::vector<Condition>& conditions) {
    if (conditions.empty()) return true;

    bool result = true;
    bool first  = true;

    for (size_t i = 0; i < conditions.size(); i++) {
        const auto& cond = conditions[i];
        int col_idx = -1;
        for (size_t j = 0; j < schema.columns.size(); j++)
            if (schema.columns[j].name == cond.column) { col_idx = j; break; }

        bool match = false;
        if (col_idx >= 0 && col_idx < (int)row.size()) {
            const std::string& cell = row[col_idx];
            const std::string& val  = cond.value;

            if (cond.op == "=")        match = (cell == val);
            else if (cond.op == "!=")  match = (cell != val);
            else if (cond.op == ">")   { try { match = std::stod(cell) > std::stod(val);  } catch(...){} }
            else if (cond.op == "<")   { try { match = std::stod(cell) < std::stod(val);  } catch(...){} }
            else if (cond.op == ">=")  { try { match = std::stod(cell) >= std::stod(val); } catch(...){} }
            else if (cond.op == "<=")  { try { match = std::stod(cell) <= std::stod(val); } catch(...){} }
            else if (cond.op == "BETWEEN") {
                auto comma = val.find(',');
                if (comma != std::string::npos) {
                    try {
                        double v    = std::stod(cell);
                        double low  = std::stod(val.substr(0, comma));
                        double high = std::stod(val.substr(comma + 1));
                        match = (v >= low && v <= high);
                    } catch (...) {
                        std::string low  = val.substr(0, comma);
                        std::string high = val.substr(comma + 1);
                        match = (cell >= low && cell <= high);
                    }
                }
            }
            else if (cond.op == "LIKE") {
                std::function<bool(size_t, size_t)> like_match;
                like_match = [&](size_t ti, size_t pi) -> bool {
                    if (pi == val.size()) return ti == cell.size();
                    if (val[pi] == '%') {
                        for (size_t k = ti; k <= cell.size(); k++)
                            if (like_match(k, pi + 1)) return true;
                        return false;
                    }
                    if (ti == cell.size()) return false;
                    if (val[pi] == '_' || val[pi] == cell[ti])
                        return like_match(ti + 1, pi + 1);
                    return false;
                };
                match = like_match(0, 0);
            }
        }

        if (first) { result = match; first = false; }
        else if (i > 0 && conditions[i-1].connector == "OR")
            result = result || match;
        else
            result = result && match;
    }
    return result;
}

// ─── Table scan helpers ───────────────────────────────────────────────────────
Result Executor::ScanTable(TableSchema* schema) {
    Result rows;
    for (auto pid : schema->page_ids) {
        Page* page = bpm_->FetchPage(pid);
        if (!page) continue;
        uint16_t slot_count = *reinterpret_cast<uint16_t*>(page->GetData() + 6);
        for (uint16_t s = 0; s < slot_count; s++) {
            char buf[PAGE_SIZE];
            uint16_t len = 0;
            if (page->GetRecord(s, buf, len))
                rows.push_back(DeserializeRow(*schema, buf, len));
        }
        bpm_->UnpinPage(pid, false);
    }
    return rows;
}

Result Executor::PKScan(TableSchema* schema,
                         const std::string& pk_col,
                         const std::string& pk_value) {
    Result rows;
    int pk_idx = -1;
    for (size_t i = 0; i < schema->columns.size(); i++)
        if (schema->columns[i].name == pk_col) { pk_idx = i; break; }
    if (pk_idx < 0) return rows;

    for (auto pid : schema->page_ids) {
        Page* page = bpm_->FetchPage(pid);
        if (!page) continue;
        uint16_t slot_count = *reinterpret_cast<uint16_t*>(page->GetData() + 6);
        for (uint16_t s = 0; s < slot_count; s++) {
            char buf[PAGE_SIZE];
            uint16_t len = 0;
            if (page->GetRecord(s, buf, len)) {
                Row row = DeserializeRow(*schema, buf, len);
                if (pk_idx < (int)row.size() && row[pk_idx] == pk_value) {
                    rows.push_back(row);
                    bpm_->UnpinPage(pid, false);
                    return rows; // PK is unique — stop immediately
                }
            }
        }
        bpm_->UnpinPage(pid, false);
    }
    return rows;
}

// ─── INDEX HELPERS ────────────────────────────────────────────────────────────

BTree* Executor::GetOrLoadIndex(const std::string& table,
                                 const std::string& col,
                                 uint32_t root_page_id) {
    std::string key = table + "." + col;
    auto it = index_trees_.find(key);
    if (it != index_trees_.end()) return it->second.get();

    if (root_page_id == UINT32_MAX) return nullptr; // index not built yet

    // Load existing BTree from its persisted root page
    auto tree = std::make_unique<BTree>(key, bpm_, root_page_id);
    BTree* ptr = tree.get();
    index_trees_[key] = std::move(tree);
    return ptr;
}

// Use B+ Tree to fetch row matching col = value in O(log n)
Result Executor::IndexScan(TableSchema* schema,
                            const std::string& col,
                            const std::string& value,
                            uint32_t index_root_page) {
    Result rows;

    BTree* tree = GetOrLoadIndex(schema->table_name, col, index_root_page);
    if (!tree) return rows; // no index — caller falls back to full scan

    // BTree only indexes INT columns
    int32_t key;
    try { key = std::stoi(value); }
    catch (...) { return rows; }

    // RangeSearch(key, key) finds ALL rows with this value (handles duplicates)
    auto rids = tree->RangeSearch(key, key);
    for (auto& rid : rids) {
        Page* page = bpm_->FetchPage(rid.first);
        if (!page) continue;
        char buf[PAGE_SIZE];
        uint16_t len = 0;
        if (page->GetRecord(rid.second, buf, len))
            rows.push_back(DeserializeRow(*schema, buf, len));
        bpm_->UnpinPage(rid.first, false);
    }
    return rows;
}

// Called after every INSERT — keeps all B+ Tree indexes in sync
void Executor::UpdateIndexesOnInsert(TableSchema* schema,
                                      const std::vector<std::string>& values,
                                      RID rid) {
    for (auto& idx_info : schema->indexes) {
        if (idx_info.root_page_id == UINT32_MAX) continue;

        int col_idx = -1;
        for (size_t i = 0; i < schema->columns.size(); i++)
            if (schema->columns[i].name == idx_info.column_name) { col_idx = i; break; }

        if (col_idx < 0 || col_idx >= (int)values.size()) continue;
        if (schema->columns[col_idx].type != DataType::INT)   continue;

        try {
            int32_t key = std::stoi(values[col_idx]);
            BTree* tree = GetOrLoadIndex(schema->table_name,
                                         idx_info.column_name,
                                         idx_info.root_page_id);
            if (tree) {
                tree->Insert(key, rid);
                // Keep root_page_id current (root may change after split)
                idx_info.root_page_id = tree->GetRootPageId();
            }
        } catch (...) {}
    }
}

// Called after every DELETE — removes stale entries from B+ Tree indexes
void Executor::UpdateIndexesOnDelete(TableSchema* schema, const Row& row) {
    for (auto& idx_info : schema->indexes) {
        if (idx_info.root_page_id == UINT32_MAX) continue;

        int col_idx = -1;
        for (size_t i = 0; i < schema->columns.size(); i++)
            if (schema->columns[i].name == idx_info.column_name) { col_idx = i; break; }

        if (col_idx < 0 || col_idx >= (int)row.size()) continue;
        if (schema->columns[col_idx].type != DataType::INT)  continue;

        try {
            int32_t key = std::stoi(row[col_idx]);
            BTree* tree = GetOrLoadIndex(schema->table_name,
                                         idx_info.column_name,
                                         idx_info.root_page_id);
            if (tree) tree->Delete(key);
        } catch (...) {}
    }
}

// ─── INSERT ───────────────────────────────────────────────────────────────────
ExecutionResult Executor::ExecInsert(std::shared_ptr<Statement> stmt) {
    TableSchema* schema = catalog_->GetTable(stmt->table_name);
    if (!schema)
        return {false, "Table '" + stmt->table_name + "' does not exist.", {}, {}, ""};

    // Apply DEFAULT values for missing columns
    std::vector<std::string> final_values = stmt->values;
    if (final_values.size() < schema->columns.size()) {
        for (size_t i = final_values.size(); i < schema->columns.size(); i++) {
            if (schema->columns[i].has_default)
                final_values.push_back(schema->columns[i].default_value);
            else
                final_values.push_back("");
        }
    }
    if (final_values.size() != schema->columns.size())
        return {false, "Column count mismatch.", {}, {}, ""};

    // NOT NULL check
    for (size_t i = 0; i < schema->columns.size(); i++) {
        if (schema->columns[i].not_null && final_values[i].empty())
            return {false, "Column '" + schema->columns[i].name +
                    "' cannot be NULL.", {}, {}, ""};
    }

    // Foreign key check
    for (size_t i = 0; i < schema->columns.size(); i++) {
        if (schema->columns[i].is_foreign_key && !final_values[i].empty()) {
            if (!ValidateForeignKey(schema->columns[i].fk_ref_table,
                                    schema->columns[i].fk_ref_column,
                                    final_values[i]))
                return {false, "Foreign key violation: '" + final_values[i] +
                        "' not in " + schema->columns[i].fk_ref_table +
                        "(" + schema->columns[i].fk_ref_column + ")", {}, {}, ""};
        }
    }

    std::string data = SerializeRow(*schema, final_values);

    // Try existing pages first
    for (auto pid : schema->page_ids) {
        Page* page = bpm_->FetchPage(pid);
        if (!page) continue;
        slot_id_t slot = page->InsertRecord(data.c_str(), (uint16_t)data.size());
        if (slot != UINT16_MAX) {
            RID rid = {pid, slot};
            bpm_->UnpinPage(pid, true);
            UpdateIndexesOnInsert(schema, final_values, rid); // ← update indexes
            catalog_->Save();
            return {true, "1 row inserted.", {}, {}, ""};
        }
        bpm_->UnpinPage(pid, false);
    }

    // Allocate a new page
    page_id_t new_pid;
    Page* new_page = bpm_->NewPage(new_pid);
    if (!new_page) return {false, "Out of space.", {}, {}, ""};

    slot_id_t slot = new_page->InsertRecord(data.c_str(), (uint16_t)data.size());
    if (slot == UINT16_MAX) {
        bpm_->UnpinPage(new_pid, false);
        return {false, "Record too large for page.", {}, {}, ""};
    }

    RID rid = {new_pid, slot};
    bpm_->UnpinPage(new_pid, true);
    schema->page_ids.push_back(new_pid);
    UpdateIndexesOnInsert(schema, final_values, rid); // ← update indexes
    catalog_->Save();
    return {true, "1 row inserted.", {}, {}, ""};
}

// ─── SELECT ───────────────────────────────────────────────────────────────────
ExecutionResult Executor::ExecSelect(std::shared_ptr<Statement> stmt,
                                      const QueryPlan& plan) {
    TableSchema* schema = catalog_->GetTable(stmt->table_name);
    if (!schema)
        return {false, "Table '" + stmt->table_name + "' does not exist.", {}, {}, ""};

    std::vector<std::string> col_names;
    std::vector<int>         col_indices;
    bool select_all = (stmt->columns.size() == 1 && stmt->columns[0] == "*");

    if (select_all) {
        for (size_t i = 0; i < schema->columns.size(); i++) {
            col_names.push_back(schema->columns[i].name);
            col_indices.push_back((int)i);
        }
    } else {
        for (const auto& cname : stmt->columns) {
            for (size_t i = 0; i < schema->columns.size(); i++) {
                if (schema->columns[i].name == cname) {
                    col_names.push_back(cname);
                    col_indices.push_back((int)i);
                }
            }
        }
    }

    // ── Choose scan strategy based on optimizer plan ─────────────────────────
    Result all_rows;

    if (plan.type == PlanType::INDEX_SCAN) {
        // O(log n) — B+ Tree lookup
        all_rows = IndexScan(schema, plan.index_col,
                             plan.index_value, plan.index_root_page);
        // Apply any remaining WHERE conditions (e.g. extra AND clauses)
        Result filtered;
        for (auto& row : all_rows)
            if (MatchesConditions(row, *schema, stmt->conditions))
                filtered.push_back(row);
        all_rows = filtered;

    } else if (plan.type == PlanType::PRIMARY_KEY_SCAN) {
        // Sequential scan with early exit on PK match
        all_rows = PKScan(schema, plan.pk_column, plan.pk_value);

    } else {
        // Full table scan with WHERE filter
        all_rows = ScanTable(schema);
        Result filtered;
        for (auto& row : all_rows)
            if (MatchesConditions(row, *schema, stmt->conditions))
                filtered.push_back(row);
        all_rows = filtered;
    }

    // Project selected columns
    Result rows;
    for (auto& full_row : all_rows) {
        Row selected;
        for (int idx : col_indices)
            if (idx < (int)full_row.size())
                selected.push_back(full_row[idx]);
        rows.push_back(selected);
    }

    // ── Subquery IN filter ────────────────────────────────────────────────────
    if (stmt->subquery && !stmt->where_in_col.empty()) {
        std::set<std::string> allowed = ExecuteSubquery(stmt->subquery);
        int in_idx = -1;
        for (size_t i = 0; i < col_names.size(); i++)
            if (col_names[i] == stmt->where_in_col) { in_idx = (int)i; break; }
        if (in_idx >= 0) {
            Result filtered;
            for (auto& row : rows)
                if (in_idx < (int)row.size() && allowed.count(row[in_idx]))
                    filtered.push_back(row);
            rows = filtered;
        }
    }

    // ── DISTINCT ──────────────────────────────────────────────────────────────
    if (stmt->is_distinct) {
        Result unique_rows;
        for (auto& row : rows) {
            bool found = false;
            for (auto& urow : unique_rows)
                if (urow == row) { found = true; break; }
            if (!found) unique_rows.push_back(row);
        }
        rows = unique_rows;
    }

    // ── GROUP BY ──────────────────────────────────────────────────────────────
    if (!stmt->group_by_col.empty()) {
        int grp_idx = -1;
        for (size_t i = 0; i < col_names.size(); i++)
            if (col_names[i] == stmt->group_by_col) { grp_idx = (int)i; break; }

        if (grp_idx >= 0) {
            std::unordered_map<std::string, Row> groups;
            std::vector<std::string> order;
            for (auto& row : rows) {
                if (grp_idx < (int)row.size() && !groups.count(row[grp_idx])) {
                    groups[row[grp_idx]] = row;
                    order.push_back(row[grp_idx]);
                }
            }
            rows.clear();
            for (auto& key : order) rows.push_back(groups[key]);
        }

        // ── HAVING ────────────────────────────────────────────────────────────
        if (!stmt->having_col.empty()) {
            int hav_idx = -1;
            for (size_t i = 0; i < col_names.size(); i++)
                if (col_names[i] == stmt->having_col) { hav_idx = (int)i; break; }
            if (hav_idx >= 0) {
                Result filtered;
                for (auto& row : rows) {
                    if (hav_idx >= (int)row.size()) continue;
                    bool match = false;
                    try {
                        double cell = std::stod(row[hav_idx]);
                        double val  = std::stod(stmt->having_val);
                        if      (stmt->having_op == "=")  match = (cell == val);
                        else if (stmt->having_op == ">")  match = (cell >  val);
                        else if (stmt->having_op == "<")  match = (cell <  val);
                        else if (stmt->having_op == ">=") match = (cell >= val);
                        else if (stmt->having_op == "<=") match = (cell <= val);
                        else if (stmt->having_op == "!=") match = (cell != val);
                    } catch (...) { match = (row[hav_idx] == stmt->having_val); }
                    if (match) filtered.push_back(row);
                }
                rows = filtered;
            }
        }
    }

    // ── ORDER BY ──────────────────────────────────────────────────────────────
    if (!stmt->order_by_col.empty()) {
        int sort_idx = -1;
        for (size_t i = 0; i < col_names.size(); i++)
            if (col_names[i] == stmt->order_by_col) { sort_idx = (int)i; break; }

        if (sort_idx >= 0) {
            bool desc = stmt->order_by_desc;
            std::sort(rows.begin(), rows.end(),
                [sort_idx, desc](const Row& a, const Row& b) {
                    if (sort_idx >= (int)a.size() || sort_idx >= (int)b.size())
                        return false;
                    try {
                        double va = std::stod(a[sort_idx]);
                        double vb = std::stod(b[sort_idx]);
                        return desc ? va > vb : va < vb;
                    } catch (...) {
                        return desc ? a[sort_idx] > b[sort_idx]
                                    : a[sort_idx] < b[sort_idx];
                    }
                });
        }
    }

    // ── LIMIT ─────────────────────────────────────────────────────────────────
    if (stmt->limit_count >= 0 && (int)rows.size() > stmt->limit_count)
        rows.resize(stmt->limit_count);

    return {true, std::to_string(rows.size()) + " row(s) returned.", rows, col_names, ""};
}

// ─── AGGREGATE ────────────────────────────────────────────────────────────────
ExecutionResult Executor::ExecAggregate(std::shared_ptr<Statement> stmt) {
    TableSchema* schema = catalog_->GetTable(stmt->table_name);
    if (!schema)
        return {false, "Table '" + stmt->table_name + "' does not exist.", {}, {}, ""};

    const std::string& func = stmt->aggregate_func;
    const std::string& col  = stmt->aggregate_col;

    int col_idx = -1;
    if (col != "*") {
        for (size_t i = 0; i < schema->columns.size(); i++) {
            if (schema->columns[i].name == col) { col_idx = (int)i; break; }
        }
        if (col_idx < 0)
            return {false, "Column '" + col + "' not found.", {}, {}, ""};
    }

    int    count  = 0;
    double sum    = 0.0;
    double maxval = -DBL_MAX;
    double minval =  DBL_MAX;

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
                    try {
                        double val = std::stod(row[col_idx]);
                        sum    += val;
                        maxval  = std::max(maxval, val);
                        minval  = std::min(minval, val);
                    } catch (...) {}
                }
            }
        }
        bpm_->UnpinPage(pid, false);
    }

    std::string label, value;
    if      (func == "COUNT") { label = "COUNT(*)";       value = std::to_string(count); }
    else if (func == "SUM")   { label = "SUM(" + col + ")";  value = std::to_string((long long)sum); }
    else if (func == "AVG")   { label = "AVG(" + col + ")";  value = count > 0 ? std::to_string(sum / count).substr(0, 6) : "0"; }
    else if (func == "MAX")   { label = "MAX(" + col + ")";  value = count > 0 ? std::to_string((long long)maxval) : "NULL"; }
    else if (func == "MIN")   { label = "MIN(" + col + ")";  value = count > 0 ? std::to_string((long long)minval) : "NULL"; }
    else return {false, "Unknown aggregate: " + func, {}, {}, ""};

    return {true, "1 row returned.", {{value}}, {label}, ""};
}

// ─── JOIN ─────────────────────────────────────────────────────────────────────
ExecutionResult Executor::ExecJoin(std::shared_ptr<Statement> stmt) {
    TableSchema* left_schema  = catalog_->GetTable(stmt->table_name);
    TableSchema* right_schema = catalog_->GetTable(stmt->join_table);

    if (!left_schema)  return {false, "Table '" + stmt->table_name + "' not found.", {}, {}, ""};
    if (!right_schema) return {false, "Table '" + stmt->join_table  + "' not found.", {}, {}, ""};

    int left_col_idx = -1, right_col_idx = -1;
    for (size_t i = 0; i < left_schema->columns.size(); i++)
        if (left_schema->columns[i].name == stmt->join_left_col)
            { left_col_idx = (int)i; break; }
    for (size_t i = 0; i < right_schema->columns.size(); i++)
        if (right_schema->columns[i].name == stmt->join_right_col)
            { right_col_idx = (int)i; break; }

    if (left_col_idx  < 0) return {false, "Column '" + stmt->join_left_col  + "' not found.", {}, {}, ""};
    if (right_col_idx < 0) return {false, "Column '" + stmt->join_right_col + "' not found.", {}, {}, ""};

    Result left_rows  = ScanTable(left_schema);
    Result right_rows = ScanTable(right_schema);

    std::vector<std::string> col_names;
    for (auto& col : left_schema->columns)  col_names.push_back(stmt->table_name + "." + col.name);
    for (auto& col : right_schema->columns) col_names.push_back(stmt->join_table  + "." + col.name);

    Result result_rows;
    auto do_join = [&](const Result& outer, const Result& inner,
                       int outer_idx, int inner_idx, bool swap_output) {
        for (auto& orow : outer) {
            for (auto& irow : inner) {
                if (outer_idx < (int)orow.size() &&
                    inner_idx < (int)irow.size() &&
                    orow[outer_idx] == irow[inner_idx]) {
                    Row combined = swap_output
                        ? (Row(irow.begin(), irow.end()), orow) // keep left+right order
                        : orow;
                    if (swap_output) {
                        combined = irow;
                        combined.insert(combined.end(), orow.begin(), orow.end());
                    } else {
                        combined.insert(combined.end(), irow.begin(), irow.end());
                    }
                    result_rows.push_back(combined);
                }
            }
        }
    };

    if (right_rows.size() < left_rows.size())
        do_join(right_rows, left_rows, right_col_idx, left_col_idx, true);
    else
        do_join(left_rows, right_rows, left_col_idx, right_col_idx, false);

    return {true, std::to_string(result_rows.size()) + " row(s) returned.",
            result_rows, col_names, ""};
}

// ─── DELETE ───────────────────────────────────────────────────────────────────
ExecutionResult Executor::ExecDelete(std::shared_ptr<Statement> stmt,
                                      const QueryPlan& plan) {
    TableSchema* schema = catalog_->GetTable(stmt->table_name);
    if (!schema)
        return {false, "Table '" + stmt->table_name + "' does not exist.", {}, {}, ""};

    int deleted = 0;

    auto delete_row = [&](Page* page, uint16_t slot, const Row& row) {
        UpdateIndexesOnDelete(schema, row); // ← remove from B+ Tree indexes
        page->DeleteRecord(slot);
        deleted++;
    };

    if (plan.type == PlanType::PRIMARY_KEY_SCAN) {
        int pk_idx = -1;
        for (size_t i = 0; i < schema->columns.size(); i++)
            if (schema->columns[i].name == plan.pk_column) { pk_idx = (int)i; break; }

        for (auto pid : schema->page_ids) {
            Page* page = bpm_->FetchPage(pid);
            if (!page) continue;
            uint16_t slot_count = *reinterpret_cast<uint16_t*>(page->GetData() + 6);
            bool found = false;
            for (uint16_t s = 0; s < slot_count; s++) {
                char buf[PAGE_SIZE]; uint16_t len = 0;
                if (page->GetRecord(s, buf, len)) {
                    Row row = DeserializeRow(*schema, buf, len);
                    if (pk_idx < (int)row.size() && row[pk_idx] == plan.pk_value) {
                        delete_row(page, s, row);
                        found = true;
                        break;
                    }
                }
            }
            bpm_->UnpinPage(pid, true);
            if (found) break;
        }
    } else {
        for (auto pid : schema->page_ids) {
            Page* page = bpm_->FetchPage(pid);
            if (!page) continue;
            uint16_t slot_count = *reinterpret_cast<uint16_t*>(page->GetData() + 6);
            for (uint16_t s = 0; s < slot_count; s++) {
                char buf[PAGE_SIZE]; uint16_t len = 0;
                if (page->GetRecord(s, buf, len)) {
                    Row row = DeserializeRow(*schema, buf, len);
                    if (MatchesConditions(row, *schema, stmt->conditions))
                        delete_row(page, s, row);
                }
            }
            bpm_->UnpinPage(pid, true);
        }
    }

    return {true, std::to_string(deleted) + " row(s) deleted.", {}, {}, ""};
}

// ─── UPDATE ───────────────────────────────────────────────────────────────────
ExecutionResult Executor::ExecUpdate(std::shared_ptr<Statement> stmt,
                                      const QueryPlan& plan) {
    TableSchema* schema = catalog_->GetTable(stmt->table_name);
    if (!schema)
        return {false, "Table '" + stmt->table_name + "' does not exist.", {}, {}, ""};

    int set_col_idx = -1;
    for (size_t i = 0; i < schema->columns.size(); i++)
        if (schema->columns[i].name == stmt->set_column) { set_col_idx = (int)i; break; }

    // Build list of (col_idx, new_value) pairs
    std::vector<std::pair<int,std::string>> set_pairs;
    if (!stmt->set_columns.empty()) {
        for (size_t i = 0; i < stmt->set_columns.size(); i++) {
            int idx = -1;
            for (size_t j = 0; j < schema->columns.size(); j++)
                if (schema->columns[j].name == stmt->set_columns[i]) { idx = (int)j; break; }
            if (idx >= 0) set_pairs.push_back({idx, stmt->set_values[i]});
        }
    } else {
        if (set_col_idx < 0)
            return {false, "Column '" + stmt->set_column + "' not found.", {}, {}, ""};
        set_pairs.push_back({set_col_idx, stmt->set_value});
    }

    int updated = 0;

    auto apply_update = [&](Page* page, uint16_t slot, Row row) {
        for (auto& p : set_pairs)
            if (p.first < (int)row.size()) row[p.first] = p.second;
        page->DeleteRecord(slot);
        std::string new_data = SerializeRow(*schema, row);
        page->InsertRecord(new_data.c_str(), (uint16_t)new_data.size());
        updated++;
    };

    if (plan.type == PlanType::PRIMARY_KEY_SCAN) {
        int pk_idx = -1;
        for (size_t i = 0; i < schema->columns.size(); i++)
            if (schema->columns[i].name == plan.pk_column) { pk_idx = (int)i; break; }

        for (auto pid : schema->page_ids) {
            Page* page = bpm_->FetchPage(pid);
            if (!page) continue;
            uint16_t slot_count = *reinterpret_cast<uint16_t*>(page->GetData() + 6);
            bool found = false;
            for (uint16_t s = 0; s < slot_count; s++) {
                char buf[PAGE_SIZE]; uint16_t len = 0;
                if (page->GetRecord(s, buf, len)) {
                    Row row = DeserializeRow(*schema, buf, len);
                    if (pk_idx < (int)row.size() && row[pk_idx] == plan.pk_value) {
                        apply_update(page, s, row);
                        found = true;
                        break;
                    }
                }
            }
            bpm_->UnpinPage(pid, true);
            if (found) break;
        }
    } else {
        for (auto pid : schema->page_ids) {
            Page* page = bpm_->FetchPage(pid);
            if (!page) continue;
            uint16_t slot_count = *reinterpret_cast<uint16_t*>(page->GetData() + 6);
            for (uint16_t s = 0; s < slot_count; s++) {
                char buf[PAGE_SIZE]; uint16_t len = 0;
                if (page->GetRecord(s, buf, len)) {
                    Row row = DeserializeRow(*schema, buf, len);
                    if (MatchesConditions(row, *schema, stmt->conditions))
                        apply_update(page, s, row);
                }
            }
            bpm_->UnpinPage(pid, true);
        }
    }

    return {true, std::to_string(updated) + " row(s) updated.", {}, {}, ""};
}

// ─── Subquery helper ──────────────────────────────────────────────────────────
std::set<std::string> Executor::ExecuteSubquery(std::shared_ptr<Statement> subq) {
    std::set<std::string> values;
    TableSchema* schema = catalog_->GetTable(subq->table_name);
    if (!schema) return values;
    for (auto& row : ScanTable(schema))
        if (MatchesConditions(row, *schema, subq->conditions) && !row.empty())
            values.insert(row[0]);
    return values;
}

// ─── Foreign key validation ───────────────────────────────────────────────────
bool Executor::ValidateForeignKey(const std::string& ref_table,
                                   const std::string& ref_col,
                                   const std::string& value) {
    TableSchema* schema = catalog_->GetTable(ref_table);
    if (!schema) return false;
    int col_idx = -1;
    for (size_t i = 0; i < schema->columns.size(); i++)
        if (schema->columns[i].name == ref_col) { col_idx = (int)i; break; }
    if (col_idx < 0) return false;
    for (auto& row : ScanTable(schema))
        if (col_idx < (int)row.size() && row[col_idx] == value)
            return true;
    return false;
}

// ─── ALTER TABLE ──────────────────────────────────────────────────────────────
ExecutionResult Executor::ExecAlterTable(std::shared_ptr<Statement> stmt) {
    TableSchema* schema = catalog_->GetTable(stmt->table_name);
    if (!schema)
        return {false, "Table '" + stmt->table_name + "' does not exist.", {}, {}, ""};

    if (stmt->alter_action == "ADD") {
        for (auto& col : schema->columns)
            if (col.name == stmt->alter_column.name)
                return {false, "Column '" + stmt->alter_column.name + "' already exists.", {}, {}, ""};

        Column col;
        col.name           = stmt->alter_column.name;
        col.type           = (stmt->alter_column.type == "INT") ? DataType::INT : DataType::VARCHAR;
        col.size           = stmt->alter_column.size;
        col.is_primary_key = false;
        col.not_null       = stmt->alter_column.not_null;
        col.has_default    = stmt->alter_column.has_default;
        col.default_value  = stmt->alter_column.default_value;
        schema->columns.push_back(col);
        catalog_->Save();
        return {true, "Column '" + col.name + "' added to '" + stmt->table_name + "'.", {}, {}, ""};

    } else if (stmt->alter_action == "DROP") {
        for (auto& col : schema->columns)
            if (col.name == stmt->alter_col_name && col.is_primary_key)
                return {false, "Cannot drop primary key column.", {}, {}, ""};

        size_t before = schema->columns.size();
        schema->columns.erase(
            std::remove_if(schema->columns.begin(), schema->columns.end(),
                [&](const Column& c) { return c.name == stmt->alter_col_name; }),
            schema->columns.end());

        if (schema->columns.size() == before)
            return {false, "Column '" + stmt->alter_col_name + "' not found.", {}, {}, ""};

        catalog_->Save();
        return {true, "Column '" + stmt->alter_col_name + "' dropped.", {}, {}, ""};
    }

    return {false, "Unknown ALTER action.", {}, {}, ""};
}

} // namespace FarhanDB
