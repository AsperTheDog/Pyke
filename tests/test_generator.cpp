// These unit tests were automatically generated using AI

#include "lexer.hpp"
#include "parser.hpp"
#include "analyzer.hpp"
#include "generator.hpp"
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

#define ASSERT_EQ(actual, expected, msg) \
    do { \
        if ((actual) != (expected)) { \
            std::ostringstream oss; \
            oss << msg << ":\n--- expected ---\n" << (expected) << "\n--- got ---\n" << (actual); \
            s_failures.push_back({oss.str(), __FILE__, __LINE__}); \
            s_testsFailed++; \
            return; \
        } \
    } while(0)

#define ASSERT_CONTAINS(haystack, needle, msg) \
    do { \
        if ((haystack).find(needle) == std::string::npos) { \
            std::ostringstream oss; \
            oss << msg << ": '" << (needle) << "' not found in:\n" << (haystack); \
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

static std::map<std::string, std::string> generateFrom(const std::string& p_source)
{
    pyke::Lexer l_lexer(p_source);
    std::vector<pyke::Token> l_tokens = l_lexer.tokenize();
    pyke::Parser l_parser(l_tokens);
    pyke::Program l_program = l_parser.parse();

    if (l_parser.hasErrors())
    {
        std::cerr << "    Parse errors:" << std::endl;
        for (const std::string& l_e : l_parser.errors())
        {
            std::cerr << "      " << l_e << std::endl;
        }
    }

    pyke::Generator l_generator(l_program);
    std::vector<pyke::GeneratedFile> l_files = l_generator.generate();

    std::map<std::string, std::string> l_result;
    for (const pyke::GeneratedFile& l_f : l_files)
    {
        l_result[l_f.path] = l_f.content;
    }
    return l_result;
}

void test_gen_root_project()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"MyProject\", version=\"1.0.0\", lang=\"c++20\")\n"
        "\n"
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
    );

    std::string& l_root = l_files["CMakeLists.txt"];
    ASSERT_CONTAINS(l_root, "cmake_minimum_required(VERSION 3.21)", "CMake minimum version");
    ASSERT_CONTAINS(l_root, "project(MyProject VERSION 1.0.0 LANGUAGES CXX)", "Project declaration");
    ASSERT_CONTAINS(l_root, "set(CMAKE_CXX_STANDARD 20)", "C++ standard");
    ASSERT_CONTAINS(l_root, "add_subdirectory(App)", "Subdirectory for App");
}

void test_gen_root_find_package()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "from packages import Boost, fmt\n"
        "\n"
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@Executable\n"
        "target App(PRIVATE fmt):\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
    );

    std::string& l_root = l_files["CMakeLists.txt"];
    ASSERT_CONTAINS(l_root, "find_package(Boost REQUIRED)", "Find Boost");
    ASSERT_CONTAINS(l_root, "find_package(fmt REQUIRED)", "Find fmt");
}

void test_gen_root_options()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "option use_opengl: bool = True\n"
        "option prefix: str = \"/usr/local\"\n"
        "\n"
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
    );

    std::string& l_root = l_files["CMakeLists.txt"];
    ASSERT_CONTAINS(l_root, "option(use_opengl", "Bool option");
    ASSERT_CONTAINS(l_root, "ON)", "Bool default ON");
    ASSERT_CONTAINS(l_root, "set(prefix", "String option");
}

void test_gen_simple_executable()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\", \"app.cpp\"]\n"
    );

    ASSERT_TRUE(l_files.count("App/CMakeLists.txt"), "Should generate App/CMakeLists.txt");
    std::string& l_app = l_files["App/CMakeLists.txt"];
    ASSERT_CONTAINS(l_app, "add_executable(App)", "Add executable");
    ASSERT_CONTAINS(l_app, "target_sources(App PRIVATE", "Target sources");
    ASSERT_CONTAINS(l_app, "\"main.cpp\"", "Source file");
}

