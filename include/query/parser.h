#pragma once
#include "query/lexer.h"
#include <memory>
#include <vector>
#include <string>
#include <variant>

namespace FarhanDB {

// AST Node types
enum class StatementType {
    SELECT, INSERT, UPDATE, DELETE,
    CREATE_TABLE, DROP_TABLE,
    BEGIN_TXN, COMMIT_TXN, ROLLBACK_TXN
};

struct ColumnDef {
    std::string name;
    std::string type;   // "INT" or "VARCHAR"
    int         size;   // for VARCHAR
    bool        is_primary_key;
};

struct Condition {
    std::string column;
    std::string op;       // =, !=, <, >, <=, >=
    std::string value;
};

struct Statement {
    StatementType               type;
    std::string                 table_name;
    std::vector<std::string>    columns;       // SELECT columns
    std::vector<std::string>    values;        // INSERT values
    std::vector<ColumnDef>      column_defs;   // CREATE TABLE
    std::vector<Condition>      conditions;    // WHERE clause
    std::string                 set_column;    // UPDATE SET
    std::string                 set_value;
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
};

} // namespace FarhanDB
