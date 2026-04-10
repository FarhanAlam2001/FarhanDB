#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <cstdint>

namespace FarhanDB {

enum class DataType { INT, VARCHAR };

struct Column {
    std::string name;
    DataType    type;
    int         size;
    bool        is_primary_key;
    bool        not_null        = false;
    bool        has_default     = false;
    std::string default_value   = "";
    // Foreign key
    bool        is_foreign_key  = false;
    std::string fk_ref_table    = "";   // referenced table
    std::string fk_ref_column   = "";   // referenced column
};

struct IndexInfo {
    std::string index_name;
    std::string table_name;
    std::string column_name;
};

struct TableSchema {
    std::string              table_name;
    std::vector<Column>      columns;
    uint32_t                 root_page_id;
    std::vector<uint32_t>    page_ids;
    std::vector<IndexInfo>   indexes;   // indexes on this table
};

class Catalog {
public:
    explicit Catalog(const std::string& catalog_file);
    ~Catalog();

    bool            CreateTable(const TableSchema& schema);
    bool            DropTable(const std::string& table_name);
    bool            TableExists(const std::string& table_name) const;
    TableSchema*    GetTable(const std::string& table_name);
    bool            CreateIndex(const IndexInfo& idx);
    void            Save();
    void            Load();

private:
    std::string                                  catalog_file_;
    std::unordered_map<std::string, TableSchema> tables_;
};

} // namespace FarhanDB
