// These unit tests were automatically generated using AI

#include "lexer.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

static int s_testsRun = 0;
static int s_testsPassed = 0;
static int s_testsFailed = 0;

struct TestFailure
{
    std::string message;
    std::string file;
    int line;
};

static std::vector<TestFailure> s_failures;

template<typename T>
std::string toStr(const T& p_val)
{
    std::ostringstream l_oss;
    l_oss << p_val;
    return l_oss.str();
}

template<>
std::string toStr<pyke::TokenType>(const pyke::TokenType& p_val)
{
    return std::string(pyke::tokenTypeName(p_val));
}

#define ASSERT_EQ(actual, expected, msg) \
    do { \
        if ((actual) != (expected)) { \
            std::ostringstream oss; \
            oss << msg << ": expected '" << toStr(expected) << "', got '" << toStr(actual) << "'"; \
            s_failures.push_back({oss.str(), __FILE__, __LINE__}); \
            s_testsFailed++; \
            return; \
        } \
    } while(0)

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            s_failures.push_back({msg, __FILE__, __LINE__}); \
            s_testsFailed++; \
            return; \
        } \
    } while(0)

#define RUN_TEST(fn) \
    do { \
        s_testsRun++; \
        int old_failed = s_testsFailed; \
        fn(); \
        if (s_testsFailed == old_failed) { \
            s_testsPassed++; \
            std::cout << "  PASS: " << #fn << std::endl; \
        } else { \
            std::cout << "  FAIL: " << #fn << std::endl; \
        } \
    } while(0)

static std::vector<pyke::Token> contentTokens(const std::vector<pyke::Token>& p_tokens)
{
    std::vector<pyke::Token> l_result;
    for (const pyke::Token& l_t : p_tokens)
    {
        if (l_t.type != pyke::TokenType::NEWLINE && l_t.type != pyke::TokenType::INDENT && l_t.type != pyke::TokenType::DEDENT && l_t.type != pyke::TokenType::END_OF_FILE)
        {
            l_result.push_back(l_t);
        }
    }
    return l_result;
}

void test_empty_input()
{
    pyke::Lexer l_lexer("");
    std::vector<pyke::Token> l_tokens = l_lexer.tokenize();
    ASSERT_EQ(l_tokens.size(), size_t(1), "Empty input should produce only EOF");
    ASSERT_EQ(l_tokens[0].type, pyke::TokenType::END_OF_FILE, "Token should be EOF");
}

void test_single_keyword_target()
{
    pyke::Lexer l_lexer("target");
    std::vector<pyke::Token> l_tokens = contentTokens(l_lexer.tokenize());
    ASSERT_EQ(l_tokens.size(), size_t(1), "Should have one content token");
    ASSERT_EQ(l_tokens[0].type, pyke::TokenType::TARGET, "Should be Target keyword");
    ASSERT_EQ(l_tokens[0].value, std::string("target"), "Value should be 'target'");
}

void test_all_keywords()
{
    pyke::Lexer l_lexer("from import project target def self if elif else option True False PUBLIC PRIVATE bool str path Executable SharedLibrary StaticLibrary HeaderOnly");
    std::vector<pyke::Token> l_tokens = contentTokens(l_lexer.tokenize());

    std::vector<pyke::TokenType> l_expected = {
        pyke::TokenType::FROM,
        pyke::TokenType::IMPORT,
        pyke::TokenType::PROJECT,
        pyke::TokenType::TARGET,
        pyke::TokenType::DEF,
        pyke::TokenType::SELF_,
        pyke::TokenType::IF,
        pyke::TokenType::ELIF,
        pyke::TokenType::ELSE,
        pyke::TokenType::OPTION,
        pyke::TokenType::TRUE_KW,
        pyke::TokenType::FALSE_KW,
        pyke::TokenType::PUBLIC_KW,
        pyke::TokenType::PRIVATE_KW,
        pyke::TokenType::BOOL_TYPE,
        pyke::TokenType::STR_TYPE,
        pyke::TokenType::PATH_TYPE,
        pyke::TokenType::EXECUTABLE,
        pyke::TokenType::SHARED_LIBRARY,
        pyke::TokenType::STATIC_LIBRARY,
        pyke::TokenType::HEADER_ONLY,
    };

    ASSERT_EQ(l_tokens.size(), l_expected.size(), "Keyword count mismatch");
    for (size_t l_i = 0; l_i < l_expected.size(); l_i++)
    {
        ASSERT_EQ(l_tokens[l_i].type, l_expected[l_i], std::string("Keyword mismatch at index ") + std::to_string(l_i));
    }
}

