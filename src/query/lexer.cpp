#include "query/lexer.h"
#include <cctype>
#include <stdexcept>
#include <algorithm>

namespace FarhanDB {

Lexer::Lexer(const std::string& input)
    : input_(input), pos_(0), line_(1) {}

char Lexer::Peek() const {
    if (pos_ >= input_.size()) return '\0';
    return input_[pos_];
}

char Lexer::Advance() {
    char c = input_[pos_++];
    if (c == '\n') line_++;
    return c;
}

void Lexer::SkipWhitespace() {
    while (pos_ < input_.size() && std::isspace(Peek())) Advance();
}

bool Lexer::HasMore() const {
    return pos_ < input_.size();
}

Token Lexer::ReadString() {
    Advance();
    std::string val;
    while (HasMore() && Peek() != '\'') val += Advance();
    Advance();
    return {TokenType::STRING_LITERAL, val, line_};
}

Token Lexer::ReadNumber() {
    std::string val;
    while (HasMore() && std::isdigit(Peek())) val += Advance();
    return {TokenType::INTEGER_LITERAL, val, line_};
}

TokenType Lexer::KeywordType(const std::string& word) const {
    std::string upper = word;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    if (upper == "SELECT")   return TokenType::SELECT;
    if (upper == "INSERT")   return TokenType::INSERT;
    if (upper == "UPDATE")   return TokenType::UPDATE;
    if (upper == "DELETE")   return TokenType::DELETE;
    if (upper == "CREATE")   return TokenType::CREATE;
    if (upper == "DROP")     return TokenType::DROP;
    if (upper == "TABLE")    return TokenType::TABLE;
    if (upper == "FROM")     return TokenType::FROM;
    if (upper == "WHERE")    return TokenType::WHERE;
    if (upper == "INTO")     return TokenType::INTO;
    if (upper == "VALUES")   return TokenType::VALUES;
    if (upper == "SET")      return TokenType::SET;
    if (upper == "AND")      return TokenType::AND;
    if (upper == "OR")       return TokenType::OR;
    if (upper == "NOT")      return TokenType::NOT;
    if (upper == "INT")      return TokenType::INT;
    if (upper == "VARCHAR")  return TokenType::VARCHAR;
    if (upper == "PRIMARY")  return TokenType::PRIMARY;
    if (upper == "KEY")      return TokenType::KEY;
    if (upper == "INDEX")    return TokenType::INDEX;
    if (upper == "ON")       return TokenType::ON;
    if (upper == "BEGIN")    return TokenType::BEGIN;
    if (upper == "COMMIT")   return TokenType::COMMIT;
    if (upper == "ROLLBACK") return TokenType::ROLLBACK;
    // Aggregate functions
    if (upper == "COUNT")    return TokenType::COUNT;
    if (upper == "SUM")      return TokenType::SUM;
    if (upper == "AVG")      return TokenType::AVG;
    if (upper == "MAX")      return TokenType::MAX;
    if (upper == "MIN")      return TokenType::MIN;
    // ✅ JOIN keywords
    if (upper == "JOIN")     return TokenType::JOIN;
    if (upper == "INNER")    return TokenType::INNER;
    if (upper == "LEFT")     return TokenType::LEFT;
    if (upper == "RIGHT")    return TokenType::RIGHT;
    return TokenType::IDENTIFIER;
}

Token Lexer::ReadIdentifierOrKeyword() {
    std::string val;
    while (HasMore() && (std::isalnum(Peek()) || Peek() == '_')) val += Advance();
    return {KeywordType(val), val, line_};
}

Token Lexer::NextToken() {
    SkipWhitespace();
    if (!HasMore()) return {TokenType::EOF_TOKEN, "", line_};

    char c = Peek();

    if (c == '\'')          return ReadString();
    if (std::isdigit(c))    return ReadNumber();
    if (std::isalpha(c) || c == '_') return ReadIdentifierOrKeyword();

    Advance();
    switch (c) {
        case '=':  return {TokenType::EQ,        "=",  line_};
        case '<':
            if (Peek() == '=') { Advance(); return {TokenType::LTE, "<=", line_}; }
            return {TokenType::LT, "<", line_};
        case '>':
            if (Peek() == '=') { Advance(); return {TokenType::GTE, ">=", line_}; }
            return {TokenType::GT, ">", line_};
        case '!':
            if (Peek() == '=') { Advance(); return {TokenType::NEQ, "!=", line_}; }
            break;
        case '.':  return {TokenType::DOT,       ".",  line_}; // ✅ table.column
        case ',':  return {TokenType::COMMA,     ",",  line_};
        case ';':  return {TokenType::SEMICOLON,  ";",  line_};
        case '(':  return {TokenType::LPAREN,    "(",  line_};
        case ')':  return {TokenType::RPAREN,    ")",  line_};
        case '*':  return {TokenType::STAR,      "*",  line_};
        case '+':  return {TokenType::PLUS,      "+",  line_};
        case '-':  return {TokenType::MINUS,     "-",  line_};
        case '/':  return {TokenType::SLASH,     "/",  line_};
    }
    return {TokenType::UNKNOWN, std::string(1, c), line_};
}

std::vector<Token> Lexer::Tokenize() {
    std::vector<Token> tokens;
    while (HasMore()) {
        Token t = NextToken();
        if (t.type == TokenType::EOF_TOKEN) break;
        tokens.push_back(t);
    }
    tokens.push_back({TokenType::EOF_TOKEN, "", line_});
    return tokens;
}

} // namespace FarhanDB
