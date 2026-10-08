#pragma once

#include "ast.hpp"
#include "token.hpp"
#include <map>
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
    // Syntax errors leave the AST unusable; the rest (reused names, bad f-string names) leave it well-formed,
    // so analysis can still run and report everything in one go.
    bool hasSyntaxErrors() const { return m_syntaxErrors > 0; }
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
    void errorAt(int p_line, int p_column, const std::string& p_message);
    void synchronize();

    ImportDecl parseImport();
    EnvImport parseEnvImport();
    FetchDecl parseFetch();
    ProjectDecl parseProject();
    OptionDecl parseOption();
    void parseVariable();
    void parseRawCmake(Program& p_program);
    ExprPtr resolveFString(const Token& p_token);
    bool stringifyForFString(const Expression& p_value, std::string& p_out) const;
    void claimName(const std::string& p_name, int p_line);
    const Token& peekAt(size_t p_offset) const;
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

    ExprPtr parseCondition();
    ExprPtr parseOrCondition();
    ExprPtr parseAndCondition();
    ExprPtr parseNotCondition();
    ExprPtr parseExpression();
    ExprPtr parsePrimary();
    ExprPtr parsePostfix(ExprPtr p_left);
    ExprPtr parseList();
    ExprPtr parseDictOrSet();
    ExprPtr parseTupleOrParen();

    const std::vector<Token>& m_tokens;
    size_t m_pos;
    size_t m_syntaxErrors = 0;
    size_t m_lastErrorPos = 0;
    bool m_hasLastError = false;
    std::vector<std::string> m_errors;
    std::set<std::string> m_envVariables;
    std::map<std::string, ExprPtr> m_variables;   // top-level constants, substituted where used
    std::set<std::string> m_declaredNames;        // packages, options, github imports, targets
    std::set<std::string> m_optionNames;
    std::string m_currentTarget;                  // for {self.name} in f-strings
};

} // namespace pyke
