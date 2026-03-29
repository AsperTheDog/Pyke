// These unit tests were automatically generated using AI

#include "lexer.hpp"
#include "parser.hpp"
#include "analyzer.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            s_failures.push_back({std::string(msg), __FILE__, __LINE__}); \
            s_testsFailed++; \
            return; \
        } \
    } while(0)

#define ASSERT_FALSE(cond, msg) ASSERT_TRUE(!(cond), msg)

#define ASSERT_EQ(actual, expected, msg) \
    do { \
        if ((actual) != (expected)) { \
            std::ostringstream oss; \
            oss << msg << ": expected '" << (expected) << "', got '" << (actual) << "'"; \
            s_failures.push_back({oss.str(), __FILE__, __LINE__}); \
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

static bool analyzeSource(const std::string& p_source)
{
    pyke::Lexer l_lexer(p_source);
    std::vector<pyke::Token> l_tokens = l_lexer.tokenize();
    pyke::Parser l_parser(l_tokens);
    pyke::Program l_program = l_parser.parse();
    if (l_parser.hasErrors()) return false;
    pyke::Analyzer l_analyzer(l_program);
    return l_analyzer.analyze();
}

static std::vector<std::string> analyzeErrors(const std::string& p_source)
{
    pyke::Lexer l_lexer(p_source);
    std::vector<pyke::Token> l_tokens = l_lexer.tokenize();
    pyke::Parser l_parser(l_tokens);
    pyke::Program l_program = l_parser.parse();
    pyke::Analyzer l_analyzer(l_program);
    l_analyzer.analyze();
    return l_analyzer.errors();
}

void test_valid_simple_target()
{
    std::string l_source =
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n";

    ASSERT_TRUE(analyzeSource(l_source), "Simple valid target should pass");
}

void test_valid_with_internal_dep()
{
    std::string l_source =
        "@SharedLibrary\n"
        "target Core():\n"
        "    def configure(self):\n"
        "        self.sources = [\"core.cpp\"]\n"
        "\n"
        "@Executable\n"
        "target App(PRIVATE Core):\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n";

    ASSERT_TRUE(analyzeSource(l_source), "Valid internal dep should pass");
}

void test_valid_with_imported_dep()
{
    std::string l_source =
        "from packages import fmt\n"
        "\n"
        "@Executable\n"
        "target App(PRIVATE fmt):\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n";

    ASSERT_TRUE(analyzeSource(l_source), "Valid imported dep should pass");
}

void test_valid_dotted_package()
{
    std::string l_source =
        "from packages import Boost\n"
        "\n"
        "@SharedLibrary\n"
        "target Net(PUBLIC Boost.System):\n"
        "    def configure(self):\n"
        "        self.sources = [\"net.cpp\"]\n";

    ASSERT_TRUE(analyzeSource(l_source), "Dotted package ref should pass");
}

void test_undefined_dependency()
{
    std::string l_source =
        "@Executable\n"
        "target App(PRIVATE UnknownLib):\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n";

    ASSERT_FALSE(analyzeSource(l_source), "Undefined dep should fail");
    std::vector<std::string> l_errors = analyzeErrors(l_source);
    ASSERT_TRUE(l_errors.size() >= 1, "Should have at least 1 error");
    ASSERT_TRUE(l_errors[0].find("UnknownLib") != std::string::npos, "Error should mention UnknownLib");
}

void test_undefined_package()
{
    std::string l_source =
        "@Executable\n"
        "target App(PRIVATE Boost.System):\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n";

    ASSERT_FALSE(analyzeSource(l_source), "Non-imported package should fail");
}

void test_duplicate_target_name()
{
    std::string l_source =
        "@SharedLibrary\n"
        "target Core():\n"
        "    def configure(self):\n"
        "        self.sources = [\"a.cpp\"]\n"
        "\n"
        "@SharedLibrary\n"
        "target Core():\n"
        "    def configure(self):\n"
        "        self.sources = [\"b.cpp\"]\n";

    ASSERT_FALSE(analyzeSource(l_source), "Duplicate target names should fail");
    std::vector<std::string> l_errors = analyzeErrors(l_source);
    ASSERT_TRUE(l_errors[0].find("duplicate") != std::string::npos, "Error should mention duplicate");
}

void test_missing_configure()
{
    std::string l_source =
        "@Executable\n"
        "target App():\n"
        "    def install(self):\n"
        "        self.runtime = \"bin\"\n";

    ASSERT_FALSE(analyzeSource(l_source), "Missing configure should fail");
    std::vector<std::string> l_errors = analyzeErrors(l_source);
    ASSERT_TRUE(l_errors[0].find("configure") != std::string::npos, "Error should mention configure");
}

void test_unknown_method()
{
    std::string l_source =
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
        "    def build(self):\n"
        "        self.flags = [\"-O2\"]\n";

    ASSERT_FALSE(analyzeSource(l_source), "Unknown method should fail");
    std::vector<std::string> l_errors = analyzeErrors(l_source);
    ASSERT_TRUE(l_errors[0].find("build") != std::string::npos, "Error should mention 'build'");
}

void test_circular_dependency()
{
    std::string l_source =
        "@SharedLibrary\n"
        "target A(PRIVATE B):\n"
        "    def configure(self):\n"
        "        self.sources = [\"a.cpp\"]\n"
        "\n"
        "@SharedLibrary\n"
        "target B(PRIVATE A):\n"
        "    def configure(self):\n"
        "        self.sources = [\"b.cpp\"]\n";

    ASSERT_FALSE(analyzeSource(l_source), "Circular deps should fail");
    std::vector<std::string> l_errors = analyzeErrors(l_source);
    bool l_foundCycle = false;
    for (const std::string& l_e : l_errors)
    {
        if (l_e.find("Circular") != std::string::npos) l_foundCycle = true;
    }
    ASSERT_TRUE(l_foundCycle, "Should report circular dependency");
}

void test_three_way_cycle()
{
    std::string l_source =
        "@SharedLibrary\n"
        "target A(PRIVATE B):\n"
        "    def configure(self):\n"
        "        self.sources = [\"a.cpp\"]\n"
        "\n"
        "@SharedLibrary\n"
        "target B(PRIVATE C):\n"
        "    def configure(self):\n"
        "        self.sources = [\"b.cpp\"]\n"
        "\n"
        "@SharedLibrary\n"
        "target C(PRIVATE A):\n"
        "    def configure(self):\n"
        "        self.sources = [\"c.cpp\"]\n";

    ASSERT_FALSE(analyzeSource(l_source), "3-way cycle should fail");
}

void test_valid_full_example()
{
    std::string l_source =
        "from packages import Boost, fmt, OpenGL\n"
        "\n"
        "project(\"GameEngine\", version=\"0.1.0\", lang=\"c++20\")\n"
        "\n"
        "option use_opengl: bool = True\n"
        "\n"
        "@SharedLibrary\n"
        "target Core():\n"
        "    def configure(self):\n"
        "        self.sources = [\"src/*.cpp\"]\n"
        "        self.exports.includes = [\"include/\"]\n"
        "\n"
        "@SharedLibrary(\"libs/renderer\")\n"
        "target Renderer(PRIVATE Core, PUBLIC Boost.System):\n"
        "    def configure(self):\n"
        "        self.sources = [\"src/*.cpp\"]\n"
        "    def install(self):\n"
        "        self.library = \"lib\"\n"
        "\n"
        "@Executable\n"
        "target Game(PRIVATE Core, PRIVATE Renderer, PRIVATE fmt):\n"
        "    def configure(self):\n"
        "        self.sources = [\"src/*.cpp\"]\n";

    ASSERT_TRUE(analyzeSource(l_source), "Full example should pass analysis");
}

void test_valid_configure_and_install()
{
    std::string l_source =
        "@SharedLibrary\n"
        "target Lib():\n"
        "    def configure(self):\n"
        "        self.sources = [\"src/*.cpp\"]\n"
        "    def install(self):\n"
        "        self.library = \"lib\"\n";

    ASSERT_TRUE(analyzeSource(l_source), "configure + install should pass");
}

int main()
{
    std::cout << "=== Pyke Analyzer Tests ===" << std::endl;

    RUN_TEST(test_valid_simple_target);
    RUN_TEST(test_valid_with_internal_dep);
    RUN_TEST(test_valid_with_imported_dep);
    RUN_TEST(test_valid_dotted_package);
    RUN_TEST(test_undefined_dependency);
    RUN_TEST(test_undefined_package);
    RUN_TEST(test_duplicate_target_name);
    RUN_TEST(test_missing_configure);
    RUN_TEST(test_unknown_method);
    RUN_TEST(test_circular_dependency);
    RUN_TEST(test_three_way_cycle);
    RUN_TEST(test_valid_full_example);
    RUN_TEST(test_valid_configure_and_install);

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