void test_gen_shared_library()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@SharedLibrary\n"
        "target Core():\n"
        "    def configure(self):\n"
        "        self.sources = [\"src/*.cpp\"]\n"
        "        self.exports.includes = [\"include/\"]\n"
    );

    std::string& l_core = l_files["Core/CMakeLists.txt"];
    ASSERT_CONTAINS(l_core, "add_library(Core SHARED)", "Shared library");
    ASSERT_CONTAINS(l_core, "file(GLOB Core_SOURCES_0 CONFIGURE_DEPENDS", "Glob for sources with CONFIGURE_DEPENDS");
    ASSERT_CONTAINS(l_core, "target_sources(Core PRIVATE ${Core_SOURCES_0})", "Glob variable");
    ASSERT_CONTAINS(l_core, "target_include_directories(Core PUBLIC", "Public includes");
}

void test_gen_static_library()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@StaticLibrary\n"
        "target Lib():\n"
        "    def configure(self):\n"
        "        self.sources = [\"lib.cpp\"]\n"
    );

    std::string& l_lib = l_files["Lib/CMakeLists.txt"];
    ASSERT_CONTAINS(l_lib, "add_library(Lib STATIC)", "Static library");
}

void test_gen_dependencies()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "from packages import Boost\n"
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@SharedLibrary\n"
        "target Core():\n"
        "    def configure(self):\n"
        "        self.sources = [\"core.cpp\"]\n"
        "\n"
        "@Executable\n"
        "target App(PRIVATE Core, PUBLIC Boost.System):\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
    );

    std::string& l_app = l_files["App/CMakeLists.txt"];
    ASSERT_CONTAINS(l_app, "target_link_libraries(App PRIVATE Core)", "Link Core");
    ASSERT_CONTAINS(l_app, "target_link_libraries(App PUBLIC Boost::system)", "Link Boost::system");
}

void test_gen_decorator_path()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@SharedLibrary(\"libs/renderer\")\n"
        "target Renderer():\n"
        "    def configure(self):\n"
        "        self.sources = [\"src/*.cpp\"]\n"
    );

    ASSERT_TRUE(l_files.count("libs/renderer/CMakeLists.txt"), "Custom path");
    std::string& l_root = l_files["CMakeLists.txt"];
    ASSERT_CONTAINS(l_root, "add_subdirectory(libs/renderer)", "Subdirectory with custom path");
}

void test_gen_definitions()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@SharedLibrary\n"
        "target Lib():\n"
        "    def configure(self):\n"
        "        self.sources = [\"lib.cpp\"]\n"
        "        self.exports.definitions = {\"LIB_API\": 1}\n"
    );

    std::string& l_lib = l_files["Lib/CMakeLists.txt"];
    ASSERT_CONTAINS(l_lib, "target_compile_definitions(Lib PUBLIC LIB_API=1)", "Public definition");
}

void test_gen_flags()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
        "        self.flags = [\"-Wall\", \"-Wextra\"]\n"
    );

    std::string& l_app = l_files["App/CMakeLists.txt"];
    ASSERT_CONTAINS(l_app, "target_compile_options(App PRIVATE", "Compile options");
    ASSERT_CONTAINS(l_app, "\"-Wall\"", "Wall flag");
}

void test_gen_if_platform()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
        "        if platform == \"windows\":\n"
        "            self.sources += [\"win.cpp\"]\n"
    );

    std::string& l_app = l_files["App/CMakeLists.txt"];
    ASSERT_CONTAINS(l_app, "if(WIN32)", "Platform condition");
    ASSERT_CONTAINS(l_app, "endif()", "Endif");
}

void test_gen_if_compiler()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
        "        if compiler == \"msvc\":\n"
        "            self.flags += [\"/W4\"]\n"
        "        else:\n"
        "            self.flags += [\"-Wall\"]\n"
    );

    std::string& l_app = l_files["App/CMakeLists.txt"];
    ASSERT_CONTAINS(l_app, "if(MSVC)", "MSVC condition");
    ASSERT_CONTAINS(l_app, "else()", "Else");
    ASSERT_CONTAINS(l_app, "endif()", "Endif");
}

