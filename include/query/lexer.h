#pragma once
#include <string>
#include <vector>

namespace FarhanDB {

enum class TokenType {
    // Keywords
    SELECT, INSERT, UPDATE, DELETE, CREATE, DROP, TABLE,
    FROM, WHERE, INTO, VALUES, SET, AND, OR, NOT,
    INT, VARCHAR, PRIMARY, KEY, INDEX, ON,
    BEGIN, COMMIT, ROLLBACK,

    // Aggregate functions
    COUNT, SUM, AVG, MAX, MIN,

    // JOIN
    JOIN, INNER, LEFT, RIGHT,

    // ORDER BY / LIMIT / DISTINCT
    ORDER, BY, ASC, DESC, LIMIT, DISTINCT,

    // GROUP BY / HAVING
    GROUP, HAVING,

    // NULL / DEFAULT
    NULLVAL, DEFAULT,

    // Subquery
    IN,

    // Foreign key
    FOREIGN, REFERENCES,

    // BETWEEN / LIKE
    BETWEEN, LIKE,

    // Literals
    INTEGER_LITERAL,
    STRING_LITERAL,
    IDENTIFIER,

    // Operators
    EQ, NEQ, LT, GT, LTE, GTE,
    PLUS, MINUS, STAR, SLASH,
    COMMA, SEMICOLON, LPAREN, RPAREN,
    DOT,

    // Special
    EOF_TOKEN,
    UNKNOWN
};

struct Token {
    TokenType   type;
    std::string value;
    int         line;
};

class Lexer {
public:
    explicit Lexer(const std::string& input);

    std::vector<Token>  Tokenize();
    Token               NextToken();
    bool                HasMore() const;

private:
    std::string input_;
    size_t      pos_;
    int         line_;

    char        Peek() const;
    char        Advance();
    void        SkipWhitespace();
    void        SkipComment();
    Token       ReadString();
    Token       ReadNumber();
    Token       ReadIdentifierOrKeyword();
    TokenType   KeywordType(const std::string& word) const;
};

} // namespace FarhanDB
