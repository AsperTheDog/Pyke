#pragma once

#include "token.hpp"
#include <string>
#include <vector>

namespace pyke
{

class Lexer
{
public:
    explicit Lexer(const std::string& p_source);

    std::vector<Token> tokenize();

private:
    Token nextToken();

    char peek() const;
    char peekNext() const;
    char advance();
    bool atEnd() const;
    bool match(char p_expected);

    Token makeToken(TokenType p_type, const std::string& p_value = "") const;
    Token makeError(const std::string& p_message) const;

    Token lexString();
    Token lexNumber();
    Token lexIdentifierOrKeyword();
    void skipComment();

    int measureIndent();
    void emitIndentTokens(int p_newIndent, std::vector<Token>& p_tokens);

    TokenType keywordType(const std::string& p_word) const;

    std::string m_source;
    size_t m_pos;
    int m_line;
    int m_column;
    int m_tokenStartLine;
    int m_tokenStartColumn;

    std::vector<int> m_indentStack;
    bool m_atLineStart;
};

} // namespace pyke