void test_gen_if_build_type()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
        "        if build_type == \"debug\":\n"
        "            self.definitions[\"DEBUG\"] = True\n"
    );

    std::string& l_app = l_files["App/CMakeLists.txt"];
    ASSERT_CONTAINS(l_app, "$<CONFIG:Debug>", "Build type as generator expression");
}

void test_gen_if_option()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "option use_opengl: bool = True\n"
        "\n"
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
        "        if use_opengl:\n"
        "            self.sources += [\"gl.cpp\"]\n"
    );

    std::string& l_app = l_files["App/CMakeLists.txt"];
    ASSERT_CONTAINS(l_app, "if(${use_opengl})", "Option condition");
}

void test_gen_install()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@SharedLibrary\n"
        "target Lib():\n"
        "    def configure(self):\n"
        "        self.sources = [\"lib.cpp\"]\n"
        "    def install(self):\n"
        "        self.library = \"lib\"\n"
        "        self.headers = (\"include/\", \"include/lib\")\n"
    );

    std::string& l_lib = l_files["Lib/CMakeLists.txt"];
    ASSERT_CONTAINS(l_lib, "install(TARGETS Lib LIBRARY DESTINATION \"lib\")", "Library install");
    ASSERT_CONTAINS(l_lib, "install(DIRECTORY \"include/\" DESTINATION \"include/lib\")", "Headers install");
}

void test_gen_index_assignment()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
        "        self.definitions[\"KEY\"] = True\n"
    );

    std::string& l_app = l_files["App/CMakeLists.txt"];
    ASSERT_CONTAINS(l_app, "target_compile_definitions(App PRIVATE KEY=1)", "Index definition");
}

void test_gen_full_example()
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

    std::map<std::string, std::string> l_files = generateFrom(l_source);

    ASSERT_EQ(l_files.size(), size_t(4), "Should have 4 files");
    ASSERT_TRUE(l_files.count("CMakeLists.txt"), "Root");
    ASSERT_TRUE(l_files.count("Core/CMakeLists.txt"), "Core");
    ASSERT_TRUE(l_files.count("libs/renderer/CMakeLists.txt"), "Renderer");
    ASSERT_TRUE(l_files.count("Game/CMakeLists.txt"), "Game");

    std::string& l_root = l_files["CMakeLists.txt"];
    ASSERT_CONTAINS(l_root, "project(GameEngine", "Project name");
    ASSERT_CONTAINS(l_root, "find_package(Boost REQUIRED COMPONENTS system)", "Find Boost with components");
    ASSERT_CONTAINS(l_root, "find_package(fmt REQUIRED)", "Find fmt");
    ASSERT_CONTAINS(l_root, "find_package(OpenGL REQUIRED)", "Find OpenGL");
    ASSERT_CONTAINS(l_root, "option(use_opengl", "Option");
    ASSERT_CONTAINS(l_root, "add_subdirectory(Core)", "Core subdir");
    ASSERT_CONTAINS(l_root, "add_subdirectory(libs/renderer)", "Renderer subdir");
    ASSERT_CONTAINS(l_root, "add_subdirectory(Game)", "Game subdir");

    std::string& l_core = l_files["Core/CMakeLists.txt"];
    ASSERT_CONTAINS(l_core, "add_library(Core SHARED)", "Core is shared");
    ASSERT_CONTAINS(l_core, "target_include_directories(Core PUBLIC", "Public includes");
    ASSERT_CONTAINS(l_core, "target_compile_definitions(Core PUBLIC CORE_API=1)", "Public definition");

    std::string& l_renderer = l_files["libs/renderer/CMakeLists.txt"];
    ASSERT_CONTAINS(l_renderer, "add_library(Renderer SHARED)", "Renderer is shared");
    ASSERT_CONTAINS(l_renderer, "target_link_libraries(Renderer PRIVATE Core)", "Link Core");
    ASSERT_CONTAINS(l_renderer, "target_link_libraries(Renderer PUBLIC Boost::system)", "Link Boost");
    ASSERT_CONTAINS(l_renderer, "if(${use_opengl})", "OpenGL condition");
    ASSERT_CONTAINS(l_renderer, "if(MSVC)", "MSVC condition");
    ASSERT_CONTAINS(l_renderer, "install(TARGETS Renderer LIBRARY DESTINATION \"lib\")", "Install");

    std::string& l_game = l_files["Game/CMakeLists.txt"];
    ASSERT_CONTAINS(l_game, "add_executable(Game)", "Game is executable");
    ASSERT_CONTAINS(l_game, "target_link_libraries(Game PRIVATE Core)", "Link Core");
    ASSERT_CONTAINS(l_game, "target_link_libraries(Game PRIVATE Renderer)", "Link Renderer");
    ASSERT_CONTAINS(l_game, "target_link_libraries(Game PRIVATE fmt::fmt)", "Link fmt");
}

