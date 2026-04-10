#include "query/catalog.h"
#include <stdexcept>
#include <sstream>

namespace FarhanDB {

Catalog::Catalog(const std::string& catalog_file)
    : catalog_file_(catalog_file) {
    Load();
}

Catalog::~Catalog() {
    Save();
}

bool Catalog::CreateTable(const TableSchema& schema) {
    if (tables_.count(schema.table_name)) return false;
    tables_[schema.table_name] = schema;
    Save();
    return true;
}

bool Catalog::DropTable(const std::string& table_name) {
    if (!tables_.count(table_name)) return false;
    tables_.erase(table_name);
    Save();
    return true;
}

bool Catalog::TableExists(const std::string& table_name) const {
    return tables_.count(table_name) > 0;
}

TableSchema* Catalog::GetTable(const std::string& table_name) {
    auto it = tables_.find(table_name);
    if (it == tables_.end()) return nullptr;
    return &it->second;
}

bool Catalog::CreateIndex(const IndexInfo& idx) {
    auto it = tables_.find(idx.table_name);
    if (it == tables_.end()) return false;
    it->second.indexes.push_back(idx);
    Save();
    return true;
}

void Catalog::Save() {
    std::ofstream f(catalog_file_);
    for (auto& [name, schema] : tables_) {
        f << "TABLE " << schema.table_name << " " << schema.root_page_id;
        f << " PAGES " << schema.page_ids.size();
        for (auto pid : schema.page_ids) f << " " << pid;
        f << "\n";

        for (auto& col : schema.columns) {
            f << "COL " << col.name << " "
              << (col.type == DataType::INT ? "INT" : "VARCHAR") << " "
              << col.size << " "
              << (col.is_primary_key ? 1 : 0) << " "
              << (col.not_null ? 1 : 0) << " "
              << (col.has_default ? 1 : 0) << " "
              << (col.has_default ? col.default_value : "-") << " "
              << (col.is_foreign_key ? 1 : 0) << " "
              << (col.is_foreign_key ? col.fk_ref_table : "-") << " "
              << (col.is_foreign_key ? col.fk_ref_column : "-") << "\n";
        }

        for (auto& idx : schema.indexes)
            f << "INDEX " << idx.index_name << " " << idx.column_name << "\n";

        f << "END\n";
    }
}

void Catalog::Load() {
    std::ifstream f(catalog_file_);
    if (!f.is_open()) return;

    std::string line;
    TableSchema* current = nullptr;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "TABLE") {
            TableSchema schema;
            ss >> schema.table_name >> schema.root_page_id;

            std::string pages_tag;
            if (ss >> pages_tag && pages_tag == "PAGES") {
                size_t count;
                ss >> count;
                for (size_t i = 0; i < count; i++) {
                    uint32_t pid; ss >> pid;
                    schema.page_ids.push_back(pid);
                }
            }

            if (schema.page_ids.empty() && schema.root_page_id != UINT32_MAX)
                schema.page_ids.push_back(schema.root_page_id);

            tables_[schema.table_name] = schema;
            current = &tables_[schema.table_name];

        } else if (tag == "COL" && current) {
            Column col;
            std::string type_str, default_val, fk_table, fk_col;
            int pk, nn, hd, fk;
            ss >> col.name >> type_str >> col.size >> pk >> nn >> hd
               >> default_val >> fk >> fk_table >> fk_col;
            col.type           = (type_str == "INT") ? DataType::INT : DataType::VARCHAR;
            col.is_primary_key = (pk == 1);
            col.not_null       = (nn == 1);
            col.has_default    = (hd == 1);
            col.is_foreign_key = (fk == 1);
            // Only use values if not placeholder
            if (hd == 1 && default_val != "-") col.default_value = default_val;
            if (fk == 1 && fk_table != "-")   col.fk_ref_table  = fk_table;
            if (fk == 1 && fk_col != "-")     col.fk_ref_column = fk_col;
            current->columns.push_back(col);

        } else if (tag == "INDEX" && current) {
            IndexInfo idx;
            idx.table_name = current->table_name;
            ss >> idx.index_name >> idx.column_name;
            current->indexes.push_back(idx);

        } else if (tag == "END") {
            current = nullptr;
        }
    }
}

} // namespace FarhanDB
