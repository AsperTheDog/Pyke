#include "analyzer.hpp"
#include "suggest.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace pyke
{

namespace
{

const std::set<std::string> s_builtinVars = {"platform", "compiler", "build_type"};

const std::map<std::string, std::set<std::string>> s_builtinValues = {
    {"platform", {"windows", "linux", "macos"}},
    {"compiler", {"msvc", "gcc", "clang"}},
    {"build_type", {"debug", "release", "relwithdebinfo", "minsizerel"}},
};

const std::set<std::string> s_configureAttrs = {
    "sources", "includes", "definitions", "flags", "link", "link_dirs",
    "copy_files", "pch", "assets", "commands", "cmake",
};

const std::set<std::string> s_exportableAttrs = {
    "includes", "definitions", "flags", "link", "link_dirs", "pch",
};

const std::set<std::string> s_installAttrs = {"runtime", "library", "headers"};

const std::set<std::string> s_standards = {"c++11", "c++14", "c++17", "c++20", "c++23", "c++26"};

bool isStringy(const Expression& p_expr)
{
    if (std::holds_alternative<StringLiteral>(p_expr.value)) return true;
    if (std::holds_alternative<EnvVariable>(p_expr.value)) return true;
    if (auto* l_concat = std::get_if<StringConcat>(&p_expr.value))
    {
        return isStringy(*l_concat->left) && isStringy(*l_concat->right);
    }
    return false;
}

bool isLinkable(const Expression& p_expr)
{
    return std::holds_alternative<Identifier>(p_expr.value) || std::holds_alternative<DotAccess>(p_expr.value);
}

std::string linkableName(const Expression& p_expr)
{
    if (auto* l_id = std::get_if<Identifier>(&p_expr.value)) return l_id->name;
    if (auto* l_dot = std::get_if<DotAccess>(&p_expr.value))
    {
        if (auto* l_inner = std::get_if<Identifier>(&l_dot->object->value)) return l_inner->name + "." + l_dot->member;
        return l_dot->member;
    }
    return "";
}

// Result of decoding the left-hand side of `self.<attr>`, `self.exports.<attr>` or `self.definitions[...]`.
struct AttrRef
{
    bool valid = false;
    bool exported = false;
    bool indexed = false;
    std::string name;
    std::string problem;
    int column = 0; // of the attribute name
};

AttrRef decodeTarget(const Expression& p_expr)
{
    AttrRef l_ref;
    const Expression* l_cur = &p_expr;

    if (auto* l_idx = std::get_if<IndexAccess>(&l_cur->value))
    {
        l_ref.indexed = true;
        l_cur = l_idx->object.get();
    }

    auto* l_dot = std::get_if<DotAccess>(&l_cur->value);
    if (!l_dot)
    {
        if (auto* l_id = std::get_if<Identifier>(&l_cur->value))
        {
            l_ref.problem = "'" + l_id->name + "' cannot be assigned inside a target; set attributes with 'self." + l_id->name +
                            "', or declare constants at the top level of the file";
        }
        else
        {
            l_ref.problem = "assignment target must be 'self.<attribute>'";
        }
        l_ref.column = l_cur->column;
        return l_ref;
    }

    const Expression* l_owner = l_dot->object.get();
    l_ref.name = l_dot->member;
    l_ref.column = l_cur->column + 1; // a DotAccess expression is positioned at its '.'

    if (auto* l_ownerDot = std::get_if<DotAccess>(&l_owner->value))
    {
        if (l_ownerDot->member != "exports")
        {
            l_ref.problem = "unknown attribute path '" + l_ownerDot->member + "." + l_dot->member + "'" +
                            didYouMean(l_ownerDot->member, std::vector<std::string>{"exports"});
            l_ref.column = l_owner->column + 1;
            return l_ref;
        }
        l_ref.exported = true;
        l_owner = l_ownerDot->object.get();
    }

    auto* l_self = std::get_if<Identifier>(&l_owner->value);
    if (!l_self || l_self->name != "self")
    {
        l_ref.problem = "assignment target must start with 'self'";
        return l_ref;
    }

    if (l_ref.name == "exports" && !l_ref.exported)
    {
        l_ref.problem = "'self.exports' cannot be assigned directly; use 'self.exports.<attribute>'";
        return l_ref;
    }

    l_ref.valid = true;
    return l_ref;
}

} // namespace

Analyzer::Analyzer(const Program& p_program)
    : m_program(p_program) {}

bool Analyzer::analyze()
{
    collectNames();
    validateProject();
    validateOptions();
    validateTargets();
    validateDependencies();
    validateMethods();
    detectCycles();
    return !hasErrors();
}

void Analyzer::collectNames()
{
    for (const TargetDecl& l_target : m_program.targets)
    {
        if (m_localTargetNames.count(l_target.name))
        {
            error(where(l_target.line) + "duplicate target name: '" + l_target.name + "'");
        }
        m_localTargetNames.insert(l_target.name);
        m_targetNames.insert(l_target.name);
        m_targetsByName.emplace(l_target.name, &l_target);
    }

    for (const ImportDecl& l_import : m_program.imports)
    {
        for (const std::string& l_pkg : l_import.packages)
        {
            m_importedPackages.insert(l_pkg);
        }
    }

    for (const EnvImport& l_env : m_program.env_imports)
    {
        for (const std::string& l_var : l_env.variables)
        {
            m_envVars.insert(l_var);
        }
    }

    for (const FetchDecl& l_fetch : m_program.fetches)
    {
        if (m_targetNames.count(l_fetch.name) || m_importedPackages.count(l_fetch.name))
        {
            error(where(l_fetch.line) + "github import name '" + l_fetch.name + "' collides with another target or package");
        }
        m_targetNames.insert(l_fetch.name);
    }
}

void Analyzer::validateProject()
{
    if (!m_program.project.has_value())
    {
        error("missing project declaration: add project(\"Name\", version=\"1.0.0\", lang=\"c++20\") (CMake requires one)");
        return;
    }

    const ProjectDecl& l_proj = *m_program.project;
    std::string l_where = where(l_proj.line);

    if (l_proj.name.empty() || !std::all_of(l_proj.name.begin(), l_proj.name.end(), [](unsigned char p_c) { return std::isalnum(p_c) || p_c == '_' || p_c == '-' || p_c == '.' || p_c == '+'; }))
    {
        error(l_where + "invalid project name '" + l_proj.name + "' (use letters, digits, '_', '-', '.', '+')");
    }

    if (!l_proj.version.empty())
    {
        // CMake accepts up to four numeric components: major[.minor[.patch[.tweak]]]
        bool l_ok = true;
        int l_parts = 1;
        bool l_prevDigit = false;
        for (char l_c : l_proj.version)
        {
            if (l_c == '.')
            {
                if (!l_prevDigit) { l_ok = false; break; }
                l_parts++;
                l_prevDigit = false;
            }
            else if (std::isdigit(static_cast<unsigned char>(l_c))) l_prevDigit = true;
            else { l_ok = false; break; }
        }
        if (!l_ok || !l_prevDigit || l_parts > 4)
        {
            error(l_where + "invalid project version '" + l_proj.version + "' (expected numeric form like 1.2.3)");
        }
    }

    if (!l_proj.lang.empty() && !s_standards.count(l_proj.lang))
    {
        error(l_where + "unsupported lang '" + l_proj.lang + "' (expected one of: c++11, c++14, c++17, c++20, c++23, c++26)");
    }

    if (!l_proj.outputDir.empty())
    {
        std::string l_norm = normalizePath(l_proj.outputDir);
        if (l_norm.empty())
        {
            error(l_where + "output_dir '" + l_proj.outputDir + "' must be a relative path inside the build directory");
        }
    }

    if (m_program.targets.empty())
    {
        warning(l_where + "project declares no targets");
    }
}

void Analyzer::validateOptions()
{
    for (const OptionDecl& l_opt : m_program.options)
    {
        std::string l_where = where(l_opt.line);

        if (!isValidIdentifier(l_opt.name))
        {
            error(l_where + "invalid option name '" + l_opt.name + "'");
        }
        if (s_builtinVars.count(l_opt.name))
        {
            error(l_where + "option '" + l_opt.name + "' shadows a builtin variable");
        }
        if (m_options.count(l_opt.name))
        {
            error(l_where + "duplicate option '" + l_opt.name + "'");
        }
        m_options.emplace(l_opt.name, &l_opt);

        if (l_opt.type != "bool" && l_opt.type != "str" && l_opt.type != "path")
        {
            error(l_where + "option '" + l_opt.name + "': unknown type '" + l_opt.type + "' (expected bool, str or path)");
            continue;
        }

        if (l_opt.defaultValue)
        {
            bool l_isBool = std::holds_alternative<BoolLiteral>(l_opt.defaultValue->value);
            bool l_isStr = std::holds_alternative<StringLiteral>(l_opt.defaultValue->value);
            if (l_opt.type == "bool" && !l_isBool)
            {
                error(l_where + "option '" + l_opt.name + "': default of a bool option must be True or False");
            }
            else if (l_opt.type != "bool" && !l_isStr)
            {
                error(l_where + "option '" + l_opt.name + "': default of a " + l_opt.type + " option must be a string");
            }
        }
    }

    for (const ImportDecl& l_import : m_program.imports)
    {
        if (!l_import.condition.empty())
        {
            auto l_it = m_options.find(l_import.condition);
            if (l_it == m_options.end())
            {
                error(where(l_import.line) + "conditional import refers to unknown option '" + l_import.condition + "'");
            }
            else if (l_it->second->type != "bool")
            {
                error(where(l_import.line) + "conditional import option '" + l_import.condition + "' must be a bool");
            }
        }
    }
}

void Analyzer::validateTargets()
{
    std::map<std::string, const TargetDecl*> l_paths;

    for (const TargetDecl& l_target : m_program.targets)
    {
        std::string l_where = where(l_target.line);

        if (!isValidIdentifier(l_target.name))
        {
            error(l_where + "invalid target name '" + l_target.name + "'");
        }
        if (m_importedPackages.count(l_target.name))
        {
            error(l_where + "target '" + l_target.name + "' has the same name as an imported package");
        }
        if (m_options.count(l_target.name))
        {
            warning(l_where + "target '" + l_target.name + "' has the same name as an option");
        }

        if (l_target.test && l_target.type != TargetType::EXECUTABLE)
        {
            error(l_where + "target '" + l_target.name + "': test=True is only valid on @Executable targets");
        }

        std::string l_raw = l_target.path.empty() ? l_target.name : l_target.path;
        std::string l_norm = normalizePath(l_raw);
        if (l_norm.empty())
        {
            error(l_where + "target '" + l_target.name + "': path '" + l_raw + "' must be relative and stay inside the output directory");
            continue;
        }

        auto l_prev = l_paths.find(l_norm);
        if (l_prev != l_paths.end())
        {
            error(l_where + "target '" + l_target.name + "': path '" + l_norm + "' is already used by target '" + l_prev->second->name + "' (each target needs its own directory)");
        }
        else
        {
            l_paths.emplace(l_norm, &l_target);
        }
    }
}

void Analyzer::validateDependencies()
{
    for (const TargetDecl& l_target : m_program.targets)
    {
        std::set<std::string> l_seen;
        for (const Dependency& l_dep : l_target.dependencies)
        {
            std::string l_base = basePackage(l_dep.name);

            if (!m_targetNames.count(l_base) && !m_importedPackages.count(l_base))
            {
                std::vector<std::string> l_known(m_targetNames.begin(), m_targetNames.end());
                l_known.insert(l_known.end(), m_importedPackages.begin(), m_importedPackages.end());
                errorAt(l_dep.line ? l_dep.line : l_target.line, l_dep.column, "target '" + l_target.name + "': unknown dependency '" + l_dep.name +
                        "'" + didYouMean(l_base, l_known) + " (not a target or imported package)");
                continue;
            }
            if (!l_seen.insert(l_dep.name).second)
            {
                warning(where(l_target.line) + "target '" + l_target.name + "': duplicate dependency '" + l_dep.name + "'");
            }
            if (l_dep.name.find('.') != std::string::npos && m_targetNames.count(l_base))
            {
                error(where(l_target.line) + "target '" + l_target.name + "': '" + l_dep.name + "' uses component syntax on a target; components only apply to imported packages");
            }
            if (l_dep.visibility == "PUBLIC" || l_dep.visibility == "PRIVATE" || l_dep.visibility == "INTERFACE") continue;
            error(where(l_target.line) + "target '" + l_target.name + "': invalid dependency visibility '" + l_dep.visibility + "'");
        }
    }
}

void Analyzer::validateMethods()
{
    for (const TargetDecl& l_target : m_program.targets)
    {
        bool l_hasConfigure = false;
        bool l_sawSources = false;
        std::set<std::string> l_methodsSeen;

        for (const Method& l_method : l_target.methods)
        {
            if (!l_methodsSeen.insert(l_method.name).second)
            {
                error(where(l_method.line) + "target '" + l_target.name + "': duplicate method '" + l_method.name + "'");
                continue;
            }

            if (l_method.name == "configure")
            {
                l_hasConfigure = true;
                validateBody(l_target, l_method, l_method.body, l_sawSources);
            }
            else if (l_method.name == "install")
            {
                bool l_ignored = false;
                validateBody(l_target, l_method, l_method.body, l_ignored);
            }
            else
            {
                error(where(l_method.line) + "target '" + l_target.name + "': unknown method '" + l_method.name + "' (only 'configure' and 'install' are allowed)");
            }
        }

        if (!l_hasConfigure)
        {
            error(where(l_target.line) + "target '" + l_target.name + "': missing required 'configure' method");
        }
        else if (!l_sawSources && l_target.type != TargetType::HEADER_ONLY)
        {
            error(where(l_target.line) + "target '" + l_target.name + "': no sources (CMake cannot create " +
                  (l_target.type == TargetType::EXECUTABLE ? "an executable" : "a library") + " without any); set self.sources in configure()");
        }
    }
}

void Analyzer::validateBody(const TargetDecl& p_target, const Method& p_method, const std::vector<StmtPtr>& p_body, bool& p_sawSources)
{
    for (const StmtPtr& l_stmt : p_body)
    {
        if (l_stmt) validateStatement(p_target, p_method, *l_stmt, p_sawSources);
    }
}

void Analyzer::validateStatement(const TargetDecl& p_target, const Method& p_method, const Statement& p_stmt, bool& p_sawSources)
{
    if (auto* l_if = std::get_if<IfStatement>(&p_stmt.value))
    {
        for (size_t l_i = 0; l_i < l_if->branches.size(); l_i++)
        {
            const IfBranch& l_branch = l_if->branches[l_i];
            if (l_branch.condition) validateCondition(p_target, *l_branch.condition, p_stmt.line);
            if (l_branch.body.empty()) warning(where(p_stmt.line) + "empty branch in target '" + p_target.name + "'");
            validateBody(p_target, p_method, l_branch.body, p_sawSources);
        }
        return;
    }

    const Expression* l_lhs = nullptr;
    const Expression* l_rhs = nullptr;
    if (auto* l_assign = std::get_if<AssignStatement>(&p_stmt.value)) { l_lhs = l_assign->target.get(); l_rhs = l_assign->value.get(); }
    else if (auto* l_aug = std::get_if<AugAssignStatement>(&p_stmt.value)) { l_lhs = l_aug->target.get(); l_rhs = l_aug->value.get(); }
    if (!l_lhs || !l_rhs) return;

    std::string l_ctx = where(p_stmt.line) + "target '" + p_target.name + "': ";
    AttrRef l_ref = decodeTarget(*l_lhs);
    int l_line = p_stmt.line;
    std::string l_who = "target '" + p_target.name + "': ";
    if (!l_ref.valid)
    {
        errorAt(l_line, l_ref.column, l_who + l_ref.problem);
        return;
    }

    bool l_inInstall = p_method.name == "install";
    const std::set<std::string>& l_allowed = l_inInstall ? s_installAttrs : s_configureAttrs;

    if (!l_allowed.count(l_ref.name))
    {
        if (s_installAttrs.count(l_ref.name) || s_configureAttrs.count(l_ref.name))
        {
            errorAt(l_line, l_ref.column, l_who + "'" + l_ref.name + "' is only valid inside " + (l_inInstall ? "configure()" : "install()"));
        }
        else
        {
            const std::set<std::string>& l_pool = l_ref.exported ? s_exportableAttrs : l_allowed;
            errorAt(l_line, l_ref.column, l_who + "unknown attribute '" + l_ref.name + "'" + didYouMean(l_ref.name, l_pool));
        }
        return;
    }

    if (l_ref.exported && !s_exportableAttrs.count(l_ref.name))
    {
        errorAt(l_line, l_ref.column, l_who + "'" + l_ref.name + "' cannot be exported (self.exports supports: includes, definitions, flags, link, link_dirs, pch)");
        return;
    }
    if (l_ref.indexed && l_ref.name != "definitions")
    {
        error(l_ctx + "only 'definitions' supports item assignment");
        return;
    }
    if (l_ref.name == "sources") p_sawSources = true;
    if (l_ref.name == "commands") p_sawSources = true;

    if (l_ref.indexed)
    {
        auto* l_idx = std::get_if<IndexAccess>(&l_lhs->value);
        if (!l_idx || !std::holds_alternative<StringLiteral>(l_idx->index->value))
        {
            error(l_ctx + "definition name must be a string literal");
        }
        if (!isStringy(*l_rhs) && !std::holds_alternative<IntLiteral>(l_rhs->value) && !std::holds_alternative<BoolLiteral>(l_rhs->value))
        {
            error(l_ctx + "definition value must be a string, number or bool");
        }
        return;
    }

    validateAttributeValue(p_target, l_ref.name, *l_rhs, p_stmt.line);
}

void Analyzer::validateAttributeValue(const TargetDecl& p_target, const std::string& p_attr, const Expression& p_value, int p_line)
{
    std::string l_ctx = where(p_line) + "target '" + p_target.name + "': ";
    auto* l_list = std::get_if<ListLiteral>(&p_value.value);

    auto l_requireStringList = [&](bool p_allowSingle)
    {
        if (!l_list)
        {
            if (p_allowSingle && isStringy(p_value)) return;
            error(l_ctx + "'" + p_attr + "' expects a list of strings");
            return;
        }
        for (const ExprPtr& l_elem : l_list->elements)
        {
            if (!isStringy(*l_elem))
            {
                error(l_ctx + "'" + p_attr + "' entries must be strings");
                return;
            }
        }
    };

    if (p_attr == "cmake")
    {
        l_requireStringList(true);
        return;
    }

    if (p_attr == "sources" || p_attr == "includes" || p_attr == "flags" || p_attr == "link_dirs" || p_attr == "copy_files")
    {
        l_requireStringList(false);
        if (p_attr == "sources" && l_list && l_list->elements.empty())
        {
            warning(l_ctx + "'sources' is an empty list");
        }
        if (p_attr == "sources" && l_list)
        {
            for (const ExprPtr& l_elem : l_list->elements)
            {
                if (auto* l_s = std::get_if<StringLiteral>(&l_elem->value))
                {
                    if (l_s->value.empty()) error(l_ctx + "empty source path");
                    else if (normalizePath(l_s->value).empty() && l_s->value.find("..") == std::string::npos) error(l_ctx + "source '" + l_s->value + "' must be a relative path");
                }
            }
        }
    }
    else if (p_attr == "pch" || p_attr == "assets")
    {
        l_requireStringList(true);
    }
    else if (p_attr == "link")
    {
        if (!l_list)
        {
            error(l_ctx + "'link' expects a list of targets or packages");
            return;
        }
        for (const ExprPtr& l_elem : l_list->elements)
        {
            if (isStringy(*l_elem)) continue; // raw library name or linker flag, e.g. "vulkan-1"
            if (!isLinkable(*l_elem))
            {
                error(l_ctx + "'link' entries must be targets, imported packages or library-name strings");
                continue;
            }
            std::string l_name = linkableName(*l_elem);
            std::string l_base = basePackage(l_name);
            if (!m_targetNames.count(l_base) && !m_importedPackages.count(l_base))
            {
                std::vector<std::string> l_known(m_targetNames.begin(), m_targetNames.end());
                l_known.insert(l_known.end(), m_importedPackages.begin(), m_importedPackages.end());
                errorAt(l_elem->line ? l_elem->line : p_line, l_elem->column, "target '" + p_target.name + "': unknown link dependency '" + l_name + "'" +
                        didYouMean(l_base, l_known) + " (not a target or imported package)");
            }
            else if (l_base == p_target.name)
            {
                error(l_ctx + "target cannot link to itself");
            }
        }
    }
    else if (p_attr == "definitions")
    {
        auto* l_dict = std::get_if<DictLiteral>(&p_value.value);
        if (!l_dict)
        {
            error(l_ctx + "'definitions' expects a dict like {\"NAME\": value}");
            return;
        }
        for (const std::pair<ExprPtr, ExprPtr>& l_entry : l_dict->entries)
        {
            if (!std::holds_alternative<StringLiteral>(l_entry.first->value))
            {
                error(l_ctx + "definition names must be string literals");
            }
            else if (std::get<StringLiteral>(l_entry.first->value).value.empty())
            {
                error(l_ctx + "definition name cannot be empty");
            }
            if (!isStringy(*l_entry.second) && !std::holds_alternative<IntLiteral>(l_entry.second->value) && !std::holds_alternative<BoolLiteral>(l_entry.second->value))
            {
                error(l_ctx + "definition values must be strings, numbers or bools");
            }
        }
    }
    else if (p_attr == "commands")
    {
        if (!l_list)
        {
            error(l_ctx + "'commands' expects a list of (command, [outputs], [depends]) tuples");
            return;
        }
        for (const ExprPtr& l_elem : l_list->elements)
        {
            auto* l_tuple = std::get_if<TupleLiteral>(&l_elem->value);
            if (!l_tuple || l_tuple->elements.size() < 2 || l_tuple->elements.size() > 3 ||
                !isStringy(*l_tuple->elements[0]) || !std::holds_alternative<ListLiteral>(l_tuple->elements[1]->value) ||
                (l_tuple->elements.size() == 3 && !std::holds_alternative<ListLiteral>(l_tuple->elements[2]->value)))
            {
                error(l_ctx + "each command must be a tuple (\"command\", [outputs]) or (\"command\", [outputs], [depends])");
                continue;
            }
            if (std::get<ListLiteral>(l_tuple->elements[1]->value).elements.empty())
            {
                error(l_ctx + "a command needs at least one output");
            }
        }
    }
    else if (p_attr == "runtime" || p_attr == "library")
    {
        if (!isStringy(p_value)) error(l_ctx + "'" + p_attr + "' expects a destination string");
        if (p_target.type == TargetType::HEADER_ONLY) error(l_ctx + "header-only targets have no '" + p_attr + "' artifact to install");
    }
    else if (p_attr == "headers")
    {
        auto* l_tuple = std::get_if<TupleLiteral>(&p_value.value);
        if (!l_tuple || l_tuple->elements.size() != 2 || !isStringy(*l_tuple->elements[0]) || !isStringy(*l_tuple->elements[1]))
        {
            error(l_ctx + "'headers' expects a (source, destination) tuple of strings");
        }
    }
}

void Analyzer::validateCondition(const TargetDecl& p_target, const Expression& p_cond, int p_line)
{
    std::string l_who = "target '" + p_target.name + "': ";
    auto l_at = [&](const Expression& p_e, const std::string& p_msg) { errorAt(p_e.line ? p_e.line : p_line, p_e.column, l_who + p_msg); };

    if (auto* l_bool = std::get_if<BoolOp>(&p_cond.value))
    {
        validateCondition(p_target, *l_bool->left, p_line);
        validateCondition(p_target, *l_bool->right, p_line);
        return;
    }
    if (auto* l_not = std::get_if<NotExpr>(&p_cond.value))
    {
        validateCondition(p_target, *l_not->operand, p_line);
        return;
    }

    if (auto* l_cmp = std::get_if<Comparison>(&p_cond.value))
    {
        if (l_cmp->op != "==" && l_cmp->op != "!=")
        {
            l_at(p_cond, "unsupported comparison operator '" + l_cmp->op + "'");
            return;
        }
        auto* l_id = std::get_if<Identifier>(&l_cmp->left->value);
        auto* l_str = std::get_if<StringLiteral>(&l_cmp->right->value);
        if (!l_id)
        {
            l_at(*l_cmp->left, "left side of a comparison must be platform, compiler, build_type or a str/path option");
            return;
        }
        if (!l_str)
        {
            l_at(*l_cmp->right, "right side of a comparison must be a string literal");
            return;
        }

        auto l_builtin = s_builtinValues.find(l_id->name);
        if (l_builtin != s_builtinValues.end())
        {
            if (!l_builtin->second.count(l_str->value))
            {
                std::string l_valid;
                for (const std::string& l_v : l_builtin->second) l_valid += (l_valid.empty() ? "" : ", ") + l_v;
                l_at(*l_cmp->right, "'" + l_str->value + "' is not a valid value for " + l_id->name + didYouMean(l_str->value, l_builtin->second) +
                                    " (expected: " + l_valid + ")");
            }
            return;
        }

        auto l_opt = m_options.find(l_id->name);
        if (l_opt == m_options.end())
        {
            std::vector<std::string> l_known(s_builtinVars.begin(), s_builtinVars.end());
            for (const auto& l_o : m_options) l_known.push_back(l_o.first);
            l_at(*l_cmp->left, "unknown variable '" + l_id->name + "' in condition" + didYouMean(l_id->name, l_known) +
                               " (use platform, compiler, build_type or a declared option)");
        }
        else if (l_opt->second->type == "bool")
        {
            l_at(*l_cmp->left, "bool option '" + l_id->name + "' cannot be compared to a string; use 'if " + l_id->name + ":'");
        }
        return;
    }

    if (auto* l_id = std::get_if<Identifier>(&p_cond.value))
    {
        auto l_opt = m_options.find(l_id->name);
        if (l_opt == m_options.end())
        {
            if (s_builtinVars.count(l_id->name))
            {
                l_at(p_cond, "builtin '" + l_id->name + "' must be compared with == or !=");
            }
            else
            {
                std::vector<std::string> l_known;
                for (const auto& l_o : m_options) l_known.push_back(l_o.first);
                l_at(p_cond, "unknown variable '" + l_id->name + "' in condition" + didYouMean(l_id->name, l_known));
            }
        }
        else if (l_opt->second->type != "bool")
        {
            warningAt(p_cond.line ? p_cond.line : p_line, p_cond.column, l_who + "option '" + l_id->name + "' is not a bool; the condition tests whether it is set");
        }
        return;
    }

    if (std::holds_alternative<BoolLiteral>(p_cond.value)) return;

    l_at(p_cond, "unsupported condition expression");
}

void Analyzer::detectCycles()
{
    std::set<std::string> l_done;
    for (const TargetDecl& l_target : m_program.targets)
    {
        if (l_done.count(l_target.name)) continue;
        std::vector<std::string> l_stack;
        findCycle(l_target.name, l_stack, l_done);
    }
}

bool Analyzer::findCycle(const std::string& p_target, std::vector<std::string>& p_stack, std::set<std::string>& p_done)
{
    p_stack.push_back(p_target);

    auto l_it = m_targetsByName.find(p_target);
    if (l_it != m_targetsByName.end())
    {
        std::set<std::string> l_next;
        for (const Dependency& l_dep : l_it->second->dependencies) l_next.insert(basePackage(l_dep.name));

        // link entries inside configure() are real dependencies too
        std::vector<const std::vector<StmtPtr>*> l_work;
        for (const Method& l_m : l_it->second->methods) l_work.push_back(&l_m.body);
        while (!l_work.empty())
        {
            const std::vector<StmtPtr>* l_body = l_work.back();
            l_work.pop_back();
            for (const StmtPtr& l_stmt : *l_body)
            {
                if (auto* l_if = std::get_if<IfStatement>(&l_stmt->value))
                {
                    for (const IfBranch& l_b : l_if->branches) l_work.push_back(&l_b.body);
                    continue;
                }
                const Expression* l_lhs = nullptr;
                const Expression* l_rhs = nullptr;
                if (auto* l_a = std::get_if<AssignStatement>(&l_stmt->value)) { l_lhs = l_a->target.get(); l_rhs = l_a->value.get(); }
                else if (auto* l_g = std::get_if<AugAssignStatement>(&l_stmt->value)) { l_lhs = l_g->target.get(); l_rhs = l_g->value.get(); }
                if (!l_lhs) continue;
                AttrRef l_ref = decodeTarget(*l_lhs);
                auto* l_list = std::get_if<ListLiteral>(&l_rhs->value);
                if (!l_ref.valid || l_ref.name != "link" || !l_list) continue;
                for (const ExprPtr& l_e : l_list->elements)
                {
                    if (isLinkable(*l_e)) l_next.insert(basePackage(linkableName(*l_e)));
                }
            }
        }

        for (const std::string& l_dep : l_next)
        {
            if (!m_localTargetNames.count(l_dep)) continue;

            auto l_pos = std::find(p_stack.begin(), p_stack.end(), l_dep);
            if (l_pos != p_stack.end())
            {
                std::string l_chain;
                for (auto l_p = l_pos; l_p != p_stack.end(); ++l_p) l_chain += "'" + *l_p + "' -> ";
                l_chain += "'" + l_dep + "'";
                error("Circular dependency detected: " + l_chain);
                p_stack.pop_back();
                p_done.insert(p_target);
                return true;
            }
            if (!p_done.count(l_dep) && findCycle(l_dep, p_stack, p_done))
            {
                p_stack.pop_back();
                p_done.insert(p_target);
                return true;
            }
        }
    }

    p_stack.pop_back();
    p_done.insert(p_target);
    return false;
}

std::string Analyzer::basePackage(const std::string& p_name)
{
    size_t l_dot = p_name.find('.');
    if (l_dot != std::string::npos)
    {
        return p_name.substr(0, l_dot);
    }
    return p_name;
}

// Returns a normalized relative path using '/', or an empty string if the path is
// absolute, empty, or climbs out of its root with "..".
std::string Analyzer::normalizePath(const std::string& p_path)
{
    if (p_path.empty()) return "";
    if (p_path[0] == '/' || p_path[0] == '\\' || (p_path.size() > 1 && p_path[1] == ':')) return "";

    std::vector<std::string> l_parts;
    std::string l_cur;
    auto l_flush = [&]() -> bool
    {
        if (l_cur.empty() || l_cur == ".") { l_cur.clear(); return true; }
        if (l_cur == "..")
        {
            if (l_parts.empty()) return false;
            l_parts.pop_back();
        }
        else l_parts.push_back(l_cur);
        l_cur.clear();
        return true;
    };

    for (char l_c : p_path)
    {
        if (l_c == '/' || l_c == '\\') { if (!l_flush()) return ""; }
        else l_cur += l_c;
    }
    if (!l_flush()) return "";

    std::string l_out;
    for (const std::string& l_p : l_parts) l_out += (l_out.empty() ? "" : "/") + l_p;
    return l_out;
}

bool Analyzer::isValidIdentifier(const std::string& p_name)
{
    if (p_name.empty() || std::isdigit(static_cast<unsigned char>(p_name[0]))) return false;
    return std::all_of(p_name.begin(), p_name.end(), [](unsigned char p_c) { return std::isalnum(p_c) || p_c == '_'; });
}

std::string Analyzer::where(int p_line) const
{
    return p_line > 0 ? "Line " + std::to_string(p_line) + ": " : "";
}

void Analyzer::record(bool p_warning, int p_line, int p_column, const std::string& p_message)
{
    m_diagnostics.push_back({p_line, p_column, p_warning, p_message});
    std::string l_text = p_line > 0 ? "Line " + std::to_string(p_line) + ": " + p_message : p_message;
    (p_warning ? m_warnings : m_errors).push_back(l_text);
}

static void splitLinePrefix(const std::string& p_message, int& p_line, std::string& p_rest)
{
    p_line = 0;
    p_rest = p_message;
    if (p_message.rfind("Line ", 0) != 0) return;
    size_t l_end = p_message.find(": ");
    if (l_end == std::string::npos) return;
    std::string l_num = p_message.substr(5, l_end - 5);
    if (l_num.empty() || !std::all_of(l_num.begin(), l_num.end(), [](unsigned char p_c) { return std::isdigit(p_c); })) return;
    p_line = std::stoi(l_num);
    p_rest = p_message.substr(l_end + 2);
}

void Analyzer::error(const std::string& p_message)
{
    int l_line;
    std::string l_rest;
    splitLinePrefix(p_message, l_line, l_rest);
    record(false, l_line, 0, l_rest);
}

void Analyzer::warning(const std::string& p_message)
{
    int l_line;
    std::string l_rest;
    splitLinePrefix(p_message, l_line, l_rest);
    record(true, l_line, 0, l_rest);
}

void Analyzer::errorAt(int p_line, int p_column, const std::string& p_message) { record(false, p_line, p_column, p_message); }
void Analyzer::warningAt(int p_line, int p_column, const std::string& p_message) { record(true, p_line, p_column, p_message); }

} // namespace pyke
