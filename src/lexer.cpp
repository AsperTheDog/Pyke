#include "lexer.hpp"
#include <algorithm>

namespace pyke
{

Lexer::Lexer(const std::string& p_source)
    : m_source(p_source)
    , m_pos(0)
    , m_line(1)
    , m_column(1)
    , m_tokenStartLine(1)
    , m_tokenStartColumn(1)
    , m_atLineStart(true)
{
    m_indentStack.push_back(0);
}

std::vector<Token> Lexer::tokenize()
{
    std::vector<Token> l_tokens;

    while (!atEnd())
    {
        if (m_atLineStart)
        {
            m_atLineStart = false;

            int l_savedPos = static_cast<int>(m_pos);
            int l_savedLine = m_line;
            int l_savedCol = m_column;

            int l_indent = measureIndent();

            if (atEnd() || peek() == '\n' || peek() == '\r')
            {
                if (!atEnd())
                {
                    if (peek() == '\r') advance();
                    if (!atEnd() && peek() == '\n') advance();
                    m_atLineStart = true;
                }
                continue;
            }
            if (peek() == '#')
            {
                skipComment();
                if (!atEnd())
                {
                    if (peek() == '\r') advance();
                    if (!atEnd() && peek() == '\n') advance();
                    m_atLineStart = true;
                }
                continue;
            }

            emitIndentTokens(l_indent, l_tokens);
            continue;
        }

        char l_c = peek();

        if (l_c == ' ' || l_c == '\t')
        {
            advance();
            continue;
        }

        if (l_c == '\n' || l_c == '\r')
        {
            m_tokenStartLine = m_line;
            m_tokenStartColumn = m_column;

            if (l_c == '\r') advance();
            if (!atEnd() && peek() == '\n') advance();

            l_tokens.push_back(makeToken(TokenType::NEWLINE, "\\n"));
            m_atLineStart = true;
            continue;
        }

        if (l_c == '#')
        {
            skipComment();
            continue;
        }

        m_tokenStartLine = m_line;
        m_tokenStartColumn = m_column;

        if (l_c == '"')
        {
            l_tokens.push_back(lexString());
            continue;
        }

        if (l_c >= '0' && l_c <= '9')
        {
            l_tokens.push_back(lexNumber());
            continue;
        }

        if ((l_c >= 'a' && l_c <= 'z') || (l_c >= 'A' && l_c <= 'Z') || l_c == '_')
        {
            l_tokens.push_back(lexIdentifierOrKeyword());
            continue;
        }

        switch (l_c)
        {
            case '@':
                advance();
                l_tokens.push_back(makeToken(TokenType::AT, "@"));
                break;
            case '=':
                advance();
                if (!atEnd() && peek() == '=')
                {
                    advance();
                    l_tokens.push_back(makeToken(TokenType::COMPARISON, "=="));
                }
                else
                {
                    l_tokens.push_back(makeToken(TokenType::EQUALS, "="));
                }
                break;
            case '+':
                advance();
                if (!atEnd() && peek() == '=')
                {
                    advance();
                    l_tokens.push_back(makeToken(TokenType::PLUS_EQUALS, "+="));
                }
                else
                {
                    l_tokens.push_back(makeToken(TokenType::PLUS, "+"));
                }
                break;
            case ':':
                advance();
                l_tokens.push_back(makeToken(TokenType::COLON, ":"));
                break;
            case ',':
                advance();
                l_tokens.push_back(makeToken(TokenType::COMMA, ","));
                break;
            case '.':
                advance();
                l_tokens.push_back(makeToken(TokenType::DOT, "."));
                break;
            case '(':
                advance();
                l_tokens.push_back(makeToken(TokenType::LEFT_PAREN, "("));
                break;
            case ')':
                advance();
                l_tokens.push_back(makeToken(TokenType::RIGHT_PAREN, ")"));
                break;
            case '[':
                advance();
                l_tokens.push_back(makeToken(TokenType::LEFT_BRACKET, "["));
                break;
            case ']':
                advance();
                l_tokens.push_back(makeToken(TokenType::RIGHT_BRACKET, "]"));
                break;
            case '{':
                advance();
                l_tokens.push_back(makeToken(TokenType::LEFT_BRACE, "{"));
                break;
            case '}':
                advance();
                l_tokens.push_back(makeToken(TokenType::RIGHT_BRACE, "}"));
                break;
            default:
                advance();
                l_tokens.push_back(makeError(std::string("Unexpected character: '") + l_c + "'"));
                break;
        }
    }

    m_tokenStartLine = m_line;
    m_tokenStartColumn = m_column;
    while (m_indentStack.size() > 1)
    {
        m_indentStack.pop_back();
        l_tokens.push_back(makeToken(TokenType::DEDENT));
    }

    l_tokens.push_back(makeToken(TokenType::END_OF_FILE));
    return l_tokens;
}

char Lexer::peek() const
{
    if (atEnd()) return '\0';
    return m_source[m_pos];
}

char Lexer::peekNext() const
{
    if (m_pos + 1 >= m_source.size()) return '\0';
    return m_source[m_pos + 1];
}

char Lexer::advance()
{
    char l_c = m_source[m_pos++];
    if (l_c == '\n')
    {
        m_line++;
        m_column = 1;
    }
    else
    {
        m_column++;
    }
    return l_c;
}

bool Lexer::atEnd() const
{
    return m_pos >= m_source.size();
}

bool Lexer::match(char p_expected)
{
    if (atEnd() || m_source[m_pos] != p_expected) return false;
    advance();
    return true;
}

Token Lexer::makeToken(TokenType p_type, const std::string& p_value) const
{
    return Token{p_type, p_value, m_tokenStartLine, m_tokenStartColumn};
}

Token Lexer::makeError(const std::string& p_message) const
{
    return Token{TokenType::ERROR_TOKEN, p_message, m_tokenStartLine, m_tokenStartColumn};
}

Token Lexer::lexString()
{
    advance();
    std::string l_value;

    while (!atEnd() && peek() != '"')
    {
        if (peek() == '\n' || peek() == '\r')
        {
            return makeError("Unterminated string literal");
        }
        if (peek() == '\\')
        {
            advance();
            if (atEnd()) return makeError("Unterminated escape sequence");
            char l_escaped = advance();
            switch (l_escaped)
            {
                case 'n':  l_value += '\n'; break;
                case 't':  l_value += '\t'; break;
                case '\\': l_value += '\\'; break;
                case '"':  l_value += '"';  break;
                default:
                    return makeError(std::string("Unknown escape sequence: \\") + l_escaped);
            }
        }
        else
        {
            l_value += advance();
        }
    }

    if (atEnd())
    {
        return makeError("Unterminated string literal");
    }

    advance();
    return makeToken(TokenType::STRING_LITERAL, l_value);
}

Token Lexer::lexNumber()
{
    std::string l_value;
    while (!atEnd() && peek() >= '0' && peek() <= '9')
    {
        l_value += advance();
    }
    return makeToken(TokenType::INT_LITERAL, l_value);
}

Token Lexer::lexIdentifierOrKeyword()
{
    std::string l_value;
    while (!atEnd() && ((peek() >= 'a' && peek() <= 'z') || (peek() >= 'A' && peek() <= 'Z') || (peek() >= '0' && peek() <= '9') || peek() == '_'))
    {
        l_value += advance();
    }

    TokenType l_type = keywordType(l_value);
    return makeToken(l_type, l_value);
}

void Lexer::skipComment()
{
    while (!atEnd() && peek() != '\n' && peek() != '\r')
    {
        advance();
    }
}

int Lexer::measureIndent()
{
    int l_indent = 0;
    while (!atEnd() && (peek() == ' ' || peek() == '\t'))
    {
        if (peek() == '\t')
        {
            l_indent += 4;
        }
        else
        {
            l_indent += 1;
        }
        advance();
    }
    return l_indent;
}

void Lexer::emitIndentTokens(int p_newIndent, std::vector<Token>& p_tokens)
{
    int l_currentIndent = m_indentStack.back();

    m_tokenStartLine = m_line;
    m_tokenStartColumn = 1;

    if (p_newIndent > l_currentIndent)
    {
        m_indentStack.push_back(p_newIndent);
        p_tokens.push_back(makeToken(TokenType::INDENT));
    }
    else if (p_newIndent < l_currentIndent)
    {
        while (m_indentStack.size() > 1 && m_indentStack.back() > p_newIndent)
        {
            m_indentStack.pop_back();
            p_tokens.push_back(makeToken(TokenType::DEDENT));
        }
        if (m_indentStack.back() != p_newIndent)
        {
            p_tokens.push_back(makeError("Indentation does not match any outer level"));
        }
    }
}

TokenType Lexer::keywordType(const std::string& p_word) const
{
    if (p_word == "from")           return TokenType::FROM;
    if (p_word == "as")             return TokenType::AS;
    if (p_word == "import")         return TokenType::IMPORT;
    if (p_word == "project")        return TokenType::PROJECT;
    if (p_word == "target")         return TokenType::TARGET;
    if (p_word == "def")            return TokenType::DEF;
    if (p_word == "self")           return TokenType::SELF_;
    if (p_word == "if")             return TokenType::IF;
    if (p_word == "elif")           return TokenType::ELIF;
    if (p_word == "else")           return TokenType::ELSE;
    if (p_word == "option")         return TokenType::OPTION;
    if (p_word == "True")           return TokenType::TRUE_KW;
    if (p_word == "False")          return TokenType::FALSE_KW;
    if (p_word == "PUBLIC")         return TokenType::PUBLIC_KW;
    if (p_word == "PRIVATE")        return TokenType::PRIVATE_KW;
    if (p_word == "bool")           return TokenType::BOOL_TYPE;
    if (p_word == "str")            return TokenType::STR_TYPE;
    if (p_word == "path")           return TokenType::PATH_TYPE;
    if (p_word == "Executable")     return TokenType::EXECUTABLE;
    if (p_word == "SharedLibrary")  return TokenType::SHARED_LIBRARY;
    if (p_word == "StaticLibrary")  return TokenType::STATIC_LIBRARY;
    if (p_word == "HeaderOnly")     return TokenType::HEADER_ONLY;
    return TokenType::IDENTIFIER;
}

} // namespace pyke