void test_identifier()
{
    pyke::Lexer l_lexer("MyApp foo_bar baz123");
    std::vector<pyke::Token> l_tokens = contentTokens(l_lexer.tokenize());
    ASSERT_EQ(l_tokens.size(), size_t(3), "Should have 3 identifiers");
    ASSERT_EQ(l_tokens[0].type, pyke::TokenType::IDENTIFIER, "First should be Identifier");
    ASSERT_EQ(l_tokens[0].value, std::string("MyApp"), "First value");
    ASSERT_EQ(l_tokens[1].value, std::string("foo_bar"), "Second value");
    ASSERT_EQ(l_tokens[2].value, std::string("baz123"), "Third value");
}

void test_string_literal()
{
    pyke::Lexer l_lexer("\"hello world\"");
    std::vector<pyke::Token> l_tokens = contentTokens(l_lexer.tokenize());
    ASSERT_EQ(l_tokens.size(), size_t(1), "Should have 1 string");
    ASSERT_EQ(l_tokens[0].type, pyke::TokenType::STRING_LITERAL, "Should be StringLiteral");
    ASSERT_EQ(l_tokens[0].value, std::string("hello world"), "String content");
}

void test_string_escape_sequences()
{
    pyke::Lexer l_lexer("\"hello\\nworld\" \"tab\\there\" \"quote\\\"end\"");
    std::vector<pyke::Token> l_tokens = contentTokens(l_lexer.tokenize());
    ASSERT_EQ(l_tokens.size(), size_t(3), "Should have 3 strings");
    ASSERT_EQ(l_tokens[0].value, std::string("hello\nworld"), "Newline escape");
    ASSERT_EQ(l_tokens[1].value, std::string("tab\there"), "Tab escape");
    ASSERT_EQ(l_tokens[2].value, std::string("quote\"end"), "Quote escape");
}

void test_unterminated_string()
{
    pyke::Lexer l_lexer("\"hello");
    std::vector<pyke::Token> l_tokens = contentTokens(l_lexer.tokenize());
    ASSERT_EQ(l_tokens.size(), size_t(1), "Should have 1 token");
    ASSERT_EQ(l_tokens[0].type, pyke::TokenType::ERROR_TOKEN, "Should be Error");
}

void test_int_literal()
{
    pyke::Lexer l_lexer("42 0 123");
    std::vector<pyke::Token> l_tokens = contentTokens(l_lexer.tokenize());
    ASSERT_EQ(l_tokens.size(), size_t(3), "Should have 3 numbers");
    ASSERT_EQ(l_tokens[0].type, pyke::TokenType::INT_LITERAL, "Should be IntLiteral");
    ASSERT_EQ(l_tokens[0].value, std::string("42"), "First number");
    ASSERT_EQ(l_tokens[2].value, std::string("123"), "Third number");
}

void test_operators()
{
    pyke::Lexer l_lexer("= += : , . ( ) [ ] { } ==");
    std::vector<pyke::Token> l_tokens = contentTokens(l_lexer.tokenize());

    std::vector<pyke::TokenType> l_expected = {
        pyke::TokenType::EQUALS,
        pyke::TokenType::PLUS_EQUALS,
        pyke::TokenType::COLON,
        pyke::TokenType::COMMA,
        pyke::TokenType::DOT,
        pyke::TokenType::LEFT_PAREN,
        pyke::TokenType::RIGHT_PAREN,
        pyke::TokenType::LEFT_BRACKET,
        pyke::TokenType::RIGHT_BRACKET,
        pyke::TokenType::LEFT_BRACE,
        pyke::TokenType::RIGHT_BRACE,
        pyke::TokenType::COMPARISON,
    };

    ASSERT_EQ(l_tokens.size(), l_expected.size(), "Operator count mismatch");
    for (size_t l_i = 0; l_i < l_expected.size(); l_i++)
    {
        ASSERT_EQ(l_tokens[l_i].type, l_expected[l_i], std::string("Operator mismatch at index ") + std::to_string(l_i));
    }
}

void test_decorator()
{
    pyke::Lexer l_lexer("@Executable");
    std::vector<pyke::Token> l_tokens = contentTokens(l_lexer.tokenize());
    ASSERT_EQ(l_tokens.size(), size_t(2), "Should have @ and Executable");
    ASSERT_EQ(l_tokens[0].type, pyke::TokenType::AT, "First should be At");
    ASSERT_EQ(l_tokens[1].type, pyke::TokenType::EXECUTABLE, "Second should be Executable");
}

void test_comment_ignored()
{
    pyke::Lexer l_lexer("target # this is a comment\nMyApp");
    std::vector<pyke::Token> l_tokens = contentTokens(l_lexer.tokenize());
    ASSERT_EQ(l_tokens.size(), size_t(2), "Comment should be skipped");
    ASSERT_EQ(l_tokens[0].type, pyke::TokenType::TARGET, "First is target");
    ASSERT_EQ(l_tokens[1].type, pyke::TokenType::IDENTIFIER, "Second is MyApp");
}

