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
    if (Check(TokenType::DROP))     return ParseDropTable();
    if (Check(TokenType::ALTER))    return ParseAlterTable();
    if (Check(TokenType::CREATE)) {
        Consume();
        if (Check(TokenType::INDEX)) return ParseCreateIndex();
        // put CREATE back via re-parse trick — just re-enter with TABLE
        if (Check(TokenType::TABLE)) {
            // already consumed CREATE, parse rest
            auto stmt = std::make_shared<Statement>();
            stmt->type = StatementType::CREATE_TABLE;
            Expect(TokenType::TABLE);
            stmt->table_name = Expect(TokenType::IDENTIFIER).value;
            Expect(TokenType::LPAREN);

            while (!Check(TokenType::RPAREN)) {
                ColumnDef col;
                col.name = Expect(TokenType::IDENTIFIER).value;

                if (Check(TokenType::INT)) {
                    Consume(); col.type = "INT"; col.size = 4;
                } else if (Check(TokenType::VARCHAR)) {
                    Consume(); col.type = "VARCHAR";
                    Expect(TokenType::LPAREN);
                    col.size = std::stoi(Consume().value);
                    Expect(TokenType::RPAREN);
                }

                bool parsing_constraints = true;
                while (parsing_constraints) {
                    if (Check(TokenType::PRIMARY)) {
                        Consume(); Expect(TokenType::KEY);
                        col.is_primary_key = true;
                    } else if (Check(TokenType::NOT)) {
                        Consume(); Expect(TokenType::NULLVAL);
                        col.not_null = true;
                    } else if (Check(TokenType::DEFAULT)) {
                        Consume();
                        col.has_default   = true;
                        col.default_value = Consume().value;
                    } else if (Check(TokenType::REFERENCES)) {
                        Consume();
                        col.is_foreign_key = true;
                        col.fk_ref_table   = Expect(TokenType::IDENTIFIER).value;
                        Expect(TokenType::LPAREN);
                        col.fk_ref_column  = Expect(TokenType::IDENTIFIER).value;
                        Expect(TokenType::RPAREN);
                    } else {
                        parsing_constraints = false;
                    }
                }

                stmt->column_defs.push_back(col);
                if (!Check(TokenType::RPAREN)) Expect(TokenType::COMMA);
            }
            Expect(TokenType::RPAREN);
            return stmt;
        }
        throw std::runtime_error("Parser: expected TABLE or INDEX after CREATE");
    }
    if (Check(TokenType::BEGIN)) {
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

std::shared_ptr<Statement> Parser::ParseCreateIndex() {
    auto stmt = std::make_shared<Statement>();
    stmt->type = StatementType::CREATE_INDEX;
    Expect(TokenType::INDEX);
    stmt->index_name = Expect(TokenType::IDENTIFIER).value;
    Expect(TokenType::ON);
    stmt->table_name = Expect(TokenType::IDENTIFIER).value;
    Expect(TokenType::LPAREN);
    stmt->index_col  = Expect(TokenType::IDENTIFIER).value;
    Expect(TokenType::RPAREN);
    return stmt;
}

std::shared_ptr<Statement> Parser::ParseSelect() {
    auto stmt = std::make_shared<Statement>();
    stmt->type = StatementType::SELECT;
    Expect(TokenType::SELECT);

    if (Match(TokenType::DISTINCT))
        stmt->is_distinct = true;

    if (IsAggregateToken(Current().type)) {
        stmt->aggregate_func = Current().value;
        for (auto& c : stmt->aggregate_func) c = toupper(c);
        Consume();
        Expect(TokenType::LPAREN);
        if (Check(TokenType::STAR)) {
            stmt->aggregate_col = "*"; Consume();
        } else {
            stmt->aggregate_col = Expect(TokenType::IDENTIFIER).value;
        }
        Expect(TokenType::RPAREN);
    } else if (Check(TokenType::STAR)) {
        Consume();
        stmt->columns.push_back("*");
    } else {
        std::string col = Expect(TokenType::IDENTIFIER).value;
        if (Match(TokenType::DOT)) col = Expect(TokenType::IDENTIFIER).value;
        stmt->columns.push_back(col);
        while (Match(TokenType::COMMA)) {
            std::string next_col = Expect(TokenType::IDENTIFIER).value;
            if (Match(TokenType::DOT)) next_col = Expect(TokenType::IDENTIFIER).value;
            stmt->columns.push_back(next_col);
        }
    }

    Expect(TokenType::FROM);
    stmt->table_name = Expect(TokenType::IDENTIFIER).value;

    // JOIN
    if (Check(TokenType::JOIN) || Check(TokenType::INNER) ||
        Check(TokenType::LEFT) || Check(TokenType::RIGHT)) {
        if (!Check(TokenType::JOIN)) Consume();
        Expect(TokenType::JOIN);
        stmt->join_table = Expect(TokenType::IDENTIFIER).value;
        Expect(TokenType::ON);
        Expect(TokenType::IDENTIFIER); Expect(TokenType::DOT);
        stmt->join_left_col = Expect(TokenType::IDENTIFIER).value;
        Expect(TokenType::EQ);
        Expect(TokenType::IDENTIFIER); Expect(TokenType::DOT);
        stmt->join_right_col = Expect(TokenType::IDENTIFIER).value;
    }

    // WHERE with possible subquery
    if (Check(TokenType::WHERE)) {
        Consume();
        // Check for subquery: col IN (SELECT ...)
        if (pos_ + 1 < tokens_.size() &&
            tokens_[pos_].type == TokenType::IDENTIFIER &&
            tokens_[pos_ + 1].type == TokenType::IN) {
            stmt->where_in_col = Consume().value; // column name
            Expect(TokenType::IN);
            Expect(TokenType::LPAREN);
            // Parse inner SELECT
            Parser inner_parser(std::vector<Token>(tokens_.begin() + pos_, tokens_.end()));
            stmt->subquery = inner_parser.ParseSelect();
            // Skip past the subquery tokens (find matching RPAREN)
            int depth = 1;
            while (pos_ < tokens_.size() && depth > 0) {
                if (tokens_[pos_].type == TokenType::LPAREN) depth++;
                else if (tokens_[pos_].type == TokenType::RPAREN) depth--;
                pos_++;
            }
        } else {
            // Normal WHERE
            std::vector<Condition> conditions;
            do {
                Condition cond;
                cond.column = Expect(TokenType::IDENTIFIER).value;
                if (Check(TokenType::BETWEEN)) {
                    Consume();
                    std::string low  = Consume().value;
                    Expect(TokenType::AND);
                    std::string high = Consume().value;
                    cond.op    = "BETWEEN";
                    cond.value = low + "," + high;
                } else if (Check(TokenType::LIKE)) {
                    Consume();
                    cond.op    = "LIKE";
                    cond.value = Consume().value;
                } else {
                    cond.op    = Consume().value;
                    cond.value = Consume().value;
                }
                if (Check(TokenType::OR)) {
                    cond.connector = "OR"; Consume();
                    conditions.push_back(cond);
                } else if (Check(TokenType::AND)) {
                    cond.connector = "AND";
                    conditions.push_back(cond);
                    Consume();
                } else {
                    cond.connector = "AND";
                    conditions.push_back(cond);
                    break;
                }
            } while (!Check(TokenType::EOF_TOKEN) &&
                     !Check(TokenType::ORDER) &&
                     !Check(TokenType::GROUP) &&
                     !Check(TokenType::HAVING) &&
                     !Check(TokenType::LIMIT) &&
                     !Check(TokenType::SEMICOLON));
            stmt->conditions = conditions;
        }
    }

    // GROUP BY
    if (Check(TokenType::GROUP)) {
        Consume(); Expect(TokenType::BY);
        stmt->group_by_col = Expect(TokenType::IDENTIFIER).value;
    }

    // HAVING
    if (Check(TokenType::HAVING)) {
        Consume();
        stmt->having_col = Expect(TokenType::IDENTIFIER).value;
        stmt->having_op  = Consume().value;
        stmt->having_val = Consume().value;
    }

    // ORDER BY
    if (Check(TokenType::ORDER)) {
        Consume(); Expect(TokenType::BY);
        stmt->order_by_col = Expect(TokenType::IDENTIFIER).value;
        if (Match(TokenType::DESC)) stmt->order_by_desc = true;
        else Match(TokenType::ASC);
    }

    // LIMIT
    if (Check(TokenType::LIMIT)) {
        Consume();
        stmt->limit_count = std::stoi(Expect(TokenType::INTEGER_LITERAL).value);
    }

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

    // Parse one or more col = val pairs
    do {
        std::string col = Expect(TokenType::IDENTIFIER).value;
        Expect(TokenType::EQ);
        std::string val = Consume().value;
        stmt->set_columns.push_back(col);
        stmt->set_values.push_back(val);
    } while (Match(TokenType::COMMA));

    // Keep single-column compat
    if (!stmt->set_columns.empty()) {
        stmt->set_column = stmt->set_columns[0];
        stmt->set_value  = stmt->set_values[0];
    }

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

        bool parsing_constraints = true;
        while (parsing_constraints) {
            if (Check(TokenType::PRIMARY)) {
                Consume(); Expect(TokenType::KEY);
                col.is_primary_key = true;
            } else if (Check(TokenType::NOT)) {
                Consume(); Expect(TokenType::NULLVAL);
                col.not_null = true;
            } else if (Check(TokenType::DEFAULT)) {
                Consume();
                col.has_default = true;
                col.default_value = Consume().value;
            } else if (Check(TokenType::REFERENCES)) {
                Consume();
                col.is_foreign_key = true;
                col.fk_ref_table   = Expect(TokenType::IDENTIFIER).value;
                Expect(TokenType::LPAREN);
                col.fk_ref_column  = Expect(TokenType::IDENTIFIER).value;
                Expect(TokenType::RPAREN);
            } else {
                parsing_constraints = false;
            }
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

        if (Check(TokenType::BETWEEN)) {
            // BETWEEN low AND high
            Consume(); // consume BETWEEN
            std::string low  = Consume().value;
            Expect(TokenType::AND);
            std::string high = Consume().value;
            cond.op    = "BETWEEN";
            cond.value = low + "," + high;
        } else if (Check(TokenType::LIKE)) {
            // LIKE pattern
            Consume(); // consume LIKE
            cond.op    = "LIKE";
            cond.value = Consume().value;
        } else {
            cond.op    = Consume().value;
            cond.value = Consume().value;
        }

        if (Check(TokenType::OR)) {
            cond.connector = "OR"; Consume();
            conditions.push_back(cond);
        } else if (Check(TokenType::AND)) {
            cond.connector = "AND";
            conditions.push_back(cond);
            Consume();
        } else {
            cond.connector = "AND";
            conditions.push_back(cond);
            break;
        }
    } while (!Check(TokenType::EOF_TOKEN) &&
             !Check(TokenType::ORDER) &&
             !Check(TokenType::GROUP) &&
             !Check(TokenType::HAVING) &&
             !Check(TokenType::LIMIT) &&
             !Check(TokenType::SEMICOLON));
    return conditions;
}

std::shared_ptr<Statement> Parser::ParseAlterTable() {
    auto stmt = std::make_shared<Statement>();
    stmt->type = StatementType::ALTER_TABLE;
    Expect(TokenType::ALTER);
    Expect(TokenType::TABLE);
    stmt->table_name = Expect(TokenType::IDENTIFIER).value;

    if (Check(TokenType::ADD)) {
        Consume();
        Match(TokenType::COLUMN); // COLUMN keyword is optional
        stmt->alter_action = "ADD";
        ColumnDef col;
        col.name = Expect(TokenType::IDENTIFIER).value;
        if (Check(TokenType::INT)) {
            Consume(); col.type = "INT"; col.size = 4;
        } else if (Check(TokenType::VARCHAR)) {
            Consume(); col.type = "VARCHAR";
            Expect(TokenType::LPAREN);
            col.size = std::stoi(Consume().value);
            Expect(TokenType::RPAREN);
        }
        if (Check(TokenType::DEFAULT)) {
            Consume();
            col.has_default   = true;
            col.default_value = Consume().value;
        }
        if (Check(TokenType::NOT)) {
            Consume(); Expect(TokenType::NULLVAL);
            col.not_null = true;
        }
        stmt->alter_column = col;
    } else if (Check(TokenType::DROP)) {
        Consume();
        Match(TokenType::COLUMN);
        stmt->alter_action   = "DROP";
        stmt->alter_col_name = Expect(TokenType::IDENTIFIER).value;
    } else {
        throw std::runtime_error("ALTER TABLE: expected ADD or DROP");
    }
    return stmt;
}

} // namespace FarhanDB