void test_gen_source_groups_default()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
    );

    std::string& l_app = l_files["App/CMakeLists.txt"];
    ASSERT_CONTAINS(l_app, "source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}", "Source groups on by default");
    ASSERT_CONTAINS(l_app, "get_target_property(App_ALL_SOURCES App SOURCES)", "Get sources for grouping");
}

void test_gen_source_groups_disabled()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@Executable(source_groups=False)\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
    );

    std::string& l_app = l_files["App/CMakeLists.txt"];
    ASSERT_TRUE(l_app.find("source_group") == std::string::npos, "Source groups should be absent when disabled");
}

void test_gen_decorator_path_and_source_groups()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@SharedLibrary(\"libs/core\", source_groups=False)\n"
        "target Core():\n"
        "    def configure(self):\n"
        "        self.sources = [\"src/*.cpp\"]\n"
    );

    ASSERT_TRUE(l_files.count("libs/core/CMakeLists.txt"), "Custom path");
    std::string& l_core = l_files["libs/core/CMakeLists.txt"];
    ASSERT_TRUE(l_core.find("source_group") == std::string::npos, "Source groups disabled");
}

void test_gen_decorator_named_path()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@SharedLibrary(path=\"libs/net\")\n"
        "target Net():\n"
        "    def configure(self):\n"
        "        self.sources = [\"net.cpp\"]\n"
    );

    ASSERT_TRUE(l_files.count("libs/net/CMakeLists.txt"), "Named path= arg");
}

void test_gen_copy_dlls_default_executable()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@Executable\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
    );

    std::string& l_app = l_files["App/CMakeLists.txt"];
    ASSERT_CONTAINS(l_app, "add_custom_command(TARGET App POST_BUILD", "Copy DLLs post-build");
    ASSERT_CONTAINS(l_app, "TARGET_RUNTIME_DLLS:App", "Uses TARGET_RUNTIME_DLLS");
    ASSERT_CONTAINS(l_app, "COMMAND_EXPAND_LISTS", "Expands list");
}

void test_gen_copy_dlls_disabled()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@Executable(copy_dlls=False)\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
    );

    std::string& l_app = l_files["App/CMakeLists.txt"];
    ASSERT_TRUE(l_app.find("POST_BUILD") == std::string::npos, "No copy DLLs when disabled");
}

void test_gen_copy_dlls_not_on_library()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@SharedLibrary\n"
        "target Lib():\n"
        "    def configure(self):\n"
        "        self.sources = [\"lib.cpp\"]\n"
    );

    std::string& l_lib = l_files["Lib/CMakeLists.txt"];
    ASSERT_TRUE(l_lib.find("POST_BUILD") == std::string::npos, "No copy DLLs for libraries");
}

