#pragma once

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
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

// `a and b` / `a or b`
struct BoolOp
{
    std::string op; // "and" | "or"
    ExprPtr left;
    ExprPtr right;
};

// `not a`
struct NotExpr
{
    ExprPtr operand;
};

struct Expression
{
    std::variant<StringLiteral, IntLiteral, BoolLiteral, Identifier, DotAccess, IndexAccess, ListLiteral, DictLiteral, TupleLiteral, Comparison, StringConcat, EnvVariable, BoolOp, NotExpr> value;
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

// cmake("...") at the top level: emitted verbatim into the root CMakeLists.txt
struct RawCmake
{
    std::string text;
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
    int line = 0;
    int column = 0;
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
    int column = 0; // of the target name
};

struct Program
{
    std::vector<ImportDecl> imports;
    std::vector<EnvImport> env_imports;
    std::vector<FetchDecl> fetches;
    std::optional<ProjectDecl> project;
    std::vector<OptionDecl> options;
    std::vector<TargetDecl> targets;
    std::vector<RawCmake> rawCmake;
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

inline ExprPtr cloneExpr(const Expression& p_expr)
{
    auto l_clone = [&](const ExprPtr& p_e) { return cloneExpr(*p_e); };
    auto l_cloneAll = [&](const std::vector<ExprPtr>& p_v)
    {
        std::vector<ExprPtr> l_out;
        for (const ExprPtr& l_e : p_v) l_out.push_back(cloneExpr(*l_e));
        return l_out;
    };

    ExprPtr l_result = std::make_unique<Expression>();
    l_result->line = p_expr.line;
    l_result->column = p_expr.column;

    std::visit([&](const auto& p_node)
    {
        using T = std::decay_t<decltype(p_node)>;
        if constexpr (std::is_same_v<T, DotAccess>) l_result->value = DotAccess{l_clone(p_node.object), p_node.member};
        else if constexpr (std::is_same_v<T, IndexAccess>) l_result->value = IndexAccess{l_clone(p_node.object), l_clone(p_node.index)};
        else if constexpr (std::is_same_v<T, ListLiteral>) l_result->value = ListLiteral{l_cloneAll(p_node.elements)};
        else if constexpr (std::is_same_v<T, TupleLiteral>) l_result->value = TupleLiteral{l_cloneAll(p_node.elements)};
        else if constexpr (std::is_same_v<T, DictLiteral>)
        {
            DictLiteral l_dict;
            for (const auto& l_entry : p_node.entries) l_dict.entries.emplace_back(l_clone(l_entry.first), l_clone(l_entry.second));
            l_result->value = std::move(l_dict);
        }
        else if constexpr (std::is_same_v<T, Comparison>) l_result->value = Comparison{l_clone(p_node.left), p_node.op, l_clone(p_node.right)};
        else if constexpr (std::is_same_v<T, StringConcat>) l_result->value = StringConcat{l_clone(p_node.left), l_clone(p_node.right)};
        else if constexpr (std::is_same_v<T, BoolOp>) l_result->value = BoolOp{p_node.op, l_clone(p_node.left), l_clone(p_node.right)};
        else if constexpr (std::is_same_v<T, NotExpr>) l_result->value = NotExpr{l_clone(p_node.operand)};
        else l_result->value = p_node; // plain-data nodes
    }, p_expr.value);

    return l_result;
}

} // namespace pyke
