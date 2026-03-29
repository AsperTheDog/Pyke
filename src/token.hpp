#pragma once

#include <string>
#include <string_view>

namespace pyke
{

enum class TokenType
{
    IDENTIFIER,
    STRING_LITERAL,
    INT_LITERAL,
    BOOL_LITERAL,

    FROM,
    IMPORT,
    PROJECT,
    TARGET,
    DEF,
    SELF_,
    IF,
    ELIF,
    ELSE,
    OPTION,
    TRUE_KW,
    FALSE_KW,

    PUBLIC_KW,
    PRIVATE_KW,

    AS,

    BOOL_TYPE,
    STR_TYPE,
    PATH_TYPE,

    AT,

    EXECUTABLE,
    SHARED_LIBRARY,
    STATIC_LIBRARY,
    HEADER_ONLY,

    EQUALS,
    PLUS,
    PLUS_EQUALS,
    COLON,
    COMMA,
    DOT,
    LEFT_PAREN,
    RIGHT_PAREN,
    LEFT_BRACKET,
    RIGHT_BRACKET,
    LEFT_BRACE,
    RIGHT_BRACE,
    COMPARISON,

    NEWLINE,
    INDENT,
    DEDENT,

    COMMENT,
    END_OF_FILE,
    ERROR_TOKEN,
};

struct Token
{
    TokenType type;
    std::string value;
    int line;
    int column;
};

inline std::string_view tokenTypeName(TokenType p_type)
{
    switch (p_type)
    {
        case TokenType::IDENTIFIER:     return "Identifier";
        case TokenType::STRING_LITERAL:  return "StringLiteral";
        case TokenType::INT_LITERAL:     return "IntLiteral";
        case TokenType::BOOL_LITERAL:    return "BoolLiteral";
        case TokenType::FROM:           return "From";
        case TokenType::IMPORT:         return "Import";
        case TokenType::PROJECT:        return "Project";
        case TokenType::TARGET:         return "Target";
        case TokenType::DEF:            return "Def";
        case TokenType::SELF_:          return "Self";
        case TokenType::IF:             return "If";
        case TokenType::ELIF:           return "Elif";
        case TokenType::ELSE:           return "Else";
        case TokenType::OPTION:         return "Option";
        case TokenType::TRUE_KW:        return "True";
        case TokenType::FALSE_KW:       return "False";
        case TokenType::PUBLIC_KW:      return "Public";
        case TokenType::PRIVATE_KW:     return "Private";
        case TokenType::AS:             return "As";
        case TokenType::BOOL_TYPE:      return "Bool";
        case TokenType::STR_TYPE:       return "Str";
        case TokenType::PATH_TYPE:      return "Path";
        case TokenType::AT:             return "At";
        case TokenType::EXECUTABLE:     return "Executable";
        case TokenType::SHARED_LIBRARY: return "SharedLibrary";
        case TokenType::STATIC_LIBRARY: return "StaticLibrary";
        case TokenType::HEADER_ONLY:    return "HeaderOnly";
        case TokenType::EQUALS:         return "Equals";
        case TokenType::PLUS:           return "Plus";
        case TokenType::PLUS_EQUALS:    return "PlusEquals";
        case TokenType::COLON:          return "Colon";
        case TokenType::COMMA:          return "Comma";
        case TokenType::DOT:            return "Dot";
        case TokenType::LEFT_PAREN:     return "LeftParen";
        case TokenType::RIGHT_PAREN:    return "RightParen";
        case TokenType::LEFT_BRACKET:   return "LeftBracket";
        case TokenType::RIGHT_BRACKET:  return "RightBracket";
        case TokenType::LEFT_BRACE:     return "LeftBrace";
        case TokenType::RIGHT_BRACE:    return "RightBrace";
        case TokenType::COMPARISON:     return "Comparison";
        case TokenType::NEWLINE:        return "Newline";
        case TokenType::INDENT:         return "Indent";
        case TokenType::DEDENT:         return "Dedent";
        case TokenType::COMMENT:        return "Comment";
        case TokenType::END_OF_FILE:    return "EndOfFile";
        case TokenType::ERROR_TOKEN:    return "Error";
    }
    return "Unknown";
}

} // namespace pyke
