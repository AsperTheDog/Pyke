#include "parser.hpp"
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
                else if (l_moduleName == "github")
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
    m_errors.push_back(p_message);
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
            advance();
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
    expect(TokenType::IDENTIFIER, "Expected 'github'");
    expect(TokenType::IMPORT, "Expected 'import'");

    if (check(TokenType::STRING_LITERAL))
    {
        l_decl.repo = peek().value;
        advance();
    }
    else
    {
        error("Expected repository string like \"user/repo\"");
    }

    expect(TokenType::AS, "Expected 'as'");
    if (check(TokenType::IDENTIFIER))
    {
        l_decl.name = peek().value;
        advance();
    }
    else
    {
        error("Expected name after 'as'");
    }

    if (match(TokenType::COMMA))
    {
        if (check(TokenType::IDENTIFIER) && peek().value == "tag")
        {
            advance();
            expect(TokenType::EQUALS, "Expected '='");
            if (check(TokenType::STRING_LITERAL))
            {
                l_decl.tag = peek().value;
                advance();
            }
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
            if (check(TokenType::STRING_LITERAL))
            {
                std::string l_val = peek().value;
                advance();
                if (l_key == "version") l_decl.version = l_val;
                else if (l_key == "lang") l_decl.lang = l_val;
                else if (l_key == "output_dir") l_decl.outputDir = l_val;
                else error("Unknown project keyword: " + l_key);
            }
            else if (check(TokenType::TRUE_KW) || check(TokenType::FALSE_KW))
            {
                bool l_val = check(TokenType::TRUE_KW);
                advance();
                if (l_key == "presets") l_decl.presets = l_val;
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
    ExprPtr l_condition = parseExpression();
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
        ExprPtr l_elifCond = parseExpression();
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