void test_gen_copy_files()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@Executable(copy_dlls=False)\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
        "        self.copy_files = [\"vendor/lib.dll\", \"vendor/other.dll\"]\n"
    );

    std::string& l_app = l_files["App/CMakeLists.txt"];
    ASSERT_CONTAINS(l_app, "copy_if_different", "Should have copy command");
    ASSERT_CONTAINS(l_app, "\"vendor/lib.dll\"", "First DLL");
    ASSERT_CONTAINS(l_app, "\"vendor/other.dll\"", "Second DLL");
    ASSERT_CONTAINS(l_app, "$<TARGET_FILE_DIR:App>", "Copies to exe dir");
}

void test_gen_copy_files_conditional()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@Executable(copy_dlls=False)\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
        "        if build_type == \"debug\":\n"
        "            self.copy_files = [\"vendor/debug/lib.dll\"]\n"
        "        else:\n"
        "            self.copy_files = [\"vendor/release/lib.dll\"]\n"
    );

    std::string& l_app = l_files["App/CMakeLists.txt"];
    ASSERT_CONTAINS(l_app, "vendor/debug/lib.dll", "Debug DLL");
    ASSERT_CONTAINS(l_app, "vendor/release/lib.dll", "Release DLL");
}

void test_gen_env_variable()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "from env import VULKAN_SDK\n"
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@Executable(copy_dlls=False)\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
        "        self.includes = [VULKAN_SDK + \"/Include\"]\n"
    );

    std::string& l_app = l_files["App/CMakeLists.txt"];
    ASSERT_CONTAINS(l_app, "$ENV{VULKAN_SDK}/Include", "Env variable in include path");
}

void test_gen_link_dirs()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "from env import VULKAN_SDK\n"
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@Executable(copy_dlls=False)\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
        "        self.link_dirs = [VULKAN_SDK + \"/Lib\"]\n"
    );

    std::string& l_app = l_files["App/CMakeLists.txt"];
    ASSERT_CONTAINS(l_app, "target_link_directories(App PRIVATE", "Link directories");
    ASSERT_CONTAINS(l_app, "$ENV{VULKAN_SDK}/Lib", "Env var in link dir");
}

void test_gen_string_concat()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "from env import SDK_PATH\n"
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@Executable(copy_dlls=False)\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
        "        self.copy_files = [SDK_PATH + \"/bin/\" + \"lib.dll\"]\n"
    );

    std::string& l_app = l_files["App/CMakeLists.txt"];
    ASSERT_CONTAINS(l_app, "$ENV{SDK_PATH}/bin/lib.dll", "Chained string concat");
}

void test_gen_env_not_in_root()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "from env import VULKAN_SDK\n"
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@Executable(copy_dlls=False)\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
    );

    std::string& l_root = l_files["CMakeLists.txt"];
    ASSERT_TRUE(l_root.find("find_package") == std::string::npos, "Env vars should not generate find_package");
}

void test_gen_ctest()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@Executable(test=True, copy_dlls=False, source_groups=False)\n"
        "target MyTest():\n"
        "    def configure(self):\n"
        "        self.sources = [\"test.cpp\"]\n"
    );

    std::string& l_root = l_files["CMakeLists.txt"];
    ASSERT_CONTAINS(l_root, "enable_testing()", "Root has enable_testing");

    std::string& l_test = l_files["MyTest/CMakeLists.txt"];
    ASSERT_CONTAINS(l_test, "add_test(NAME MyTest COMMAND MyTest)", "Test registered");
}

void test_gen_unique_glob_names()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@Executable(copy_dlls=False, source_groups=False)\n"
        "target App():\n"
        "    def configure(self):\n"
        "        self.sources = [\"src/*.cpp\"]\n"
        "        if platform == \"windows\":\n"
        "            self.sources += [\"src/win/*.cpp\"]\n"
    );

    std::string& l_app = l_files["App/CMakeLists.txt"];
    ASSERT_CONTAINS(l_app, "App_SOURCES_0", "First glob");
    ASSERT_CONTAINS(l_app, "App_SOURCES_1", "Second glob (unique)");
}

