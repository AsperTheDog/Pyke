#pragma once

#include "ast.hpp"
#include "token.hpp"
#include <set>
#include <string>
#include <vector>

namespace pyke
{

class Parser
{
public:
    explicit Parser(const std::vector<Token>& p_tokens);

    Program parse();

    bool hasErrors() const { return !m_errors.empty(); }
    const std::vector<std::string>& errors() const { return m_errors; }

private:
    const Token& peek() const;
    const Token& previous() const;
    const Token& advance();
    bool atEnd() const;
    bool check(TokenType p_type) const;
    bool match(TokenType p_type);
    void expect(TokenType p_type, const std::string& p_message);
    void skipNewlines();
    void skipCollectionWhitespace();

    void error(const std::string& p_message);
    void synchronize();

    ImportDecl parseImport();
    EnvImport parseEnvImport();
    FetchDecl parseFetch();
    ProjectDecl parseProject();
    OptionDecl parseOption();
    TargetDecl parseTarget(TargetType p_type, const std::string& p_path);

    struct DecoratorArgs
    {
        std::string path;
        bool sourceGroups = true;
        bool copyDlls = false;
        bool test = false;
        bool unityBuild = false;
    };
    TargetType parseDecorator(DecoratorArgs& p_args);
    std::vector<Dependency> parseDependencies();
    Method parseMethod();

    StmtPtr parseStatement();
    StmtPtr parseAssignmentOrAugAssign();
    StmtPtr parseIfStatement();

    ExprPtr parseExpression();
    ExprPtr parsePrimary();
    ExprPtr parsePostfix(ExprPtr p_left);
    ExprPtr parseList();
    ExprPtr parseDictOrSet();
    ExprPtr parseTupleOrParen();

    const std::vector<Token>& m_tokens;
    size_t m_pos;
    std::vector<std::string> m_errors;
    std::set<std::string> m_envVariables;
};

} // namespace pyke
