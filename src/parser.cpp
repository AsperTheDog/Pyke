#include "parser.hpp"
#include "suggest.hpp"
#include <cctype>
#include <sstream>

namespace pyke
{

Parser::Parser(const std::vector<Token>& p_tokens)
    : m_tokens(p_tokens), m_pos(0) {}

Program Parser::parse()
{
    Program l_program;

    skipNewlines();

    while (!atEnd())
    {
        if (check(TokenType::FROM))
        {
            if (m_pos + 1 < m_tokens.size())
            {
                const std::string& l_moduleName = m_tokens[m_pos + 1].value;
                if (l_moduleName == "env")
                {
                    l_program.env_imports.push_back(parseEnvImport());
                }
                else if (l_moduleName == "github" || l_moduleName == "vendor")
                {
                    l_program.fetches.push_back(parseFetch());
                }
                else
                {
                    l_program.imports.push_back(parseImport());
                }
            }
            else
            {
                l_program.imports.push_back(parseImport());
            }
        }
        else if (check(TokenType::PROJECT))
        {
            if (l_program.project.has_value()) error("duplicate project declaration");
            l_program.project = parseProject();
        }
        else if (check(TokenType::OPTION))
        {
            l_program.options.push_back(parseOption());
        }
        else if (check(TokenType::AT))
        {
            DecoratorArgs l_args;
            TargetType l_type = parseDecorator(l_args);
            if (!hasErrors() || !atEnd())
            {
                skipNewlines();
                if (check(TokenType::TARGET))
                {
                    TargetDecl l_target = parseTarget(l_type, l_args.path);
                    l_target.sourceGroups = l_args.sourceGroups;
                    l_target.copyDlls = l_args.copyDlls;
                    l_target.test = l_args.test;
                    l_target.unityBuild = l_args.unityBuild;
                    l_program.targets.push_back(std::move(l_target));
                }
                else
                {
                    error("Expected 'target' after decorator");
                }
            }
        }
        else if (check(TokenType::IDENTIFIER) && peekAt(1).type == TokenType::EQUALS)
        {
            parseVariable();
        }
        else if (check(TokenType::IDENTIFIER) && peek().value == "cmake" && peekAt(1).type == TokenType::LEFT_PAREN)
        {
            parseRawCmake(l_program);
        }
        else if (check(TokenType::NEWLINE))
        {
            advance();
        }
        else
        {
            error("Unexpected token: " + peek().value);
            advance();
        }

        skipNewlines();
    }

    return l_program;
}

const Token& Parser::peek() const
{
    return m_tokens[m_pos];
}

const Token& Parser::previous() const
{
    return m_tokens[m_pos - 1];
}

const Token& Parser::advance()
{
    const Token& l_tok = m_tokens[m_pos];
    if (!atEnd()) m_pos++;
    return l_tok;
}

bool Parser::atEnd() const
{
    return m_pos >= m_tokens.size() || m_tokens[m_pos].type == TokenType::END_OF_FILE;
}

bool Parser::check(TokenType p_type) const
{
    if (atEnd()) return false;
    return m_tokens[m_pos].type == p_type;
}

bool Parser::match(TokenType p_type)
{
    if (check(p_type))
    {
        advance();
        return true;
    }
    return false;
}

void Parser::expect(TokenType p_type, const std::string& p_message)
{
    if (!match(p_type))
    {
        std::ostringstream l_oss;
        l_oss << "Line " << peek().line << ":" << peek().column << ": " << p_message << " (got " << tokenTypeName(peek().type) << ")";
        error(l_oss.str());
    }
}

void Parser::skipNewlines()
{
    while (!atEnd() && check(TokenType::NEWLINE))
    {
        advance();
    }
}

void Parser::skipCollectionWhitespace()
{
    while (!atEnd() && (check(TokenType::NEWLINE) || check(TokenType::INDENT) || check(TokenType::DEDENT)))
    {
        advance();
    }
}

void Parser::error(const std::string& p_message)
{
    // One error per token position: follow-up errors at the same spot are noise from the first
    if (m_hasLastError && m_lastErrorPos == m_pos) return;
    m_hasLastError = true;
    m_lastErrorPos = m_pos;
    m_syntaxErrors++;

    if (p_message.rfind("Line ", 0) == 0)
    {
        m_errors.push_back(p_message);
        return;
    }
    const Token& l_tok = peek();
    m_errors.push_back("Line " + std::to_string(l_tok.line) + ":" + std::to_string(l_tok.column) + ": " + p_message);
}

void Parser::errorAt(int p_line, int p_column, const std::string& p_message)
{
    m_errors.push_back("Line " + std::to_string(p_line) + ":" + std::to_string(p_column) + ": " + p_message);
}

const Token& Parser::peekAt(size_t p_offset) const
{
    size_t l_idx = m_pos + p_offset;
    return l_idx < m_tokens.size() ? m_tokens[l_idx] : m_tokens.back();
}

void Parser::claimName(const std::string& p_name, int p_line)
{
    if (m_variables.count(p_name))
    {
        errorAt(p_line, peek().column, "'" + p_name + "' is already a variable; names must be unique");
    }
    m_declaredNames.insert(p_name);
}

void Parser::synchronize()
{
    while (!atEnd())
    {
        if (check(TokenType::NEWLINE))
        {
            advance();
            if (check(TokenType::AT) || check(TokenType::TARGET) || check(TokenType::FROM) || check(TokenType::PROJECT) || check(TokenType::OPTION))
            {
                return;
            }
        }
        else
        {
            advance();
        }
    }
}

ImportDecl Parser::parseImport()
{
    ImportDecl l_decl;
    l_decl.line = peek().line;

    expect(TokenType::FROM, "Expected 'from'");
    expect(TokenType::IDENTIFIER, "Expected 'packages'");
    expect(TokenType::IMPORT, "Expected 'import'");

    do
    {
        if (check(TokenType::IDENTIFIER))
        {
            l_decl.packages.push_back(peek().value);
            claimName(peek().value, peek().line);
            advance();
            l_decl.specs.push_back(parsePackageSpec());
        }
        else
        {
            error("Expected package name");
            break;
        }
    } while (match(TokenType::COMMA));

    if (match(TokenType::IF))
    {
        if (check(TokenType::IDENTIFIER))
        {
            l_decl.condition = peek().value;
            advance();
        }
        else
        {
            error("Expected option name after 'if'");
        }
    }

    return l_decl;
}

// Optional `(version, optional=True, config=True)` after a package name
PackageSpec Parser::parsePackageSpec()
{
    PackageSpec l_spec;
    if (!match(TokenType::LEFT_PAREN)) return l_spec;

    bool l_first = true;
    while (!check(TokenType::RIGHT_PAREN) && !atEnd() && !check(TokenType::NEWLINE))
    {
        if (!l_first && !match(TokenType::COMMA)) break;
        l_first = false;
        if (check(TokenType::RIGHT_PAREN)) break;

        if (check(TokenType::INT_LITERAL) || check(TokenType::STRING_LITERAL))
        {
            if (!l_spec.version.empty()) error("A package takes only one version");
            if (check(TokenType::STRING_LITERAL))
            {
                l_spec.version = peek().value;
                advance();
            }
            else
            {
                l_spec.version = peek().value;
                advance();
                while (check(TokenType::DOT) && m_pos + 1 < m_tokens.size() && m_tokens[m_pos + 1].type == TokenType::INT_LITERAL)
                {
                    advance();
                    l_spec.version += "." + peek().value;
                    advance();
                }
            }
        }
        else if (check(TokenType::IDENTIFIER) && (peek().value == "optional" || peek().value == "config"))
        {
            std::string l_key = peek().value;
            advance();
            expect(TokenType::EQUALS, "Expected '=' after '" + l_key + "'");
            bool l_value = true;
            if (match(TokenType::TRUE_KW)) l_value = true;
            else if (match(TokenType::FALSE_KW)) l_value = false;
            else error("Expected True or False for '" + l_key + "'");
            (l_key == "optional" ? l_spec.optional : l_spec.config) = l_value;
        }
        else
        {
            error("Expected a version, 'optional=True' or 'config=True'");
            break;
        }
    }
    expect(TokenType::RIGHT_PAREN, "Expected ')' after the package settings");
    return l_spec;
}

EnvImport Parser::parseEnvImport()
{
    EnvImport l_decl;
    l_decl.line = peek().line;

    expect(TokenType::FROM, "Expected 'from'");
    expect(TokenType::IDENTIFIER, "Expected 'env'");
    expect(TokenType::IMPORT, "Expected 'import'");

    do
    {
        if (check(TokenType::IDENTIFIER))
        {
            l_decl.variables.push_back(peek().value);
            m_envVariables.insert(peek().value);
            claimName(peek().value, peek().line);
            advance();
        }
        else
        {
            error("Expected environment variable name");
            break;
        }
    } while (match(TokenType::COMMA));

    return l_decl;
}

FetchDecl Parser::parseFetch()
{
    FetchDecl l_decl;
    l_decl.line = peek().line;

    expect(TokenType::FROM, "Expected 'from'");
    l_decl.local = check(TokenType::IDENTIFIER) && peek().value == "vendor";
    expect(TokenType::IDENTIFIER, "Expected 'github'");
    expect(TokenType::IMPORT, "Expected 'import'");

    if (check(TokenType::STRING_LITERAL))
    {
        l_decl.repo = peek().value;
        advance();
    }
    else
    {
        error(l_decl.local ? "Expected a folder string like \"third_party/lz4\"" : "Expected repository string like \"user/repo\"");
    }

    expect(TokenType::AS, "Expected 'as'");
    if (check(TokenType::IDENTIFIER))
    {
        l_decl.name = peek().value;
        claimName(l_decl.name, peek().line);
        advance();
    }
    else
    {
        error("Expected name after 'as'");
    }

    while (match(TokenType::COMMA))
    {
        if (!check(TokenType::IDENTIFIER) || (peek().value != "tag" && peek().value != "options"))
        {
            error(l_decl.local ? "Expected 'options={...}' after the import name" : "Expected 'tag=\"...\"' or 'options={...}' after the import name");
            break;
        }
        std::string l_key = peek().value;
        if (l_decl.local && l_key == "tag")
        {
            error("Vendored folders have no 'tag'; only 'options={...}' applies");
            break;
        }
        advance();
        expect(TokenType::EQUALS, "Expected '='");
        if (l_key == "tag")
        {
            if (check(TokenType::STRING_LITERAL) || check(TokenType::FSTRING_LITERAL))
            {
                l_decl.tag = peek().value;
                advance();
            }
            else
            {
                error("Expected a string for 'tag'");
            }
        }
        else
        {
            ExprPtr l_value = parseExpression();
            auto* l_dict = l_value ? std::get_if<DictLiteral>(&l_value->value) : nullptr;
            if (!l_dict)
            {
                error("Expected a dict like {\"OPTION\": False} for 'options'");
                break;
            }
            for (auto& l_entry : l_dict->entries)
            {
                auto* l_name = std::get_if<StringLiteral>(&l_entry.first->value);
                if (!l_name)
                {
                    errorAt(l_entry.first->line, l_entry.first->column, "option names in 'options' must be strings");
                    continue;
                }
                l_decl.options.emplace_back(l_name->value, std::move(l_entry.second));
            }
        }
    }

    if (match(TokenType::IF))
    {
        if (check(TokenType::IDENTIFIER))
        {
            l_decl.condition = peek().value;
            advance();
        }
        else
        {
            error("Expected option name after 'if'");
        }
    }

    return l_decl;
}

ProjectDecl Parser::parseProject()
{
    ProjectDecl l_decl;
    l_decl.line = peek().line;

    expect(TokenType::PROJECT, "Expected 'project'");
    expect(TokenType::LEFT_PAREN, "Expected '(' after 'project'");

    if (check(TokenType::STRING_LITERAL))
    {
        l_decl.name = peek().value;
        advance();
    }
    else
    {
        error("Expected project name string");
    }

    while (match(TokenType::COMMA))
    {
        if (check(TokenType::IDENTIFIER))
        {
            std::string l_key = peek().value;
            advance();
            expect(TokenType::EQUALS, "Expected '=' after keyword");
            if (l_key == "lang" && check(TokenType::LEFT_BRACKET))
            {
                advance();
                while (check(TokenType::STRING_LITERAL))
                {
                    l_decl.langs.push_back(peek().value);
                    advance();
                    if (!match(TokenType::COMMA)) break;
                }
                expect(TokenType::RIGHT_BRACKET, "Expected ']' after lang list");
            }
            else if (check(TokenType::STRING_LITERAL))
            {
                std::string l_val = peek().value;
                advance();
                if (l_key == "version") l_decl.version = l_val;
                else if (l_key == "lang") l_decl.langs.push_back(l_val);
                else if (l_key == "output_dir") l_decl.outputDir = l_val;
                else error("Unknown project keyword: " + l_key);
            }
            else if (check(TokenType::TRUE_KW) || check(TokenType::FALSE_KW))
            {
                bool l_val = check(TokenType::TRUE_KW);
                advance();
                if (l_key == "presets") l_decl.presets = l_val;
                else if (l_key == "import_std") l_decl.importStd = l_val;
                else error("Unknown project keyword: " + l_key);
            }
            else
            {
                error("Expected string value for " + l_key);
            }
        }
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after project declaration");
    return l_decl;
}

OptionDecl Parser::parseOption()
{
    OptionDecl l_decl;
    l_decl.line = peek().line;

    expect(TokenType::OPTION, "Expected 'option'");

    if (check(TokenType::IDENTIFIER))
    {
        l_decl.name = peek().value;
        claimName(l_decl.name, peek().line);
        m_optionNames.insert(l_decl.name);
        advance();
    }
    else
    {
        error("Expected option name");
    }

    expect(TokenType::COLON, "Expected ':' after option name");

    if (check(TokenType::BOOL_TYPE))
    {
        l_decl.type = "bool";
        advance();
    }
    else if (check(TokenType::STR_TYPE))
    {
        l_decl.type = "str";
        advance();
    }
    else if (check(TokenType::PATH_TYPE))
    {
        l_decl.type = "path";
        advance();
    }
    else
    {
        error("Expected type (bool, str, path) after ':'");
    }

    expect(TokenType::EQUALS, "Expected '=' for default value");
    l_decl.defaultValue = parseExpression();

    return l_decl;
}

TargetType Parser::parseDecorator(DecoratorArgs& p_args)
{
    expect(TokenType::AT, "Expected '@'");

    TargetType l_type = TargetType::EXECUTABLE;

    if (match(TokenType::EXECUTABLE))          l_type = TargetType::EXECUTABLE;
    else if (match(TokenType::SHARED_LIBRARY)) l_type = TargetType::SHARED_LIBRARY;
    else if (match(TokenType::STATIC_LIBRARY)) l_type = TargetType::STATIC_LIBRARY;
    else if (match(TokenType::HEADER_ONLY))    l_type = TargetType::HEADER_ONLY;
    else error("Expected target type after '@'");

    p_args.copyDlls = (l_type == TargetType::EXECUTABLE);

    if (match(TokenType::LEFT_PAREN))
    {
        if (check(TokenType::STRING_LITERAL))
        {
            p_args.path = peek().value;
            advance();
        }

        while (match(TokenType::COMMA) || check(TokenType::IDENTIFIER) || check(TokenType::PATH_TYPE))
        {
            if (check(TokenType::IDENTIFIER) || check(TokenType::PATH_TYPE))
            {
                std::string l_key = peek().value;
                advance();
                expect(TokenType::EQUALS, "Expected '=' after keyword in decorator");

                const auto l_parseBool = [&](bool& p_out)
                {
                    if (match(TokenType::TRUE_KW)) p_out = true;
                    else if (match(TokenType::FALSE_KW)) p_out = false;
                    else error("Expected True or False for '" + l_key + "'");
                };

                if (l_key == "path")
                {
                    if (check(TokenType::STRING_LITERAL))
                    {
                        p_args.path = peek().value;
                        advance();
                    }
                    else
                    {
                        error("Expected string for 'path'");
                    }
                }
                else if (l_key == "source_groups")
                {
                    l_parseBool(p_args.sourceGroups);
                }
                else if (l_key == "copy_dlls")
                {
                    l_parseBool(p_args.copyDlls);
                }
                else if (l_key == "test")
                {
                    l_parseBool(p_args.test);
                }
                else if (l_key == "unity_build")
                {
                    l_parseBool(p_args.unityBuild);
                }
                else
                {
                    error("Unknown decorator keyword: " + l_key);
                }
            }
        }

        expect(TokenType::RIGHT_PAREN, "Expected ')' after decorator arguments");
    }

    return l_type;
}

TargetDecl Parser::parseTarget(TargetType p_type, const std::string& p_path)
{
    TargetDecl l_decl;
    l_decl.type = p_type;
    l_decl.path = p_path;
    l_decl.line = peek().line;

    expect(TokenType::TARGET, "Expected 'target'");

    if (check(TokenType::IDENTIFIER))
    {
        l_decl.name = peek().value;
        l_decl.column = peek().column;
        claimName(l_decl.name, peek().line);
        m_currentTarget = l_decl.name;
        advance();
    }
    else
    {
        error("Expected target name");
    }

    expect(TokenType::LEFT_PAREN, "Expected '(' after target name");
    l_decl.dependencies = parseDependencies();
    expect(TokenType::RIGHT_PAREN, "Expected ')' after dependencies");

    expect(TokenType::COLON, "Expected ':' after target declaration");
    skipNewlines();
    expect(TokenType::INDENT, "Expected indented block after target declaration");

    while (!atEnd() && !check(TokenType::DEDENT))
    {
        skipNewlines();
        if (check(TokenType::DEDENT) || atEnd()) break;

        if (check(TokenType::DEF))
        {
            l_decl.methods.push_back(parseMethod());
        }
        else
        {
            error("Expected method definition (def configure/install) inside target");
            synchronize();
        }
        skipNewlines();
    }

    if (match(TokenType::DEDENT))
    {
    }

    m_currentTarget.clear();
    return l_decl;
}

std::vector<Dependency> Parser::parseDependencies()
{
    std::vector<Dependency> l_deps;

    if (check(TokenType::RIGHT_PAREN)) return l_deps;

    do
    {
        Dependency l_dep;
        l_dep.visibility = "PRIVATE";

        if (match(TokenType::PUBLIC_KW))
        {
            l_dep.visibility = "PUBLIC";
        }
        else if (match(TokenType::PRIVATE_KW))
        {
            l_dep.visibility = "PRIVATE";
        }

        if (check(TokenType::IDENTIFIER))
        {
            l_dep.name = peek().value;
            l_dep.line = peek().line;
            l_dep.column = peek().column;
            advance();

            while (match(TokenType::DOT))
            {
                if (check(TokenType::IDENTIFIER))
                {
                    l_dep.name += "." + peek().value;
                    advance();
                }
            }
        }
        else
        {
            error("Expected dependency name");
            break;
        }

        l_deps.push_back(std::move(l_dep));
    } while (match(TokenType::COMMA));

    return l_deps;
}

Method Parser::parseMethod()
{
    Method l_method;
    l_method.line = peek().line;

    expect(TokenType::DEF, "Expected 'def'");

    if (check(TokenType::IDENTIFIER))
    {
        l_method.name = peek().value;
        advance();
    }
    else
    {
        error("Expected method name");
    }

    expect(TokenType::LEFT_PAREN, "Expected '('");
    expect(TokenType::SELF_, "Expected 'self'");
    expect(TokenType::RIGHT_PAREN, "Expected ')'");
    expect(TokenType::COLON, "Expected ':'");
    skipNewlines();
    expect(TokenType::INDENT, "Expected indented block after method definition");

    while (!atEnd() && !check(TokenType::DEDENT))
    {
        skipNewlines();
        if (check(TokenType::DEDENT) || atEnd()) break;

        StmtPtr l_stmt = parseStatement();
        if (l_stmt)
        {
            l_method.body.push_back(std::move(l_stmt));
        }
        skipNewlines();
    }

    if (match(TokenType::DEDENT))
    {
    }

    return l_method;
}

StmtPtr Parser::parseStatement()
{
    if (check(TokenType::IF))
    {
        return parseIfStatement();
    }
    return parseAssignmentOrAugAssign();
}

StmtPtr Parser::parseAssignmentOrAugAssign()
{
    int l_line = peek().line;
    int l_col = peek().column;

    ExprPtr l_target = parseExpression();

    if (match(TokenType::EQUALS))
    {
        ExprPtr l_value = parseExpression();
        AssignStatement l_assign;
        l_assign.target = std::move(l_target);
        l_assign.value = std::move(l_value);
        return makeStmt(l_line, l_col, std::move(l_assign));
    }
    else if (match(TokenType::PLUS_EQUALS))
    {
        ExprPtr l_value = parseExpression();
        AugAssignStatement l_aug;
        l_aug.target = std::move(l_target);
        l_aug.value = std::move(l_value);
        return makeStmt(l_line, l_col, std::move(l_aug));
    }

    error("Expected '=' or '+=' in statement");
    return nullptr;
}

StmtPtr Parser::parseIfStatement()
{
    int l_line = peek().line;
    int l_col = peek().column;

    IfStatement l_ifStmt;

    expect(TokenType::IF, "Expected 'if'");
    ExprPtr l_condition = parseCondition();
    expect(TokenType::COLON, "Expected ':' after if condition");
    skipNewlines();
    expect(TokenType::INDENT, "Expected indented block after if");

    IfBranch l_ifBranch;
    l_ifBranch.condition = std::move(l_condition);

    while (!atEnd() && !check(TokenType::DEDENT))
    {
        skipNewlines();
        if (check(TokenType::DEDENT) || atEnd()) break;
        StmtPtr l_stmt = parseStatement();
        if (l_stmt) l_ifBranch.body.push_back(std::move(l_stmt));
        skipNewlines();
    }
    match(TokenType::DEDENT);
    l_ifStmt.branches.push_back(std::move(l_ifBranch));

    while (check(TokenType::ELIF))
    {
        advance();
        ExprPtr l_elifCond = parseCondition();
        expect(TokenType::COLON, "Expected ':' after elif condition");
        skipNewlines();
        expect(TokenType::INDENT, "Expected indented block after elif");

        IfBranch l_elifBranch;
        l_elifBranch.condition = std::move(l_elifCond);

        while (!atEnd() && !check(TokenType::DEDENT))
        {
            skipNewlines();
            if (check(TokenType::DEDENT) || atEnd()) break;
            StmtPtr l_stmt = parseStatement();
            if (l_stmt) l_elifBranch.body.push_back(std::move(l_stmt));
            skipNewlines();
        }
        match(TokenType::DEDENT);
        l_ifStmt.branches.push_back(std::move(l_elifBranch));
    }

    if (check(TokenType::ELSE))
    {
        advance();
        expect(TokenType::COLON, "Expected ':' after else");
        skipNewlines();
        expect(TokenType::INDENT, "Expected indented block after else");

        IfBranch l_elseBranch;

        while (!atEnd() && !check(TokenType::DEDENT))
        {
            skipNewlines();
            if (check(TokenType::DEDENT) || atEnd()) break;
            StmtPtr l_stmt = parseStatement();
            if (l_stmt) l_elseBranch.body.push_back(std::move(l_stmt));
            skipNewlines();
        }
        match(TokenType::DEDENT);
        l_ifStmt.branches.push_back(std::move(l_elseBranch));
    }

    return makeStmt(l_line, l_col, std::move(l_ifStmt));
}

// ---- conditions: `or` < `and` < `not` < comparison / bool option / ( group ) ----

ExprPtr Parser::parseCondition()
{
    return parseOrCondition();
}

ExprPtr Parser::parseOrCondition()
{
    ExprPtr l_left = parseAndCondition();
    while (check(TokenType::OR))
    {
        int l_line = peek().line;
        int l_col = peek().column;
        advance();
        ExprPtr l_right = parseAndCondition();
        l_left = makeExpr(l_line, l_col, BoolOp{"or", std::move(l_left), std::move(l_right)});
    }
    return l_left;
}

ExprPtr Parser::parseAndCondition()
{
    ExprPtr l_left = parseNotCondition();
    while (check(TokenType::AND))
    {
        int l_line = peek().line;
        int l_col = peek().column;
        advance();
        ExprPtr l_right = parseNotCondition();
        l_left = makeExpr(l_line, l_col, BoolOp{"and", std::move(l_left), std::move(l_right)});
    }
    return l_left;
}

ExprPtr Parser::parseNotCondition()
{
    int l_line = peek().line;
    int l_col = peek().column;

    if (match(TokenType::NOT))
    {
        ExprPtr l_operand = parseNotCondition();
        return makeExpr(l_line, l_col, NotExpr{std::move(l_operand)});
    }
    if (match(TokenType::LEFT_PAREN))
    {
        ExprPtr l_inner = parseOrCondition();
        expect(TokenType::RIGHT_PAREN, "Expected ')' to close the grouped condition");
        return l_inner;
    }
    return parseExpression();
}

// ---- top-level variables and cmake() ----

void Parser::parseVariable()
{
    const Token l_nameTok = peek();
    const std::string& l_name = l_nameTok.value;
    advance();
    expect(TokenType::EQUALS, "Expected '='");

    ExprPtr l_value = parseExpression();

    if (l_name == "platform" || l_name == "compiler" || l_name == "build_type")
    {
        errorAt(l_nameTok.line, l_nameTok.column, "'" + l_name + "' is a builtin and cannot be used as a variable name");
        return;
    }
    if (m_variables.count(l_name))
    {
        errorAt(l_nameTok.line, l_nameTok.column, "variable '" + l_name + "' is already defined (variables are constants; use a new name)");
        return;
    }
    if (m_declaredNames.count(l_name) || m_envVariables.count(l_name))
    {
        errorAt(l_nameTok.line, l_nameTok.column, "'" + l_name + "' is already used by a package, option, github import or target");
        return;
    }
    m_variables.emplace(l_name, std::move(l_value));
}

void Parser::parseRawCmake(Program& p_program)
{
    advance(); // cmake
    expect(TokenType::LEFT_PAREN, "Expected '(' after 'cmake'");
    skipCollectionWhitespace();

    bool l_any = false;
    while (!check(TokenType::RIGHT_PAREN) && !atEnd())
    {
        int l_line = peek().line;
        int l_col = peek().column;
        ExprPtr l_arg = parseExpression();
        if (auto* l_str = std::get_if<StringLiteral>(&l_arg->value))
        {
            p_program.rawCmake.push_back({l_str->value, l_line});
            l_any = true;
        }
        else
        {
            errorAt(l_line, l_col, "cmake() expects string arguments");
        }
        skipCollectionWhitespace();
        if (!match(TokenType::COMMA)) break;
        skipCollectionWhitespace();
    }
    if (!l_any) error("cmake() needs at least one string");
    expect(TokenType::RIGHT_PAREN, "Expected ')' after cmake(...)");
}

// ---- f-strings: resolved here, so later stages only ever see plain string literals ----

bool Parser::stringifyForFString(const Expression& p_value, std::string& p_out) const
{
    if (auto* l_s = std::get_if<StringLiteral>(&p_value.value)) { p_out = l_s->value; return true; }
    if (auto* l_i = std::get_if<IntLiteral>(&p_value.value)) { p_out = std::to_string(l_i->value); return true; }
    if (auto* l_b = std::get_if<BoolLiteral>(&p_value.value)) { p_out = l_b->value ? "ON" : "OFF"; return true; }
    if (auto* l_e = std::get_if<EnvVariable>(&p_value.value)) { p_out = "$ENV{" + l_e->name + "}"; return true; }
    if (auto* l_id = std::get_if<Identifier>(&p_value.value)) { p_out = l_id->name; return true; }
    if (auto* l_list = std::get_if<ListLiteral>(&p_value.value))
    {
        p_out.clear();
        for (const ExprPtr& l_elem : l_list->elements)
        {
            std::string l_part;
            if (!stringifyForFString(*l_elem, l_part)) return false;
            if (!p_out.empty()) p_out += " ";
            p_out += l_part;
        }
        return true;
    }
    return false;
}

ExprPtr Parser::resolveFString(const Token& p_token)
{
    const std::string& l_src = p_token.value;
    std::string l_out;
    const int l_baseCol = p_token.column + 2; // skip f"

    auto l_fail = [&](size_t p_at, const std::string& p_msg)
    {
        errorAt(p_token.line, l_baseCol + static_cast<int>(p_at), p_msg);
    };

    for (size_t l_i = 0; l_i < l_src.size(); l_i++)
    {
        char l_c = l_src[l_i];

        // ${VAR}, $ENV{VAR}, $CACHE{VAR}: CMake syntax, never interpolation
        if (l_c == '$')
        {
            size_t l_j = l_i + 1;
            while (l_j < l_src.size() && (std::isalpha(static_cast<unsigned char>(l_src[l_j])) || l_src[l_j] == '_')) l_j++;
            if (l_j < l_src.size() && l_src[l_j] == '{')
            {
                size_t l_close = l_src.find('}', l_j);
                if (l_close == std::string::npos) { l_fail(l_i, "unclosed '{' after '$'"); return makeExpr(p_token.line, p_token.column, StringLiteral{l_out}); }
                l_out += l_src.substr(l_i, l_close - l_i + 1);
                l_i = l_close;
                continue;
            }
            l_out += l_c;
            continue;
        }

        if (l_c == '{')
        {
            if (l_i + 1 < l_src.size() && l_src[l_i + 1] == '{') { l_out += '{'; l_i++; continue; }

            size_t l_close = l_src.find('}', l_i);
            if (l_close == std::string::npos) { l_fail(l_i, "unclosed '{' in f-string (write '{{' for a literal brace)"); break; }

            std::string l_name = l_src.substr(l_i + 1, l_close - l_i - 1);
            size_t l_a = l_name.find_first_not_of(' ');
            size_t l_b = l_name.find_last_not_of(' ');
            l_name = (l_a == std::string::npos) ? "" : l_name.substr(l_a, l_b - l_a + 1);
            size_t l_at = l_i + 1;

            if (l_name.empty()) { l_fail(l_at, "empty '{}' in f-string"); }
            else if (l_name == "self.name")
            {
                if (m_currentTarget.empty()) l_fail(l_at, "{self.name} can only be used inside a target");
                else l_out += m_currentTarget;
            }
            else if (m_variables.count(l_name))
            {
                std::string l_text;
                if (stringifyForFString(*m_variables.at(l_name), l_text)) l_out += l_text;
                else l_fail(l_at, "variable '" + l_name + "' cannot be used in a string (only strings, numbers, bools and lists of them)");
            }
            else if (m_optionNames.count(l_name)) l_out += "${" + l_name + "}";
            else if (m_envVariables.count(l_name)) l_out += "$ENV{" + l_name + "}";
            else if (l_name == "platform" || l_name == "compiler" || l_name == "build_type")
            {
                l_fail(l_at, "builtin '" + l_name + "' can only be used in conditions");
            }
            else
            {
                std::vector<std::string> l_candidates = {"self.name"};
                for (const auto& l_v : m_variables) l_candidates.push_back(l_v.first);
                for (const std::string& l_o : m_optionNames) l_candidates.push_back(l_o);
                for (const std::string& l_e : m_envVariables) l_candidates.push_back(l_e);
                std::string l_hint = didYouMean(l_name, l_candidates);
                l_fail(l_at, "unknown name '" + l_name + "' in f-string" + (l_hint.empty() ? " (variables and options must be declared before use)" : l_hint));
            }
            l_i = l_close;
            continue;
        }

        if (l_c == '}')
        {
            if (l_i + 1 < l_src.size() && l_src[l_i + 1] == '}') { l_out += '}'; l_i++; continue; }
            l_fail(l_i, "single '}' in f-string (write '}}' for a literal brace)");
            continue;
        }

        l_out += l_c;
    }

    return makeExpr(p_token.line, p_token.column, StringLiteral{l_out});
}


ExprPtr Parser::parseExpression()
{
    ExprPtr l_left = parsePrimary();
    l_left = parsePostfix(std::move(l_left));

    while (check(TokenType::PLUS))
    {
        int l_line = peek().line;
        int l_col = peek().column;
        advance();

        ExprPtr l_right = parsePrimary();
        l_right = parsePostfix(std::move(l_right));

        auto* l_leftList = std::get_if<ListLiteral>(&l_left->value);
        auto* l_rightList = std::get_if<ListLiteral>(&l_right->value);
        auto* l_leftDict = std::get_if<DictLiteral>(&l_left->value);
        auto* l_rightDict = std::get_if<DictLiteral>(&l_right->value);
        if (l_leftList && l_rightList)
        {
            // list + list is folded right away so variables compose: base_flags + ["-Werror"]
            for (ExprPtr& l_e : l_rightList->elements) l_leftList->elements.push_back(std::move(l_e));
            continue;
        }
        if (l_leftDict && l_rightDict)
        {
            for (auto& l_entry : l_rightDict->entries) l_leftDict->entries.push_back(std::move(l_entry));
            continue;
        }

        StringConcat l_concat;
        l_concat.left = std::move(l_left);
        l_concat.right = std::move(l_right);
        l_left = makeExpr(l_line, l_col, std::move(l_concat));
    }

    if (check(TokenType::COMPARISON))
    {
        int l_line = peek().line;
        int l_col = peek().column;
        std::string l_op = peek().value;
        advance();

        ExprPtr l_right = parsePrimary();
        l_right = parsePostfix(std::move(l_right));

        Comparison l_cmp;
        l_cmp.left = std::move(l_left);
        l_cmp.op = l_op;
        l_cmp.right = std::move(l_right);
        return makeExpr(l_line, l_col, std::move(l_cmp));
    }

    return l_left;
}

ExprPtr Parser::parsePrimary()
{
    int l_line = peek().line;
    int l_col = peek().column;

    if (check(TokenType::STRING_LITERAL))
    {
        std::string l_val = peek().value;
        advance();
        return makeExpr(l_line, l_col, StringLiteral{l_val});
    }

    if (check(TokenType::FSTRING_LITERAL))
    {
        Token l_tok = peek();
        advance();
        return resolveFString(l_tok);
    }

    if (check(TokenType::INT_LITERAL))
    {
        int l_val = std::stoi(peek().value);
        advance();
        return makeExpr(l_line, l_col, IntLiteral{l_val});
    }

    if (check(TokenType::TRUE_KW))
    {
        advance();
        return makeExpr(l_line, l_col, BoolLiteral{true});
    }

    if (check(TokenType::FALSE_KW))
    {
        advance();
        return makeExpr(l_line, l_col, BoolLiteral{false});
    }

    if (check(TokenType::SELF_))
    {
        advance();
        return makeExpr(l_line, l_col, Identifier{"self"});
    }

    if (check(TokenType::IDENTIFIER))
    {
        std::string l_name = peek().value;
        advance();
        auto l_var = m_variables.find(l_name);
        if (l_var != m_variables.end())
        {
            ExprPtr l_copy = cloneExpr(*l_var->second);
            l_copy->line = l_line;
            l_copy->column = l_col;
            return l_copy;
        }
        if (m_envVariables.count(l_name))
        {
            return makeExpr(l_line, l_col, EnvVariable{l_name});
        }
        return makeExpr(l_line, l_col, Identifier{l_name});
    }

    if (check(TokenType::LEFT_BRACKET))
    {
        return parseList();
    }

    if (check(TokenType::LEFT_BRACE))
    {
        return parseDictOrSet();
    }

    if (check(TokenType::LEFT_PAREN))
    {
        return parseTupleOrParen();
    }

    error("Unexpected token in expression: " + std::string(tokenTypeName(peek().type)));
    advance();
    return makeExpr(l_line, l_col, Identifier{"<error>"});
}

ExprPtr Parser::parsePostfix(ExprPtr p_left)
{
    while (true)
    {
        if (check(TokenType::DOT))
        {
            int l_line = peek().line;
            int l_col = peek().column;
            advance();

            if (check(TokenType::IDENTIFIER))
            {
                std::string l_member = peek().value;
                advance();
                DotAccess l_dot;
                l_dot.object = std::move(p_left);
                l_dot.member = l_member;
                p_left = makeExpr(l_line, l_col, std::move(l_dot));
            }
            else
            {
                error("Expected identifier after '.'");
                break;
            }
        }
        else if (check(TokenType::LEFT_BRACKET))
        {
            int l_line = peek().line;
            int l_col = peek().column;
            advance();

            ExprPtr l_index = parseExpression();
            expect(TokenType::RIGHT_BRACKET, "Expected ']' after index");

            IndexAccess l_idx;
            l_idx.object = std::move(p_left);
            l_idx.index = std::move(l_index);
            p_left = makeExpr(l_line, l_col, std::move(l_idx));
        }
        else
        {
            break;
        }
    }
    return p_left;
}

ExprPtr Parser::parseList()
{
    int l_line = peek().line;
    int l_col = peek().column;

    expect(TokenType::LEFT_BRACKET, "Expected '['");
    skipCollectionWhitespace();

    ListLiteral l_list;
    if (!check(TokenType::RIGHT_BRACKET))
    {
        l_list.elements.push_back(parseExpression());
        skipCollectionWhitespace();
        while (match(TokenType::COMMA))
        {
            skipCollectionWhitespace();
            if (check(TokenType::RIGHT_BRACKET)) break;
            l_list.elements.push_back(parseExpression());
            skipCollectionWhitespace();
        }
    }

    expect(TokenType::RIGHT_BRACKET, "Expected ']'");
    return makeExpr(l_line, l_col, std::move(l_list));
}

ExprPtr Parser::parseDictOrSet()
{
    int l_line = peek().line;
    int l_col = peek().column;

    expect(TokenType::LEFT_BRACE, "Expected '{'");
    skipCollectionWhitespace();

    DictLiteral l_dict;
    if (!check(TokenType::RIGHT_BRACE))
    {
        ExprPtr l_key = parseExpression();
        expect(TokenType::COLON, "Expected ':' in dict entry");
        ExprPtr l_val = parseExpression();
        l_dict.entries.push_back({std::move(l_key), std::move(l_val)});
        skipCollectionWhitespace();

        while (match(TokenType::COMMA))
        {
            skipCollectionWhitespace();
            if (check(TokenType::RIGHT_BRACE)) break;
            ExprPtr l_k = parseExpression();
            expect(TokenType::COLON, "Expected ':' in dict entry");
            ExprPtr l_v = parseExpression();
            l_dict.entries.push_back({std::move(l_k), std::move(l_v)});
            skipCollectionWhitespace();
        }
    }

    expect(TokenType::RIGHT_BRACE, "Expected '}'");
    return makeExpr(l_line, l_col, std::move(l_dict));
}

ExprPtr Parser::parseTupleOrParen()
{
    int l_line = peek().line;
    int l_col = peek().column;

    expect(TokenType::LEFT_PAREN, "Expected '('");
    skipCollectionWhitespace();

    std::vector<ExprPtr> l_elements;
    if (!check(TokenType::RIGHT_PAREN))
    {
        l_elements.push_back(parseExpression());
        skipCollectionWhitespace();
        while (match(TokenType::COMMA))
        {
            skipCollectionWhitespace();
            if (check(TokenType::RIGHT_PAREN)) break;
            l_elements.push_back(parseExpression());
            skipCollectionWhitespace();
        }
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')'");

    if (l_elements.size() == 1)
    {
        return std::move(l_elements[0]);
    }

    TupleLiteral l_tuple;
    l_tuple.elements = std::move(l_elements);
    return makeExpr(l_line, l_col, std::move(l_tuple));
}

} // namespace pyke
