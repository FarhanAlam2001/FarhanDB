#include "query/parser.h"
#include <stdexcept>

namespace FarhanDB {

Parser::Parser(const std::vector<Token>& tokens)
    : tokens_(tokens), pos_(0) {}

Token& Parser::Current() { return tokens_[pos_]; }
Token& Parser::Peek(int offset) { return tokens_[pos_ + offset]; }
Token& Parser::Consume() { return tokens_[pos_++]; }

Token& Parser::Expect(TokenType type) {
    if (Current().type != type)
        throw std::runtime_error("Parser: unexpected token '" + Current().value + "'");
    return Consume();
}

bool Parser::Check(TokenType type) const { return tokens_[pos_].type == type; }
bool Parser::Match(TokenType type) {
    if (Check(type)) { Consume(); return true; }
    return false;
}

bool Parser::IsAggregateToken(TokenType t) const {
    return t == TokenType::COUNT || t == TokenType::SUM ||
           t == TokenType::AVG   || t == TokenType::MAX ||
           t == TokenType::MIN;
}

std::shared_ptr<Statement> Parser::Parse() {
    if (Check(TokenType::SELECT))   return ParseSelect();
    if (Check(TokenType::INSERT))   return ParseInsert();
    if (Check(TokenType::UPDATE))   return ParseUpdate();
    if (Check(TokenType::DELETE))   return ParseDelete();
    if (Check(TokenType::CREATE))   return ParseCreateTable();
    if (Check(TokenType::DROP))     return ParseDropTable();
    if (Check(TokenType::BEGIN))  {
        Consume();
        auto s = std::make_shared<Statement>();
        s->type = StatementType::BEGIN_TXN;
        return s;
    }
    if (Check(TokenType::COMMIT)) {
        Consume();
        auto s = std::make_shared<Statement>();
        s->type = StatementType::COMMIT_TXN;
        return s;
    }
    if (Check(TokenType::ROLLBACK)) {
        Consume();
        auto s = std::make_shared<Statement>();
        s->type = StatementType::ROLLBACK_TXN;
        return s;
    }
    throw std::runtime_error("Parser: unknown statement starting with '" + Current().value + "'");
}

std::shared_ptr<Statement> Parser::ParseSelect() {
    auto stmt = std::make_shared<Statement>();
    stmt->type = StatementType::SELECT;
    Expect(TokenType::SELECT);

    // ✅ Check for aggregate function: COUNT(*), SUM(col), AVG(col), MAX(col), MIN(col)
    if (IsAggregateToken(Current().type)) {
        stmt->aggregate_func = Current().value;
        // uppercase it
        for (auto& c : stmt->aggregate_func) c = toupper(c);
        Consume();
        Expect(TokenType::LPAREN);
        if (Check(TokenType::STAR)) {
            stmt->aggregate_col = "*";
            Consume();
        } else {
            stmt->aggregate_col = Expect(TokenType::IDENTIFIER).value;
        }
        Expect(TokenType::RPAREN);
    } else if (Check(TokenType::STAR)) {
        Consume();
        stmt->columns.push_back("*");
    } else {
        stmt->columns.push_back(Expect(TokenType::IDENTIFIER).value);
        while (Match(TokenType::COMMA))
            stmt->columns.push_back(Expect(TokenType::IDENTIFIER).value);
    }

    Expect(TokenType::FROM);
    stmt->table_name = Expect(TokenType::IDENTIFIER).value;

    if (Check(TokenType::WHERE))
        stmt->conditions = ParseWhere();

    return stmt;
}

std::shared_ptr<Statement> Parser::ParseInsert() {
    auto stmt = std::make_shared<Statement>();
    stmt->type = StatementType::INSERT;
    Expect(TokenType::INSERT);
    Expect(TokenType::INTO);
    stmt->table_name = Expect(TokenType::IDENTIFIER).value;
    Expect(TokenType::VALUES);
    Expect(TokenType::LPAREN);
    stmt->values.push_back(Consume().value);
    while (Match(TokenType::COMMA))
        stmt->values.push_back(Consume().value);
    Expect(TokenType::RPAREN);
    return stmt;
}

std::shared_ptr<Statement> Parser::ParseUpdate() {
    auto stmt = std::make_shared<Statement>();
    stmt->type = StatementType::UPDATE;
    Expect(TokenType::UPDATE);
    stmt->table_name = Expect(TokenType::IDENTIFIER).value;
    Expect(TokenType::SET);
    stmt->set_column = Expect(TokenType::IDENTIFIER).value;
    Expect(TokenType::EQ);
    stmt->set_value  = Consume().value;
    if (Check(TokenType::WHERE))
        stmt->conditions = ParseWhere();
    return stmt;
}

std::shared_ptr<Statement> Parser::ParseDelete() {
    auto stmt = std::make_shared<Statement>();
    stmt->type = StatementType::DELETE;
    Expect(TokenType::DELETE);
    Expect(TokenType::FROM);
    stmt->table_name = Expect(TokenType::IDENTIFIER).value;
    if (Check(TokenType::WHERE))
        stmt->conditions = ParseWhere();
    return stmt;
}

std::shared_ptr<Statement> Parser::ParseCreateTable() {
    auto stmt = std::make_shared<Statement>();
    stmt->type = StatementType::CREATE_TABLE;
    Expect(TokenType::CREATE);
    Expect(TokenType::TABLE);
    stmt->table_name = Expect(TokenType::IDENTIFIER).value;
    Expect(TokenType::LPAREN);

    while (!Check(TokenType::RPAREN)) {
        ColumnDef col;
        col.name = Expect(TokenType::IDENTIFIER).value;
        if (Check(TokenType::INT)) { Consume(); col.type = "INT"; col.size = 4; }
        else if (Check(TokenType::VARCHAR)) {
            Consume(); col.type = "VARCHAR";
            Expect(TokenType::LPAREN);
            col.size = std::stoi(Consume().value);
            Expect(TokenType::RPAREN);
        }
        if (Check(TokenType::PRIMARY)) {
            Consume(); Expect(TokenType::KEY);
            col.is_primary_key = true;
        }
        stmt->column_defs.push_back(col);
        if (!Check(TokenType::RPAREN)) Expect(TokenType::COMMA);
    }
    Expect(TokenType::RPAREN);
    return stmt;
}

std::shared_ptr<Statement> Parser::ParseDropTable() {
    auto stmt = std::make_shared<Statement>();
    stmt->type = StatementType::DROP_TABLE;
    Expect(TokenType::DROP);
    Expect(TokenType::TABLE);
    stmt->table_name = Expect(TokenType::IDENTIFIER).value;
    return stmt;
}

std::vector<Condition> Parser::ParseWhere() {
    Expect(TokenType::WHERE);
    std::vector<Condition> conditions;
    do {
        Condition cond;
        cond.column = Expect(TokenType::IDENTIFIER).value;
        cond.op     = Consume().value;
        cond.value  = Consume().value;
        conditions.push_back(cond);
    } while (Match(TokenType::AND));
    return conditions;
}

} // namespace FarhanDB