void test_gen_fetch_content()
{
    std::map<std::string, std::string> l_files = generateFrom(
        "from github import \"fmtlib/fmt\" as fmt, tag=\"10.2.1\"\n"
        "from github import \"gabime/spdlog\" as spdlog\n"
        "project(\"Test\", version=\"1.0\", lang=\"c++17\")\n"
        "\n"
        "@Executable(copy_dlls=False, source_groups=False)\n"
        "target App(PRIVATE fmt, PRIVATE spdlog):\n"
        "    def configure(self):\n"
        "        self.sources = [\"main.cpp\"]\n"
    );

    std::string& l_root = l_files["CMakeLists.txt"];
    ASSERT_CONTAINS(l_root, "include(FetchContent)", "FetchContent included");
    ASSERT_CONTAINS(l_root, "FetchContent_Declare(fmt", "Declare fmt");
    ASSERT_CONTAINS(l_root, "https://github.com/fmtlib/fmt.git", "fmt repo URL");
    ASSERT_CONTAINS(l_root, "GIT_TAG 10.2.1", "fmt tag");
    ASSERT_CONTAINS(l_root, "FetchContent_Declare(spdlog", "Declare spdlog");
    ASSERT_CONTAINS(l_root, "FetchContent_MakeAvailable(fmt)", "Make fmt available");
    ASSERT_CONTAINS(l_root, "FetchContent_MakeAvailable(spdlog)", "Make spdlog available");

    std::string& l_app = l_files["App/CMakeLists.txt"];
    ASSERT_CONTAINS(l_app, "target_link_libraries(App PRIVATE fmt)", "Bare name for fetched");
    ASSERT_CONTAINS(l_app, "target_link_libraries(App PRIVATE spdlog)", "Bare name for fetched");
}

int main()
{
    std::cout << "=== Pyke Generator Tests ===" << std::endl;

    RUN_TEST(test_gen_root_project);
    RUN_TEST(test_gen_root_find_package);
    RUN_TEST(test_gen_root_options);
    RUN_TEST(test_gen_simple_executable);
    RUN_TEST(test_gen_shared_library);
    RUN_TEST(test_gen_static_library);
    RUN_TEST(test_gen_dependencies);
    RUN_TEST(test_gen_decorator_path);
    RUN_TEST(test_gen_definitions);
    RUN_TEST(test_gen_flags);
    RUN_TEST(test_gen_if_platform);
    RUN_TEST(test_gen_if_compiler);
    RUN_TEST(test_gen_if_build_type);
    RUN_TEST(test_gen_if_option);
    RUN_TEST(test_gen_install);
    RUN_TEST(test_gen_index_assignment);
    RUN_TEST(test_gen_full_example);
    RUN_TEST(test_gen_source_groups_default);
    RUN_TEST(test_gen_source_groups_disabled);
    RUN_TEST(test_gen_decorator_path_and_source_groups);
    RUN_TEST(test_gen_decorator_named_path);
    RUN_TEST(test_gen_copy_dlls_default_executable);
    RUN_TEST(test_gen_copy_dlls_disabled);
    RUN_TEST(test_gen_copy_dlls_not_on_library);
    RUN_TEST(test_gen_copy_files);
    RUN_TEST(test_gen_copy_files_conditional);
    RUN_TEST(test_gen_env_variable);
    RUN_TEST(test_gen_link_dirs);
    RUN_TEST(test_gen_string_concat);
    RUN_TEST(test_gen_env_not_in_root);
    RUN_TEST(test_gen_ctest);
    RUN_TEST(test_gen_unique_glob_names);
    RUN_TEST(test_gen_fetch_content);

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
