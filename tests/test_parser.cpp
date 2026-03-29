// These unit tests were automatically generated using AI

#include "lexer.hpp"
#include "parser.hpp"
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

#define ASSERT_TRUE(cond, msg) \
    do { \
        if (!(cond)) { \
            s_failures.push_back({std::string(msg), __FILE__, __LINE__}); \
            s_testsFailed++; \
            return; \
        } \
    } while(0)

#define ASSERT_FALSE(cond, msg) ASSERT_TRUE(!(cond), msg)

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

static pyke::Program parseSource(const std::string& p_source, bool p_expectSuccess = true)
{
    pyke::Lexer l_lexer(p_source);
    std::vector<pyke::Token> l_tokens = l_lexer.tokenize();
    pyke::Parser l_parser(l_tokens);
    pyke::Program l_program = l_parser.parse();

    if (p_expectSuccess && l_parser.hasErrors())
    {
        std::cerr << "    Parse errors:" << std::endl;
        for (const std::string& l_e : l_parser.errors())
        {
            std::cerr << "      " << l_e << std::endl;
        }
    }

    return l_program;
}

static bool parseHasErrors(const std::string& p_source)
{
    pyke::Lexer l_lexer(p_source);
    std::vector<pyke::Token> l_tokens = l_lexer.tokenize();
    pyke::Parser l_parser(l_tokens);
    l_parser.parse();
    return l_parser.hasErrors();
}

void test_parse_import()
{
    pyke::Program l_prog = parseSource("from packages import Boost, fmt, Threads");
    ASSERT_EQ(l_prog.imports.size(), size_t(1), "Should have 1 import");
    ASSERT_EQ(l_prog.imports[0].packages.size(), size_t(3), "Should have 3 packages");
    ASSERT_EQ(l_prog.imports[0].packages[0], std::string("Boost"), "First package");
    ASSERT_EQ(l_prog.imports[0].packages[1], std::string("fmt"), "Second package");
    ASSERT_EQ(l_prog.imports[0].packages[2], std::string("Threads"), "Third package");
}

void test_parse_project()
{
    pyke::Program l_prog = parseSource("project(\"MyProject\", version=\"1.0.0\", lang=\"c++20\")");
    ASSERT_TRUE(l_prog.project.has_value(), "Should have project");
    ASSERT_EQ(l_prog.project->name, std::string("MyProject"), "Project name");
    ASSERT_EQ(l_prog.project->version, std::string("1.0.0"), "Project version");
    ASSERT_EQ(l_prog.project->lang, std::string("c++20"), "Project language");
}

void test_parse_option_bool()
{
    pyke::Program l_prog = parseSource("option use_opengl: bool = True");
    ASSERT_EQ(l_prog.options.size(), size_t(1), "Should have 1 option");
    ASSERT_EQ(l_prog.options[0].name, std::string("use_opengl"), "Option name");
    ASSERT_EQ(l_prog.options[0].type, std::string("bool"), "Option type");
    ASSERT_TRUE(l_prog.options[0].defaultValue != nullptr, "Should have default value");

    auto* l_boolVal = std::get_if<pyke::BoolLiteral>(&l_prog.options[0].defaultValue->value);
    ASSERT_TRUE(l_boolVal != nullptr, "Default should be BoolLiteral");
    ASSERT_TRUE(l_boolVal->value, "Default should be true");
}

void test_parse_option_str()
{
    pyke::Program l_prog = parseSource("option prefix: str = \"hello\"");
    ASSERT_EQ(l_prog.options[0].type, std::string("str"), "Option type");
    auto* l_strVal = std::get_if<pyke::StringLiteral>(&l_prog.options[0].defaultValue->value);
    ASSERT_TRUE(l_strVal != nullptr, "Default should be StringLiteral");
    ASSERT_EQ(l_strVal->value, std::string("hello"), "Default value");
}

void test_parse_minimal_target()
{
    std::string l_source =
        "@Executable\n"
        "target MyApp():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n";

    pyke::Program l_prog = parseSource(l_source);
    ASSERT_EQ(l_prog.targets.size(), size_t(1), "Should have 1 target");
    ASSERT_EQ(l_prog.targets[0].name, std::string("MyApp"), "Target name");
    ASSERT_EQ(static_cast<int>(l_prog.targets[0].type), static_cast<int>(pyke::TargetType::EXECUTABLE), "Target type");
    ASSERT_TRUE(l_prog.targets[0].dependencies.empty(), "No dependencies");
    ASSERT_EQ(l_prog.targets[0].methods.size(), size_t(1), "Should have 1 method");
    ASSERT_EQ(l_prog.targets[0].methods[0].name, std::string("configure"), "Method name");
}

