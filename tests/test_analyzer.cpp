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

// Most tests focus on targets; give them the project declaration CMake requires.
static std::string withProject(const std::string& p_source)
{
    if (p_source.find("project(") != std::string::npos) return p_source;
    return "project(\"T\")\n" + p_source;
}

static bool analyzeSource(const std::string& p_rawSource)
{
    std::string p_source = withProject(p_rawSource);
    pyke::Lexer l_lexer(p_source);
    std::vector<pyke::Token> l_tokens = l_lexer.tokenize();
    pyke::Parser l_parser(l_tokens);
    pyke::Program l_program = l_parser.parse();
    if (l_parser.hasErrors()) return false;
    pyke::Analyzer l_analyzer(l_program);
    return l_analyzer.analyze();
}

static std::vector<std::string> analyzeErrors(const std::string& p_rawSource)
{
    std::string p_source = withProject(p_rawSource);
    pyke::Lexer l_lexer(p_source);
    std::vector<pyke::Token> l_tokens = l_lexer.tokenize();
    pyke::Parser l_parser(l_tokens);
    pyke::Program l_program = l_parser.parse();
    pyke::Analyzer l_analyzer(l_program);
    l_analyzer.analyze();
    return l_analyzer.errors();
}

void test_package_versions_and_optional()
{
    std::string l_t = "@Executable(\"app\")\ntarget app(PRIVATE Vulkan):\n    def configure(self):\n        self.sources = [\"m.cpp\"]\n";
    ASSERT_TRUE(analyzeSource("from packages import Boost(1.78), Vulkan(optional=True)\n" + l_t), "Versions and optional are accepted");
    ASSERT_FALSE(analyzeSource("from packages import Vulkan(1.x)\n" + l_t), "Bad version rejected");
    std::string l_cond = "@Executable(\"app\")\ntarget app():\n    def configure(self):\n        self.sources = [\"m.cpp\"]\n        if Vulkan:\n            self.definitions = {\"V\": 1}\n";
    ASSERT_TRUE(analyzeSource("from packages import Vulkan(optional=True)\n" + l_cond), "Optional package usable in conditions");
    ASSERT_FALSE(analyzeSource("from packages import Vulkan\n" + l_cond), "Required package is not a condition");
}

void test_quality_settings_validation()
{
    auto l_t = [](const std::string& p_line) {
        return "@Executable(\"app\")\ntarget app():\n    def configure(self):\n        self.sources = [\"m.cpp\"]\n        " + p_line + "\n";
    };
    ASSERT_TRUE(analyzeSource(l_t("self.warnings = \"strict\"")), "strict accepted");
    ASSERT_FALSE(analyzeSource(l_t("self.warnings = \"strcit\"")), "Unknown level rejected");
    ASSERT_TRUE(analyzeSource(l_t("self.sanitize = [\"address\", \"undefined\"]")), "Sanitizers accepted");
    ASSERT_FALSE(analyzeSource(l_t("self.sanitize = [\"adress\"]")), "Unknown sanitizer rejected");
    ASSERT_FALSE(analyzeSource(l_t("self.sanitize = [\"address\", \"thread\"]")), "address+thread rejected");
    ASSERT_FALSE(analyzeSource(l_t("self.lto = \"yes\"")), "lto must be a bool");
}

void test_conditional_github_import()
{
    std::string l_t = "@Executable(\"app\")\ntarget app():\n    def configure(self):\n        self.sources = [\"m.cpp\"]\n";
    ASSERT_TRUE(analyzeSource("option use_fmt: bool = False\nfrom github import \"fmtlib/fmt\" as fmt, tag=\"10.2.1\" if use_fmt\n" + l_t), "Conditional github import accepted");
    ASSERT_FALSE(analyzeSource("from github import \"fmtlib/fmt\" as fmt if nope\n" + l_t), "Unknown option rejected");
}

void test_vendor_import()
{
    std::string l_t = "@Executable(\"app\")\ntarget app(PRIVATE lz4):\n    def configure(self):\n        self.sources = [\"m.cpp\"]\n";
    ASSERT_TRUE(analyzeSource("from vendor import \"third_party/lz4\" as lz4\n" + l_t), "Vendor import accepted");
    ASSERT_FALSE(analyzeSource("from vendor import \"../lz4\" as lz4\n" + l_t), "Folders outside the project rejected");
    ASSERT_FALSE(analyzeSource("from vendor import \"app\" as lz4\n" + l_t), "Folder of a target rejected");
}

