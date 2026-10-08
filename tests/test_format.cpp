#include "format.hpp"
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
            oss << msg << ": expected '" << expected << "', got '" << actual << "'"; \
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

static std::string fmt(const std::string& p_source)
{
    std::string l_error;
    std::string l_out = pyke::formatSource(p_source, &l_error);
    if (!l_error.empty()) return "ERROR: " + l_error;
    return l_out;
}

void test_fmt_whitespace_and_blank_lines()
{
    ASSERT_EQ(fmt("project(\"T\")   \r\n\r\n\r\n\r\ntarget a():\n"), "project(\"T\")\n\ntarget a():\n", "Trailing space, CRLF, blank lines");
}

void test_fmt_indentation()
{
    std::string l_in = "@Executable(\"a\")\ntarget a():\n\tdef configure(self):\n\t\tself.sources = [\"m.cpp\"]\n";
    ASSERT_EQ(fmt(l_in), "@Executable(\"a\")\ntarget a():\n    def configure(self):\n        self.sources = [\"m.cpp\"]\n", "Tabs become 4-space levels");
    std::string l_two = "target a():\n  def configure(self):\n    if x:\n      self.a = 1\n";
    ASSERT_EQ(fmt(l_two), "target a():\n    def configure(self):\n        if x:\n            self.a = 1\n", "2-space indentation is re-leveled");
}

void test_fmt_spacing()
{
    ASSERT_EQ(fmt("project( \"T\",version=\"1.0\" ,lang = \"c++20\" )\n"), "project(\"T\", version=\"1.0\", lang=\"c++20\")\n", "kwargs and commas");
    ASSERT_EQ(fmt("target a():\n    def configure(self):\n        self.sources=[ \"a.cpp\",\"b.cpp\" ]\n        self.cmake+=[\"x\"]\n"),
              "target a():\n    def configure(self):\n        self.sources = [\"a.cpp\", \"b.cpp\"]\n        self.cmake += [\"x\"]\n", "assignment and brackets");
    ASSERT_EQ(fmt("target a():\n    def configure(self):\n        if platform==\"linux\" and x!=1:\n            self.a = {\"K\":1,\"L\" : \"v\"}\n"),
              "target a():\n    def configure(self):\n        if platform == \"linux\" and x != 1:\n            self.a = {\"K\": 1, \"L\": \"v\"}\n", "comparisons and dicts");
}

void test_fmt_comments_and_strings_untouched()
{
    ASSERT_EQ(fmt("x = \"a,b  =  c\"   # keep  this,text\n"), "x = \"a,b  =  c\"  # keep  this,text\n", "Strings and comments keep their text");
    ASSERT_EQ(fmt("# top\ntarget a():\n    # note\n    def configure(self):\n        self.a = 1\n"),
              "# top\ntarget a():\n    # note\n    def configure(self):\n        self.a = 1\n", "Comments keep their level");
}

void test_fmt_multiline_lists_keep_alignment()
{
    std::string l_in = "target a():\n    def configure(self):\n        self.sources = [\n            \"a.cpp\",\n            \"b.cpp\"\n        ]\n";
    ASSERT_EQ(fmt(l_in), l_in, "Already-formatted multi-line list is unchanged");
}

void test_fmt_import_keywords()
{
    ASSERT_EQ(fmt("from github import \"a/b\" as b,tag = \"1\" , options={\"X\":False}\n"), "from github import \"a/b\" as b, tag=\"1\", options={\"X\": False}\n", "Import keyword arguments");
}

void test_fmt_is_idempotent()
{
    std::string l_in = "project( \"T\" ,lang=\"c++17\")\n\n\nfrom packages import  Boost(1.78) , Threads\n@Executable( \"a\" )\ntarget a(PRIVATE  Threads):\n\tdef configure(self):\n\t\tself.sources=[\"m.cpp\"]  \n";
    std::string l_once = fmt(l_in);
    ASSERT_EQ(fmt(l_once), l_once, "Formatting twice changes nothing");
}

int main()
{
    std::cout << "Running format tests..." << std::endl;
    RUN_TEST(test_fmt_whitespace_and_blank_lines);
    RUN_TEST(test_fmt_indentation);
    RUN_TEST(test_fmt_spacing);
    RUN_TEST(test_fmt_comments_and_strings_untouched);
    RUN_TEST(test_fmt_multiline_lists_keep_alignment);
    RUN_TEST(test_fmt_import_keywords);
    RUN_TEST(test_fmt_is_idempotent);

    std::cout << std::endl << "Results: " << s_testsPassed << "/" << s_testsRun << " passed";
    if (s_testsFailed > 0)
    {
        std::cout << " (" << s_testsFailed << " failed)" << std::endl << std::endl << "Failures:" << std::endl;
        for (const TestFailure& l_f : s_failures) std::cout << "  " << l_f.file << ":" << l_f.line << " - " << l_f.message << std::endl;
        return 1;
    }
    std::cout << std::endl;
    return 0;
}