void test_parse_target_with_deps()
{
    std::string l_source =
        "@SharedLibrary\n"
        "target Renderer(PRIVATE Core, PUBLIC Boost.System):\n"
        "    def configure(self):\n"
        "        self.sources = [\"src/*.cpp\"]\n";

    pyke::Program l_prog = parseSource(l_source);
    ASSERT_EQ(l_prog.targets[0].dependencies.size(), size_t(2), "Should have 2 deps");
    ASSERT_EQ(l_prog.targets[0].dependencies[0].visibility, std::string("PRIVATE"), "First dep visibility");
    ASSERT_EQ(l_prog.targets[0].dependencies[0].name, std::string("Core"), "First dep name");
    ASSERT_EQ(l_prog.targets[0].dependencies[1].visibility, std::string("PUBLIC"), "Second dep visibility");
    ASSERT_EQ(l_prog.targets[0].dependencies[1].name, std::string("Boost.System"), "Second dep name (dotted)");
}

void test_parse_default_private()
{
    std::string l_source =
        "@Executable\n"
        "target App(Lib1, PUBLIC Lib2):\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n";

    pyke::Program l_prog = parseSource(l_source);
    ASSERT_EQ(l_prog.targets[0].dependencies[0].visibility, std::string("PRIVATE"), "Bare dep defaults to PRIVATE");
    ASSERT_EQ(l_prog.targets[0].dependencies[1].visibility, std::string("PUBLIC"), "Explicit PUBLIC");
}

void test_parse_decorator_with_path()
{
    std::string l_source =
        "@SharedLibrary(\"libs/renderer\")\n"
        "target Renderer():\n"
        "    def configure(self):\n"
        "        self.sources = [\"src/*.cpp\"]\n";

    pyke::Program l_prog = parseSource(l_source);
    ASSERT_EQ(l_prog.targets[0].path, std::string("libs/renderer"), "Decorator path");
    ASSERT_EQ(static_cast<int>(l_prog.targets[0].type), static_cast<int>(pyke::TargetType::SHARED_LIBRARY), "Type");
}

void test_parse_assignment()
{
    std::string l_source =
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"a.cpp\", \"b.cpp\"]\n";

    pyke::Program l_prog = parseSource(l_source);
    std::vector<pyke::StmtPtr>& l_body = l_prog.targets[0].methods[0].body;
    ASSERT_EQ(l_body.size(), size_t(1), "Should have 1 statement");

    auto* l_assign = std::get_if<pyke::AssignStatement>(&l_body[0]->value);
    ASSERT_TRUE(l_assign != nullptr, "Should be AssignStatement");

    auto* l_dot = std::get_if<pyke::DotAccess>(&l_assign->target->value);
    ASSERT_TRUE(l_dot != nullptr, "Target should be DotAccess");
    ASSERT_EQ(l_dot->member, std::string("sources"), "Member name");

    auto* l_list = std::get_if<pyke::ListLiteral>(&l_assign->value->value);
    ASSERT_TRUE(l_list != nullptr, "Value should be ListLiteral");
    ASSERT_EQ(l_list->elements.size(), size_t(2), "List should have 2 elements");
}