void test_cxx_modules_validation()
{
    std::string l_t = "@StaticLibrary(\"m\")\ntarget m():\n    def configure(self):\n        self.exports.modules = [\"m.cppm\"]\n";
    ASSERT_TRUE(analyzeSource("project(\"T\", lang=\"c++20\")\n" + l_t), "Modules-only target is valid on c++20");
    ASSERT_FALSE(analyzeSource("project(\"T\", lang=\"c++17\")\n" + l_t), "Modules need c++20");
    ASSERT_FALSE(analyzeSource("project(\"T\", lang=\"c++20\", import_std=True)\n" + l_t), "import_std needs c++23");
    ASSERT_TRUE(analyzeSource("project(\"T\", lang=\"c++23\", import_std=True)\n" + l_t), "import_std is valid on c++23");
    ASSERT_FALSE(analyzeSource("project(\"T\", lang=\"c++20\")\n" + l_t + "    def install(self):\n        self.export = \"M\"\n"), "Exported targets cannot ship modules yet");
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

static const std::string s_exe = "@Executable\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n";

static bool anyErrorContains(const std::string& p_source, const std::string& p_needle)
{
    for (const std::string& l_e : analyzeErrors(p_source))
    {
        if (l_e.find(p_needle) != std::string::npos) return true;
    }
    return false;
}

void test_missing_project()
{
    pyke::Lexer l_lexer(s_exe);
    pyke::Parser l_parser(l_lexer.tokenize());
    pyke::Program l_program = l_parser.parse();
    pyke::Analyzer l_analyzer(l_program);
    ASSERT_FALSE(l_analyzer.analyze(), "Program without project must fail");
    ASSERT_TRUE(l_analyzer.errors()[0].find("missing project") != std::string::npos, "Missing project should be reported");
}

void test_invalid_project_fields()
{
    ASSERT_TRUE(anyErrorContains("project(\"A\", version=\"x.y\")\n" + s_exe, "invalid project version"), "Bad version");
    ASSERT_TRUE(anyErrorContains("project(\"A\", lang=\"rust\")\n" + s_exe, "unsupported lang"), "Bad lang");
    ASSERT_TRUE(anyErrorContains("project(\"A b\")\n" + s_exe, "invalid project name"), "Bad name");
}

void test_unknown_attribute()
{
    ASSERT_TRUE(anyErrorContains("@Executable\ntarget A():\n    def configure(self):\n        self.sorces = [\"a.cpp\"]\n", "unknown attribute 'sorces'"), "Typo in attribute");
}

void test_attribute_type_checks()
{
    ASSERT_TRUE(anyErrorContains("@Executable\ntarget A():\n    def configure(self):\n        self.sources = 5\n", "'sources' expects a list"), "Non-list sources");
    ASSERT_TRUE(anyErrorContains("@Executable\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n        self.definitions = [\"X\"]\n", "'definitions' expects a dict"), "Non-dict definitions");
}

void test_install_attr_in_configure()
{
    ASSERT_TRUE(anyErrorContains("@Executable\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n        self.library = \"lib\"\n", "only valid inside install()"), "install attr in configure");
}

void test_no_sources()
{
    ASSERT_TRUE(anyErrorContains("@Executable\ntarget A():\n    def configure(self):\n        self.flags = [\"-Wall\"]\n", "no sources"), "Target without sources");
    ASSERT_TRUE(analyzeSource("@HeaderOnly\ntarget H():\n    def configure(self):\n        self.exports.includes = [\"include/\"]\n"), "Header-only needs no sources");
}

void test_duplicate_and_escaping_paths()
{
    ASSERT_TRUE(anyErrorContains("@Executable(\"d\")\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n@Executable(\"d/\")\ntarget B():\n    def configure(self):\n        self.sources = [\"b.cpp\"]\n", "already used"), "Duplicate dir");
    ASSERT_TRUE(anyErrorContains("@Executable(\"../out\")\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n", "stay inside"), "Path traversal");
    ASSERT_TRUE(anyErrorContains("@Executable(\"/abs\")\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n", "stay inside"), "Absolute path");
}

void test_condition_validation()
{
    ASSERT_TRUE(anyErrorContains("@Executable\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n        if platform == \"beos\":\n            self.flags += [\"-x\"]\n", "not a valid value for platform"), "Bad platform");
    ASSERT_TRUE(anyErrorContains("@Executable\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n        if nope:\n            self.flags += [\"-x\"]\n", "unknown variable 'nope'"), "Unknown condition var");
}

void test_option_validation()
{
    ASSERT_TRUE(anyErrorContains("option x: bool = \"s\"\n" + s_exe, "must be True or False"), "Bad bool default");
    ASSERT_TRUE(anyErrorContains("option x: bool = True\noption x: bool = False\n" + s_exe, "duplicate option"), "Duplicate option");
}

void test_cycle_reported_once_with_chain()
{
    std::vector<std::string> l_errs = analyzeErrors(
        "@StaticLibrary\ntarget A(B):\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n"
        "@StaticLibrary\ntarget B(A):\n    def configure(self):\n        self.sources = [\"b.cpp\"]\n");
    ASSERT_EQ(l_errs.size(), static_cast<size_t>(1), "Exactly one cycle error");
    ASSERT_TRUE(l_errs[0].find("'A' -> 'B' -> 'A'") != std::string::npos, "Full chain reported");
}

void test_cycle_via_configure_link()
{
    ASSERT_TRUE(anyErrorContains(
        "@StaticLibrary\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n        self.link += [B]\n"
        "@StaticLibrary\ntarget B(A):\n    def configure(self):\n        self.sources = [\"b.cpp\"]\n", "Circular dependency"), "Cycle through self.link");
}


static std::vector<pyke::Diagnostic> analyzeDiagnostics(const std::string& p_rawSource)
{
    std::string l_source = withProject(p_rawSource);
    pyke::Lexer l_lexer(l_source);
    std::vector<pyke::Token> l_tokens = l_lexer.tokenize();
    pyke::Parser l_parser(l_tokens);
    pyke::Program l_program = l_parser.parse();
    pyke::Analyzer l_analyzer(l_program);
    l_analyzer.analyze();
    return l_analyzer.diagnostics();
}

void test_suggest_attribute_with_column()
{
    // line 1 is the injected project line; the typo is on line 5
    std::vector<pyke::Diagnostic> l_d = analyzeDiagnostics("@Executable\ntarget A():\n    def configure(self):\n        self.sorces = [\"a.cpp\"]\n");
    ASSERT_TRUE(!l_d.empty(), "Has a diagnostic");
    ASSERT_TRUE(l_d[0].message.find("did you mean 'sources'") != std::string::npos, "Suggests sources");
    ASSERT_EQ(l_d[0].line, 5, "Line");
    ASSERT_EQ(l_d[0].column, 14, "Column points at the attribute name");
}

void test_suggest_dependency_and_link()
{
    ASSERT_TRUE(anyErrorContains("from packages import fmt\n@Executable\ntarget A(PRIVATE fm):\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n", "did you mean 'fmt'"), "Dependency suggestion");
    ASSERT_TRUE(anyErrorContains("from packages import Threads\n@Executable\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n        self.link += [Thread]\n", "did you mean 'Threads'"), "Link suggestion");
}

void test_suggest_condition_names_and_values()
{
    ASSERT_TRUE(anyErrorContains("@Executable\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n        if platfrom == \"linux\":\n            self.flags += [\"-x\"]\n", "did you mean 'platform'"), "Variable typo");
    ASSERT_TRUE(anyErrorContains("@Executable\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n        if platform == \"linx\":\n            self.flags += [\"-x\"]\n", "did you mean 'linux'"), "Value typo");
}

void test_boolean_condition_operands_are_validated()
{
    ASSERT_TRUE(anyErrorContains("@Executable\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n        if platform == \"linux\" and not nope:\n            self.flags += [\"-x\"]\n", "unknown variable 'nope'"), "Operand inside and/not");
}

void test_cmake_attribute_validation()
{
    ASSERT_TRUE(analyzeSource("@Executable\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n        self.cmake += [\"message(hi)\"]\n"), "cmake lines are accepted");
    ASSERT_TRUE(anyErrorContains("@Executable\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n        self.cmake += [1]\n", "'cmake' entries must be strings"), "Non-string rejected");
    ASSERT_TRUE(anyErrorContains("@Executable\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n    def install(self):\n        self.cmake = [\"x\"]\n", "only valid inside configure()"), "cmake is configure-only");
}

void test_bare_name_assignment_hint()
{
    ASSERT_TRUE(anyErrorContains("@Executable\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n        flags = [\"-x\"]\n", "declare constants at the top level"), "Hint for bare assignment");
}

void test_variables_usable_in_targets_end_to_end()
{
    ASSERT_TRUE(analyzeSource("w = [\"-Wall\"]\n@Executable\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n        self.flags += w + [\"-Werror\"]\n"), "Variables compose and validate");
}

void test_fetch_components_and_properties()
{
    std::string l_fetch = "from github import \"google/googletest\" as GTest\n";
    std::string l_body = "@Executable\ntarget A(PRIVATE GTest.gtest_main):\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n";
    ASSERT_TRUE(analyzeSource(l_fetch + l_body), "Components are allowed on github imports");
    ASSERT_FALSE(analyzeSource("@Executable\ntarget B():\n    def configure(self):\n        self.sources = [\"b.cpp\"]\n@Executable\ntarget A(PRIVATE B.x):\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n"), "Components are still rejected on local targets");
    ASSERT_FALSE(analyzeSource("from github import \"a/b\" as B, options={\"X\": [1]}\n@Executable\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n"), "Fetch option values must be scalars");
    ASSERT_TRUE(analyzeSource("@SharedLibrary\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n        self.output_name = \"x\"\n        self.soversion = \"1\"\n        self.features = [\"cxx_std_17\"]\n"), "Target properties are valid");
    ASSERT_FALSE(analyzeSource("@HeaderOnly\ntarget A():\n    def configure(self):\n        self.output_name = \"x\"\n"), "output_name is rejected on header-only targets");
    ASSERT_FALSE(analyzeSource("@Executable\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n        self.output_name = [\"x\"]\n"), "output_name must be a string");
}

void test_c_languages()
{
    std::string l_exe = "@Executable\ntarget A():\n    def configure(self):\n        self.sources = [\"a.c\"]\n";
    ASSERT_TRUE(analyzeSource("project(\"T\", lang=\"c11\")\n" + l_exe), "C standard accepted");
    ASSERT_TRUE(analyzeSource("project(\"T\", lang=[\"c99\", \"c++20\"])\n" + l_exe), "Mixed languages accepted");
    ASSERT_FALSE(analyzeSource("project(\"T\", lang=[\"c99\", \"c11\"])\n" + l_exe), "Two C standards rejected");
    ASSERT_FALSE(analyzeSource("project(\"T\", lang=\"c12\")\n" + l_exe), "Unknown C standard rejected");
}

void test_root_level_targets()
{
    std::string l_a = "@Executable(\".\")\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n";
    std::string l_b = "@Executable(\".\")\ntarget B():\n    def configure(self):\n        self.sources = [\"b.cpp\"]\n";
    ASSERT_TRUE(analyzeSource(l_a + l_b), "Several targets may share the project root");
    ASSERT_FALSE(analyzeSource("@Executable(\"..\")\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n"), "Parent directory is still rejected");
}

void test_export_rules()
{
    std::string l_lib = "@StaticLibrary\ntarget L%DEPS%:\n    def configure(self):\n        self.sources = [\"l.cpp\"]\n    def install(self):\n        self.export = \"L\"\n";
    auto l_with = [&](const std::string& p_deps) { std::string l_s = l_lib; l_s.replace(l_s.find("%DEPS%"), 6, p_deps); return l_s; };
    ASSERT_TRUE(analyzeSource(l_with("()")), "Exported static library");
    ASSERT_FALSE(analyzeSource("@Executable\ntarget A():\n    def configure(self):\n        self.sources = [\"a.cpp\"]\n    def install(self):\n        self.export = \"A\"\n"), "Executables can't be exported");
    ASSERT_FALSE(analyzeSource("@StaticLibrary\ntarget L():\n    def configure(self):\n        self.sources = [\"l.cpp\"]\n    def install(self):\n        self.export = \"not valid\"\n"), "Package name must be an identifier");
    std::string l_other = "@StaticLibrary\ntarget Other():\n    def configure(self):\n        self.sources = [\"o.cpp\"]\n";
    ASSERT_FALSE(analyzeSource(l_other + l_with("(PUBLIC Other)")), "Exported targets can't depend on unexported targets");
    ASSERT_FALSE(analyzeSource("from github import \"fmtlib/fmt\" as fmt\n" + l_with("(PUBLIC fmt)")), "Exported targets can't depend on fetched projects");
}

int main()
{
    std::cout << "=== Pyke Analyzer Tests ===" << std::endl;

    RUN_TEST(test_package_versions_and_optional);

    RUN_TEST(test_quality_settings_validation);

    RUN_TEST(test_conditional_github_import);

    RUN_TEST(test_vendor_import);

    RUN_TEST(test_cxx_modules_validation);

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
    RUN_TEST(test_missing_project);
    RUN_TEST(test_invalid_project_fields);
    RUN_TEST(test_unknown_attribute);
    RUN_TEST(test_attribute_type_checks);
    RUN_TEST(test_install_attr_in_configure);
    RUN_TEST(test_no_sources);
    RUN_TEST(test_duplicate_and_escaping_paths);
    RUN_TEST(test_condition_validation);
    RUN_TEST(test_option_validation);
    RUN_TEST(test_cycle_reported_once_with_chain);
    RUN_TEST(test_cycle_via_configure_link);
    RUN_TEST(test_suggest_attribute_with_column);
    RUN_TEST(test_suggest_dependency_and_link);
    RUN_TEST(test_suggest_condition_names_and_values);
    RUN_TEST(test_boolean_condition_operands_are_validated);
    RUN_TEST(test_cmake_attribute_validation);
    RUN_TEST(test_bare_name_assignment_hint);
    RUN_TEST(test_variables_usable_in_targets_end_to_end);
    RUN_TEST(test_fetch_components_and_properties);
    RUN_TEST(test_c_languages);
    RUN_TEST(test_root_level_targets);
    RUN_TEST(test_export_rules);

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
