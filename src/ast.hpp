#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace pyke
{

struct Expression;
struct Statement;

using ExprPtr = std::unique_ptr<Expression>;
using StmtPtr = std::unique_ptr<Statement>;

struct StringLiteral
{
    std::string value;
};

struct IntLiteral
{
    int value;
};

struct BoolLiteral
{
    bool value;
};

struct Identifier
{
    std::string name;
};

struct DotAccess
{
    ExprPtr object;
    std::string member;
};

struct IndexAccess
{
    ExprPtr object;
    ExprPtr index;
};

struct ListLiteral
{
    std::vector<ExprPtr> elements;
};

struct DictLiteral
{
    std::vector<std::pair<ExprPtr, ExprPtr>> entries;
};

struct TupleLiteral
{
    std::vector<ExprPtr> elements;
};

struct Comparison
{
    ExprPtr left;
    std::string op;
    ExprPtr right;
};

struct StringConcat
{
    ExprPtr left;
    ExprPtr right;
};

struct EnvVariable
{
    std::string name;
};

struct Expression
{
    std::variant<StringLiteral, IntLiteral, BoolLiteral, Identifier, DotAccess, IndexAccess, ListLiteral, DictLiteral, TupleLiteral, Comparison, StringConcat, EnvVariable> value;
    int line = 0;
    int column = 0;
};

struct AssignStatement
{
    ExprPtr target;
    ExprPtr value;
};

struct AugAssignStatement
{
    ExprPtr target;
    ExprPtr value;
};

struct IfBranch
{
    ExprPtr condition;
    std::vector<StmtPtr> body;
};

struct IfStatement
{
    std::vector<IfBranch> branches;
};

struct Statement
{
    std::variant<AssignStatement, AugAssignStatement, IfStatement> value;
    int line = 0;
    int column = 0;
};

struct ImportDecl
{
    std::vector<std::string> packages;
    std::string condition;
    int line = 0;
};

struct EnvImport
{
    std::vector<std::string> variables;
    int line = 0;
};

struct FetchDecl
{
    std::string repo;
    std::string name;
    std::string tag;
    std::string condition;
    int line = 0;
};

struct ProjectDecl
{
    std::string name;
    std::string version;
    std::string lang;
    std::string outputDir;
    bool presets = false;
    int line = 0;
};

struct OptionDecl
{
    std::string name;
    std::string type;
    ExprPtr defaultValue;
    int line = 0;
};

struct Dependency
{
    std::string visibility;
    std::string name;
};

enum class TargetType
{
    EXECUTABLE,
    SHARED_LIBRARY,
    STATIC_LIBRARY,
    HEADER_ONLY,
};

struct Method
{
    std::string name;
    std::vector<StmtPtr> body;
    int line = 0;
};

struct TargetDecl
{
    TargetType type;
    std::string path;
    bool sourceGroups = true;
    bool copyDlls = false;
    bool test = false;
    bool unityBuild = false;
    std::string name;
    std::vector<Dependency> dependencies;
    std::vector<Method> methods;
    int line = 0;
};

struct Program
{
    std::vector<ImportDecl> imports;
    std::vector<EnvImport> env_imports;
    std::vector<FetchDecl> fetches;
    std::optional<ProjectDecl> project;
    std::vector<OptionDecl> options;
    std::vector<TargetDecl> targets;
};

inline ExprPtr makeExpr(int p_line, int p_col, auto&& p_val)
{
    ExprPtr l_expr = std::make_unique<Expression>();
    l_expr->value = std::forward<decltype(p_val)>(p_val);
    l_expr->line = p_line;
    l_expr->column = p_col;
    return l_expr;
}

inline StmtPtr makeStmt(int p_line, int p_col, auto&& p_val)
{
    StmtPtr l_stmt = std::make_unique<Statement>();
    l_stmt->value = std::forward<decltype(p_val)>(p_val);
    l_stmt->line = p_line;
    l_stmt->column = p_col;
    return l_stmt;
}

} // namespace pyke