void test_parse_aug_assignment()
{
    std::string l_source =
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources += [\"extra.cpp\"]\n";

    pyke::Program l_prog = parseSource(l_source);
    std::vector<pyke::StmtPtr>& l_body = l_prog.targets[0].methods[0].body;
    auto* l_aug = std::get_if<pyke::AugAssignStatement>(&l_body[0]->value);
    ASSERT_TRUE(l_aug != nullptr, "Should be AugAssignStatement");
}

void test_parse_dict_literal()
{
    std::string l_source =
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.definitions = {\"DEBUG\": 1, \"VERSION\": 2}\n";

    pyke::Program l_prog = parseSource(l_source);
    std::vector<pyke::StmtPtr>& l_body = l_prog.targets[0].methods[0].body;
    auto* l_assign = std::get_if<pyke::AssignStatement>(&l_body[0]->value);
    ASSERT_TRUE(l_assign != nullptr, "Should be AssignStatement");

    auto* l_dict = std::get_if<pyke::DictLiteral>(&l_assign->value->value);
    ASSERT_TRUE(l_dict != nullptr, "Value should be DictLiteral");
    ASSERT_EQ(l_dict->entries.size(), size_t(2), "Dict should have 2 entries");
}

void test_parse_index_assignment()
{
    std::string l_source =
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.definitions[\"KEY\"] = True\n";

    pyke::Program l_prog = parseSource(l_source);
    std::vector<pyke::StmtPtr>& l_body = l_prog.targets[0].methods[0].body;
    auto* l_assign = std::get_if<pyke::AssignStatement>(&l_body[0]->value);
    ASSERT_TRUE(l_assign != nullptr, "Should be AssignStatement");

    auto* l_idx = std::get_if<pyke::IndexAccess>(&l_assign->target->value);
    ASSERT_TRUE(l_idx != nullptr, "Target should be IndexAccess");
}

void test_parse_if_statement()
{
    std::string l_source =
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        if platform == \"windows\":\n"
        "            self.sources += [\"win.cpp\"]\n";

    pyke::Program l_prog = parseSource(l_source);
    std::vector<pyke::StmtPtr>& l_body = l_prog.targets[0].methods[0].body;
    ASSERT_EQ(l_body.size(), size_t(1), "Should have 1 statement");

    auto* l_ifStmt = std::get_if<pyke::IfStatement>(&l_body[0]->value);
    ASSERT_TRUE(l_ifStmt != nullptr, "Should be IfStatement");
    ASSERT_EQ(l_ifStmt->branches.size(), size_t(1), "Should have 1 branch (if only)");
    ASSERT_TRUE(l_ifStmt->branches[0].condition != nullptr, "If branch has condition");
    ASSERT_EQ(l_ifStmt->branches[0].body.size(), size_t(1), "If branch has 1 statement");
}

void test_parse_if_else()
{
    std::string l_source =
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        if compiler == \"msvc\":\n"
        "            self.flags += [\"/W4\"]\n"
        "        else:\n"
        "            self.flags += [\"-Wall\"]\n";

    pyke::Program l_prog = parseSource(l_source);
    std::vector<pyke::StmtPtr>& l_body = l_prog.targets[0].methods[0].body;
    auto* l_ifStmt = std::get_if<pyke::IfStatement>(&l_body[0]->value);
    ASSERT_TRUE(l_ifStmt != nullptr, "Should be IfStatement");
    ASSERT_EQ(l_ifStmt->branches.size(), size_t(2), "Should have 2 branches (if + else)");
    ASSERT_TRUE(l_ifStmt->branches[0].condition != nullptr, "If has condition");
    ASSERT_TRUE(l_ifStmt->branches[1].condition == nullptr, "Else has no condition");
}

void test_parse_if_elif_else()
{
    std::string l_source =
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        if platform == \"windows\":\n"
        "            self.sources += [\"win.cpp\"]\n"
        "        elif platform == \"linux\":\n"
        "            self.sources += [\"linux.cpp\"]\n"
        "        else:\n"
        "            self.sources += [\"other.cpp\"]\n";

    pyke::Program l_prog = parseSource(l_source);
    std::vector<pyke::StmtPtr>& l_body = l_prog.targets[0].methods[0].body;
    auto* l_ifStmt = std::get_if<pyke::IfStatement>(&l_body[0]->value);
    ASSERT_TRUE(l_ifStmt != nullptr, "Should be IfStatement");
    ASSERT_EQ(l_ifStmt->branches.size(), size_t(3), "Should have 3 branches (if + elif + else)");
}

void test_parse_exports_dot_access()
{
    std::string l_source =
        "@SharedLibrary\n"
        "target Lib():\n"
        "    def configure(self):\n"
        "        self.exports.includes = [\"include/\"]\n";

    pyke::Program l_prog = parseSource(l_source);
    std::vector<pyke::StmtPtr>& l_body = l_prog.targets[0].methods[0].body;
    auto* l_assign = std::get_if<pyke::AssignStatement>(&l_body[0]->value);
    ASSERT_TRUE(l_assign != nullptr, "Should be AssignStatement");

    auto* l_outer = std::get_if<pyke::DotAccess>(&l_assign->target->value);
    ASSERT_TRUE(l_outer != nullptr, "Should be DotAccess");
    ASSERT_EQ(l_outer->member, std::string("includes"), "Outer member");

    auto* l_inner = std::get_if<pyke::DotAccess>(&l_outer->object->value);
    ASSERT_TRUE(l_inner != nullptr, "Inner should be DotAccess");
    ASSERT_EQ(l_inner->member, std::string("exports"), "Inner member");
}

void test_parse_install_method()
{
    std::string l_source =
        "@SharedLibrary\n"
        "target Lib():\n"
        "    def configure(self):\n"
        "        self.sources = [\"src/*.cpp\"]\n"
        "    def install(self):\n"
        "        self.library = \"lib\"\n"
        "        self.headers = (\"include/\", \"include/lib\")\n";

    pyke::Program l_prog = parseSource(l_source);
    ASSERT_EQ(l_prog.targets[0].methods.size(), size_t(2), "Should have 2 methods");
    ASSERT_EQ(l_prog.targets[0].methods[0].name, std::string("configure"), "First method");
    ASSERT_EQ(l_prog.targets[0].methods[1].name, std::string("install"), "Second method");
    ASSERT_EQ(l_prog.targets[0].methods[1].body.size(), size_t(2), "Install has 2 statements");

    auto* l_assign = std::get_if<pyke::AssignStatement>(&l_prog.targets[0].methods[1].body[1]->value);
    ASSERT_TRUE(l_assign != nullptr, "Should be AssignStatement");
    auto* l_tuple = std::get_if<pyke::TupleLiteral>(&l_assign->value->value);
    ASSERT_TRUE(l_tuple != nullptr, "Value should be TupleLiteral");
    ASSERT_EQ(l_tuple->elements.size(), size_t(2), "Tuple should have 2 elements");
}

void test_parse_multiple_targets()
{
    std::string l_source =
        "@SharedLibrary\n"
        "target Core():\n"
        "    def configure(self):\n"
        "        self.sources = [\"src/*.cpp\"]\n"
        "\n"
        "@Executable\n"
        "target App(PRIVATE Core):\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n";

    pyke::Program l_prog = parseSource(l_source);
    ASSERT_EQ(l_prog.targets.size(), size_t(2), "Should have 2 targets");
    ASSERT_EQ(l_prog.targets[0].name, std::string("Core"), "First target");
    ASSERT_EQ(l_prog.targets[1].name, std::string("App"), "Second target");
}

void test_parse_comparison_in_if()
{
    std::string l_source =
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        if build_type == \"debug\":\n"
        "            self.definitions[\"DEBUG\"] = True\n";

    pyke::Program l_prog = parseSource(l_source);
    std::vector<pyke::StmtPtr>& l_body = l_prog.targets[0].methods[0].body;
    auto* l_ifStmt = std::get_if<pyke::IfStatement>(&l_body[0]->value);
    ASSERT_TRUE(l_ifStmt != nullptr, "Should be IfStatement");

    auto* l_cmp = std::get_if<pyke::Comparison>(&l_ifStmt->branches[0].condition->value);
    ASSERT_TRUE(l_cmp != nullptr, "Condition should be Comparison");
    ASSERT_EQ(l_cmp->op, std::string("=="), "Operator");

    auto* l_left = std::get_if<pyke::Identifier>(&l_cmp->left->value);
    ASSERT_TRUE(l_left != nullptr, "Left should be Identifier");
    ASSERT_EQ(l_left->name, std::string("build_type"), "Left name");
}

void test_parse_bare_option_if()
{
    std::string l_source =
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        if use_opengl:\n"
        "            self.link += [OpenGL]\n";

    pyke::Program l_prog = parseSource(l_source);
    std::vector<pyke::StmtPtr>& l_body = l_prog.targets[0].methods[0].body;
    auto* l_ifStmt = std::get_if<pyke::IfStatement>(&l_body[0]->value);
    ASSERT_TRUE(l_ifStmt != nullptr, "Should be IfStatement");

    auto* l_ident = std::get_if<pyke::Identifier>(&l_ifStmt->branches[0].condition->value);
    ASSERT_TRUE(l_ident != nullptr, "Condition should be bare Identifier");
    ASSERT_EQ(l_ident->name, std::string("use_opengl"), "Option name");
}

void test_parse_all_target_types()
{
    std::string l_types[] = {
        "@Executable\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n",
        "@SharedLibrary\ntarget B():\n    def configure(self):\n        self.sources = [\"b.cpp\"]\n",
        "@StaticLibrary\ntarget C():\n    def configure(self):\n        self.sources = [\"c.cpp\"]\n",
        "@HeaderOnly\ntarget D():\n    def configure(self):\n        self.sources = [\"d.cpp\"]\n",
    };

    pyke::TargetType l_expected[] = {
        pyke::TargetType::EXECUTABLE,
        pyke::TargetType::SHARED_LIBRARY,
        pyke::TargetType::STATIC_LIBRARY,
        pyke::TargetType::HEADER_ONLY,
    };

    for (int l_i = 0; l_i < 4; l_i++)
    {
        pyke::Program l_prog = parseSource(l_types[l_i]);
        ASSERT_EQ(static_cast<int>(l_prog.targets[0].type), static_cast<int>(l_expected[l_i]), std::string("Target type mismatch at index ") + std::to_string(l_i));
    }
}

void test_parse_full_example()
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
        "        self.exports.definitions = {\"CORE_API\": 1}\n"
        "\n"
        "@SharedLibrary(\"libs/renderer\")\n"
        "target Renderer(PRIVATE Core, PUBLIC Boost.System):\n"
        "    def configure(self):\n"
        "        self.sources = [\"src/*.cpp\"]\n"
        "        if use_opengl:\n"
        "            self.sources += [\"src/gl/*.cpp\"]\n"
        "            self.link += [OpenGL]\n"
        "        if compiler == \"msvc\":\n"
        "            self.flags += [\"/W4\"]\n"
        "        else:\n"
        "            self.flags += [\"-Wall\", \"-Wextra\"]\n"
        "    def install(self):\n"
        "        self.library = \"lib\"\n"
        "\n"
        "@Executable\n"
        "target Game(PRIVATE Core, PRIVATE Renderer, PRIVATE fmt):\n"
        "    def configure(self):\n"
        "        self.sources = [\"src/*.cpp\"]\n";

    pyke::Program l_prog = parseSource(l_source);

    ASSERT_EQ(l_prog.imports.size(), size_t(1), "1 import");
    ASSERT_EQ(l_prog.imports[0].packages.size(), size_t(3), "3 packages");

    ASSERT_TRUE(l_prog.project.has_value(), "Has project");
    ASSERT_EQ(l_prog.project->name, std::string("GameEngine"), "Project name");

    ASSERT_EQ(l_prog.options.size(), size_t(1), "1 option");

    ASSERT_EQ(l_prog.targets.size(), size_t(3), "3 targets");

    ASSERT_EQ(l_prog.targets[0].name, std::string("Core"), "Core");
    ASSERT_EQ(static_cast<int>(l_prog.targets[0].type), static_cast<int>(pyke::TargetType::SHARED_LIBRARY), "Core type");
    ASSERT_TRUE(l_prog.targets[0].path.empty(), "Core has default path");

    ASSERT_EQ(l_prog.targets[1].name, std::string("Renderer"), "Renderer");
    ASSERT_EQ(l_prog.targets[1].path, std::string("libs/renderer"), "Renderer path");
    ASSERT_EQ(l_prog.targets[1].dependencies.size(), size_t(2), "Renderer deps");
    ASSERT_EQ(l_prog.targets[1].methods.size(), size_t(2), "Renderer has configure + install");

    ASSERT_EQ(l_prog.targets[2].name, std::string("Game"), "Game");
    ASSERT_EQ(l_prog.targets[2].dependencies.size(), size_t(3), "Game deps");
}

void test_parse_error_missing_configure()
{
    std::string l_source =
        "@Executable\n"
        "target App():\n"
        "    self.sources = [\"main.cpp\"]\n";

    ASSERT_TRUE(parseHasErrors(l_source), "Should have errors — no def configure");
}

int main()
{
    std::cout << "=== Pyke Parser Tests ===" << std::endl;

    RUN_TEST(test_parse_import);
    RUN_TEST(test_parse_project);
    RUN_TEST(test_parse_option_bool);
    RUN_TEST(test_parse_option_str);
    RUN_TEST(test_parse_minimal_target);
    RUN_TEST(test_parse_target_with_deps);
    RUN_TEST(test_parse_default_private);
    RUN_TEST(test_parse_decorator_with_path);
    RUN_TEST(test_parse_assignment);
    RUN_TEST(test_parse_aug_assignment);
    RUN_TEST(test_parse_dict_literal);
    RUN_TEST(test_parse_index_assignment);
    RUN_TEST(test_parse_if_statement);
    RUN_TEST(test_parse_if_else);
    RUN_TEST(test_parse_if_elif_else);
    RUN_TEST(test_parse_exports_dot_access);
    RUN_TEST(test_parse_install_method);
    RUN_TEST(test_parse_multiple_targets);
    RUN_TEST(test_parse_comparison_in_if);
    RUN_TEST(test_parse_bare_option_if);
    RUN_TEST(test_parse_all_target_types);
    RUN_TEST(test_parse_full_example);
    RUN_TEST(test_parse_error_missing_configure);

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