void test_comment_only_line()
{
    pyke::Lexer l_lexer("# full line comment\ntarget");
    std::vector<pyke::Token> l_tokens = contentTokens(l_lexer.tokenize());
    ASSERT_EQ(l_tokens.size(), size_t(1), "Comment line should produce no tokens");
    ASSERT_EQ(l_tokens[0].type, pyke::TokenType::TARGET, "Should be target");
}

void test_blank_lines_ignored()
{
    pyke::Lexer l_lexer("target\n\n\nMyApp");
    std::vector<pyke::Token> l_tokens = contentTokens(l_lexer.tokenize());
    ASSERT_EQ(l_tokens.size(), size_t(2), "Blank lines should be skipped");
    ASSERT_EQ(l_tokens[0].type, pyke::TokenType::TARGET, "First is target");
    ASSERT_EQ(l_tokens[1].type, pyke::TokenType::IDENTIFIER, "Second is MyApp");
}

void test_simple_indentation()
{
    pyke::Lexer l_lexer("target:\n    sources");
    std::vector<pyke::Token> l_tokens = l_lexer.tokenize();

    std::vector<pyke::TokenType> l_expected = {
        pyke::TokenType::TARGET,
        pyke::TokenType::COLON,
        pyke::TokenType::NEWLINE,
        pyke::TokenType::INDENT,
        pyke::TokenType::IDENTIFIER,
        pyke::TokenType::DEDENT,
        pyke::TokenType::END_OF_FILE,
    };

    ASSERT_EQ(l_tokens.size(), l_expected.size(), "Token count mismatch");
    for (size_t l_i = 0; l_i < l_expected.size(); l_i++)
    {
        ASSERT_EQ(l_tokens[l_i].type, l_expected[l_i], std::string("Token mismatch at index ") + std::to_string(l_i));
    }
}

void test_nested_indentation()
{
    pyke::Lexer l_lexer("a:\n    b:\n        c\n    d\ne");
    std::vector<pyke::Token> l_tokens = l_lexer.tokenize();

    int l_indentCount = 0;
    int l_dedentCount = 0;
    for (const pyke::Token& l_t : l_tokens)
    {
        if (l_t.type == pyke::TokenType::INDENT) l_indentCount++;
        if (l_t.type == pyke::TokenType::DEDENT) l_dedentCount++;
    }

    ASSERT_EQ(l_indentCount, 2, "Should have 2 indents");
    ASSERT_EQ(l_dedentCount, 2, "Should have 2 dedents (matching indents)");
}

void test_line_numbers()
{
    pyke::Lexer l_lexer("first\nsecond\nthird");
    std::vector<pyke::Token> l_tokens = contentTokens(l_lexer.tokenize());
    ASSERT_EQ(l_tokens[0].line, 1, "First token on line 1");
    ASSERT_EQ(l_tokens[1].line, 2, "Second token on line 2");
    ASSERT_EQ(l_tokens[2].line, 3, "Third token on line 3");
}

void test_import_statement()
{
    pyke::Lexer l_lexer("from packages import Boost, fmt");
    std::vector<pyke::Token> l_tokens = contentTokens(l_lexer.tokenize());

    ASSERT_EQ(l_tokens.size(), size_t(6), "Import should have 6 content tokens");
    ASSERT_EQ(l_tokens[0].type, pyke::TokenType::FROM, "from");
    ASSERT_EQ(l_tokens[1].type, pyke::TokenType::IDENTIFIER, "packages");
    ASSERT_EQ(l_tokens[1].value, std::string("packages"), "packages value");
    ASSERT_EQ(l_tokens[2].type, pyke::TokenType::IMPORT, "import");
    ASSERT_EQ(l_tokens[3].type, pyke::TokenType::IDENTIFIER, "Boost");
    ASSERT_EQ(l_tokens[4].type, pyke::TokenType::COMMA, "comma");
    ASSERT_EQ(l_tokens[5].type, pyke::TokenType::IDENTIFIER, "fmt");
}

void test_target_declaration()
{
    pyke::Lexer l_lexer("@SharedLibrary\ntarget Core(PRIVATE Lib1, PUBLIC Lib2):\n    def configure(self):\n        self.sources = [\"src/*.cpp\"]");
    std::vector<pyke::Token> l_tokens = contentTokens(l_lexer.tokenize());

    ASSERT_TRUE(l_tokens.size() > 15, "Should have many tokens");
    ASSERT_EQ(l_tokens[0].type, pyke::TokenType::AT, "@");
    ASSERT_EQ(l_tokens[1].type, pyke::TokenType::SHARED_LIBRARY, "SharedLibrary");
    ASSERT_EQ(l_tokens[2].type, pyke::TokenType::TARGET, "target");
    ASSERT_EQ(l_tokens[3].type, pyke::TokenType::IDENTIFIER, "Core");
    ASSERT_EQ(l_tokens[4].type, pyke::TokenType::LEFT_PAREN, "(");
    ASSERT_EQ(l_tokens[5].type, pyke::TokenType::PRIVATE_KW, "PRIVATE");
}

