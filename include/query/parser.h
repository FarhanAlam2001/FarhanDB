#pragma once
#include "query/lexer.h"
#include <memory>
#include <vector>
#include <string>

namespace FarhanDB {

enum class StatementType {
    SELECT, INSERT, UPDATE, DELETE,
    CREATE_TABLE, DROP_TABLE,
    BEGIN_TXN, COMMIT_TXN, ROLLBACK_TXN
};

struct ColumnDef {
    std::string name;
    std::string type;
    int         size;
    bool        is_primary_key;
};

struct Condition {
    std::string column;
    std::string op;
    std::string value;
};

struct Statement {
    StatementType               type;
    std::string                 table_name;
    std::vector<std::string>    columns;
    std::vector<std::string>    values;
    std::vector<ColumnDef>      column_defs;
    std::vector<Condition>      conditions;
    std::string                 set_column;
    std::string                 set_value;

    // Aggregate support
    std::string                 aggregate_func;
    std::string                 aggregate_col;

    // ✅ JOIN support
    std::string                 join_table;     // second table name
    std::string                 join_left_col;  // left table column
    std::string                 join_right_col; // right table column
};

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);

    std::shared_ptr<Statement>  Parse();

private:
    std::vector<Token>  tokens_;
    size_t              pos_;

    Token&          Current();
    Token&          Peek(int offset = 1);
    Token&          Consume();
    Token&          Expect(TokenType type);
    bool            Check(TokenType type) const;
    bool            Match(TokenType type);

    std::shared_ptr<Statement>  ParseSelect();
    std::shared_ptr<Statement>  ParseInsert();
    std::shared_ptr<Statement>  ParseUpdate();
    std::shared_ptr<Statement>  ParseDelete();
    std::shared_ptr<Statement>  ParseCreateTable();
    std::shared_ptr<Statement>  ParseDropTable();
    std::vector<Condition>      ParseWhere();

    bool IsAggregateToken(TokenType t) const;
};

} // namespace FarhanDB
