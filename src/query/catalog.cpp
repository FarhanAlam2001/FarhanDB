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

void Catalog::Save() {
    std::ofstream f(catalog_file_);
    for (auto& [name, schema] : tables_) {
        // Save page IDs on one line
        f << "TABLE " << schema.table_name << " " << schema.root_page_id;
        f << " PAGES " << schema.page_ids.size();
        for (auto pid : schema.page_ids) f << " " << pid;
        f << "\n";

        for (auto& col : schema.columns) {
            f << "COL " << col.name << " "
              << (col.type == DataType::INT ? "INT" : "VARCHAR") << " "
              << col.size << " "
              << (col.is_primary_key ? 1 : 0) << "\n";
        }
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

            // Load page IDs if present
            std::string pages_tag;
            if (ss >> pages_tag && pages_tag == "PAGES") {
                size_t count;
                ss >> count;
                for (size_t i = 0; i < count; i++) {
                    uint32_t pid;
                    ss >> pid;
                    schema.page_ids.push_back(pid);
                }
            }

            // If no page_ids saved, add root_page_id
            if (schema.page_ids.empty() && schema.root_page_id != UINT32_MAX)
                schema.page_ids.push_back(schema.root_page_id);

            tables_[schema.table_name] = schema;
            current = &tables_[schema.table_name];
        } else if (tag == "COL" && current) {
            Column col;
            std::string type_str;
            int pk;
            ss >> col.name >> type_str >> col.size >> pk;
            col.type = (type_str == "INT") ? DataType::INT : DataType::VARCHAR;
            col.is_primary_key = (pk == 1);
            current->columns.push_back(col);
        } else if (tag == "END") {
            current = nullptr;
        }
    }
}

} // namespace FarhanDB