void test_unexpected_character()
{
    pyke::Lexer l_lexer("target ~");
    std::vector<pyke::Token> l_tokens = contentTokens(l_lexer.tokenize());
    ASSERT_EQ(l_tokens.size(), size_t(2), "Should have target + error");
    ASSERT_EQ(l_tokens[1].type, pyke::TokenType::ERROR_TOKEN, "Should be Error");
}

void test_dot_access()
{
    pyke::Lexer l_lexer("self.exports.includes");
    std::vector<pyke::Token> l_tokens = contentTokens(l_lexer.tokenize());
    ASSERT_EQ(l_tokens.size(), size_t(5), "Should be self . exports . includes");
    ASSERT_EQ(l_tokens[0].type, pyke::TokenType::SELF_, "self");
    ASSERT_EQ(l_tokens[1].type, pyke::TokenType::DOT, ".");
    ASSERT_EQ(l_tokens[2].type, pyke::TokenType::IDENTIFIER, "exports");
    ASSERT_EQ(l_tokens[3].type, pyke::TokenType::DOT, ".");
    ASSERT_EQ(l_tokens[4].type, pyke::TokenType::IDENTIFIER, "includes");
}

void test_option_declaration()
{
    pyke::Lexer l_lexer("option use_opengl: bool = True");
    std::vector<pyke::Token> l_tokens = contentTokens(l_lexer.tokenize());
    ASSERT_EQ(l_tokens[0].type, pyke::TokenType::OPTION, "option");
    ASSERT_EQ(l_tokens[1].type, pyke::TokenType::IDENTIFIER, "use_opengl");
    ASSERT_EQ(l_tokens[2].type, pyke::TokenType::COLON, ":");
    ASSERT_EQ(l_tokens[3].type, pyke::TokenType::BOOL_TYPE, "bool");
    ASSERT_EQ(l_tokens[4].type, pyke::TokenType::EQUALS, "=");
    ASSERT_EQ(l_tokens[5].type, pyke::TokenType::TRUE_KW, "True");
}

void test_dict_literal()
{
    pyke::Lexer l_lexer("{\"CORE_API\": 1}");
    std::vector<pyke::Token> l_tokens = contentTokens(l_lexer.tokenize());
    ASSERT_EQ(l_tokens[0].type, pyke::TokenType::LEFT_BRACE, "{");
    ASSERT_EQ(l_tokens[1].type, pyke::TokenType::STRING_LITERAL, "CORE_API");
    ASSERT_EQ(l_tokens[2].type, pyke::TokenType::COLON, ":");
    ASSERT_EQ(l_tokens[3].type, pyke::TokenType::INT_LITERAL, "1");
    ASSERT_EQ(l_tokens[4].type, pyke::TokenType::RIGHT_BRACE, "}");
}

int main()
{
    std::cout << "=== Pyke Lexer Tests ===" << std::endl;

    RUN_TEST(test_empty_input);
    RUN_TEST(test_single_keyword_target);
    RUN_TEST(test_all_keywords);
    RUN_TEST(test_identifier);
    RUN_TEST(test_string_literal);
    RUN_TEST(test_string_escape_sequences);
    RUN_TEST(test_unterminated_string);
    RUN_TEST(test_int_literal);
    RUN_TEST(test_operators);
    RUN_TEST(test_decorator);
    RUN_TEST(test_comment_ignored);
    RUN_TEST(test_comment_only_line);
    RUN_TEST(test_blank_lines_ignored);
    RUN_TEST(test_simple_indentation);
    RUN_TEST(test_nested_indentation);
    RUN_TEST(test_line_numbers);
    RUN_TEST(test_import_statement);
    RUN_TEST(test_target_declaration);
    RUN_TEST(test_unexpected_character);
    RUN_TEST(test_dot_access);
    RUN_TEST(test_option_declaration);
    RUN_TEST(test_dict_literal);

    std::cout << std::endl;
    std::cout << "Results: " << s_testsPassed << "/" << s_testsRun << " passed";
    if (s_testsFailed > 0)
    {
        std::cout << " (" << s_testsFailed << " failed)" << std::endl;
        std::cout << std::endl << "Failures:" << std::endl;
        for (const TestFailure& l_f : s_failures)
        {
            std::cout << "  " << l_f.file << ":" << l_f.line << " - " << l_f.message << std::endl;
        }
        return 1;
    }
    std::cout << std::endl;
    return 0;
}
