#include "lexer.hpp"
#include "parser.hpp"
#include "analyzer.hpp"
#include "generator.hpp"
#include "error.hpp"
#include "format.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

static std::string toIdentifier(const std::string& p_name)
{
    std::string l_out;
    for (unsigned char l_c : p_name) l_out += (std::isalnum(l_c) || l_c == '_') ? static_cast<char>(l_c) : '_';
    if (l_out.empty() || std::isdigit(static_cast<unsigned char>(l_out[0]))) l_out.insert(l_out.begin(), '_');
    return l_out;
}

static bool isIgnoredDir(const std::string& p_name)
{
    return (!p_name.empty() && p_name[0] == '.') || p_name == "node_modules" || p_name == "CMakeFiles" ||
           p_name == "build" || p_name.rfind("build-", 0) == 0 || p_name.rfind("cmake-build", 0) == 0 || p_name == "out";
}

int runInit(const std::string& p_dir)
{
    fs::path l_root(p_dir);
    if (!fs::exists(l_root))
    {
        std::cerr << "Error: Directory does not exist: " << p_dir << std::endl;
        return 1;
    }

    std::vector<std::string> l_sources;
    std::vector<std::string> l_headers;

    fs::recursive_directory_iterator l_iter(l_root, fs::directory_options::skip_permission_denied);
    for (; l_iter != fs::recursive_directory_iterator(); ++l_iter)
    {
        const fs::directory_entry& l_entry = *l_iter;
        if (l_entry.is_directory())
        {
            if (isIgnoredDir(l_entry.path().filename().string())) l_iter.disable_recursion_pending();
            continue;
        }
        if (!l_entry.is_regular_file()) continue;
        std::string l_ext = l_entry.path().extension().string();
        std::string l_rel = fs::relative(l_entry.path(), l_root).generic_string();

        if (l_ext == ".cpp" || l_ext == ".cc" || l_ext == ".cxx" || l_ext == ".c")
        {
            l_sources.push_back(l_rel);
        }
        else if (l_ext == ".h" || l_ext == ".hpp" || l_ext == ".hxx")
        {
            l_headers.push_back(l_rel);
        }
    }

    if (l_sources.empty())
    {
        std::cerr << "No source files found in: " << p_dir << std::endl;
        return 1;
    }

    std::sort(l_sources.begin(), l_sources.end());
    std::sort(l_headers.begin(), l_headers.end());

    std::string l_projectName = l_root.filename().string();
    if (l_projectName.empty() || l_projectName == "." || l_projectName == "..")
    {
        l_projectName = fs::weakly_canonical(l_root).filename().string();
    }
    std::string l_targetName = toIdentifier(l_projectName);
    l_projectName = l_targetName;

    std::set<std::string> l_sourceDirs;
    for (const std::string& l_s : l_sources)
    {
        std::string l_parent = fs::path(l_s).parent_path().generic_string();
        if (!l_parent.empty()) l_sourceDirs.insert(l_parent);
    }

    std::set<std::string> l_includeDirs;
    for (const std::string& l_h : l_headers)
    {
        std::string l_parent = fs::path(l_h).parent_path().generic_string();
        if (!l_parent.empty()) l_includeDirs.insert(l_parent);
    }

    bool l_useGlob = l_sources.size() > 5;

    std::ostringstream l_out;
    l_out << "project(\"" << l_projectName << "\", version=\"0.1.0\", lang=\"c++20\")\n\n";

    bool l_hasMain = false;
    for (const std::string& l_s : l_sources)
    {
        if (fs::path(l_s).filename().string() == "main.cpp" || fs::path(l_s).filename().string() == "main.cc")
        {
            l_hasMain = true;
            break;
        }
    }

    std::string l_targetType = l_hasMain ? "@Executable" : "@SharedLibrary";
    l_out << l_targetType << "\n";
    l_out << "target " << l_targetName << "():\n";
    l_out << "    def configure(self):\n";

    if (l_useGlob)
    {
        std::set<std::string> l_globs;
        for (const std::string& l_s : l_sources)
        {
            std::string l_parent = fs::path(l_s).parent_path().generic_string();
            std::string l_ext = fs::path(l_s).extension().string();
            l_globs.insert((l_parent.empty() ? "" : l_parent + "/") + "*" + l_ext);
        }
        l_out << "        self.sources = [";
        bool l_first = true;
        for (const std::string& l_g : l_globs)
        {
            if (!l_first) l_out << ", ";
            l_out << "\"" << l_g << "\"";
            l_first = false;
        }
        l_out << "]\n";
    }
    else
    {
        l_out << "        self.sources = [\n";
        for (size_t l_i = 0; l_i < l_sources.size(); l_i++)
        {
            l_out << "            \"" << l_sources[l_i] << "\"";
            if (l_i + 1 < l_sources.size()) l_out << ",";
            l_out << "\n";
        }
        l_out << "        ]\n";
    }

    if (!l_includeDirs.empty())
    {
        l_out << "        self.includes = [";
        bool l_first = true;
        for (const std::string& l_d : l_includeDirs)
        {
            if (!l_first) l_out << ", ";
            l_out << "\"" << l_d << "/\"";
            l_first = false;
        }
        l_out << "]\n";
    }

    std::string l_outputFile = (l_root / (l_root.filename().string().empty() || l_root.filename() == "." ? l_projectName : l_root.filename().string())).string() + ".pyke";
    if (fs::exists(l_outputFile))
    {
        std::cerr << "Error: " << l_outputFile << " already exists; refusing to overwrite it" << std::endl;
        return 1;
    }
    std::ofstream l_file(l_outputFile);
    if (!l_file.is_open())
    {
        std::cerr << "Error: Could not write: " << l_outputFile << std::endl;
        return 1;
    }
    l_file << l_out.str();
    std::cout << "Generated: " << l_outputFile << std::endl;
    return 0;
}

// Matches one path component against a pattern with * and ?
bool globMatch(const std::string& p_pattern, const std::string& p_text, size_t p_pi = 0, size_t p_ti = 0)
{
    while (p_pi < p_pattern.size())
    {
        if (p_pattern[p_pi] == '*')
        {
            for (size_t l_skip = p_ti; l_skip <= p_text.size(); ++l_skip)
            {
                if (globMatch(p_pattern, p_text, p_pi + 1, l_skip)) return true;
            }
            return false;
        }
        if (p_ti >= p_text.size()) return false;
        if (p_pattern[p_pi] != '?' && p_pattern[p_pi] != p_text[p_ti]) return false;
        ++p_pi;
        ++p_ti;
    }
    return p_ti == p_text.size();
}

// Files under p_root matching a pattern like "src/*.cpp" (wildcards allowed in any component)
void globExpand(const fs::path& p_dir, const std::vector<std::string>& p_parts, size_t p_index, std::vector<fs::path>& p_out)
{
    std::error_code l_ec;
    if (p_index == p_parts.size()) return;
    const std::string& l_part = p_parts[p_index];
    bool l_last = p_index + 1 == p_parts.size();
    if (l_part == "**")
    {
        // zero or more folders
        globExpand(p_dir, p_parts, p_index + 1, p_out);
        for (fs::directory_iterator l_it(p_dir, l_ec), l_end; !l_ec && l_it != l_end; l_it.increment(l_ec))
        {
            if (l_it->is_directory(l_ec)) globExpand(l_it->path(), p_parts, p_index, p_out);
        }
        return;
    }
    if (l_part.find_first_of("*?") == std::string::npos)
    {
        fs::path l_next = p_dir / l_part;
        if (l_last) { if (fs::is_regular_file(l_next, l_ec)) p_out.push_back(l_next); }
        else globExpand(l_next, p_parts, p_index + 1, p_out);
        return;
    }
    for (fs::directory_iterator l_it(p_dir, l_ec), l_end; !l_ec && l_it != l_end; l_it.increment(l_ec))
    {
        if (!globMatch(l_part, l_it->path().filename().string())) continue;
        if (l_last) { if (l_it->is_regular_file(l_ec)) p_out.push_back(l_it->path()); }
        else if (l_it->is_directory(l_ec)) globExpand(l_it->path(), p_parts, p_index + 1, p_out);
    }
}

bool isCompiledSource(const fs::path& p_path)
{
    static const std::set<std::string> s_exts = {".c", ".cc", ".cpp", ".cxx", ".c++", ".cppm", ".ixx", ".mpp", ".cxxm", ".c++m", ".ccm"};
    return s_exts.count(p_path.extension().string()) > 0;
}

// Warns about globs that match nothing and about source files next to a target that none of its patterns pick up
void checkSources(const pyke::Generator& p_generator, const fs::path& p_outRoot)
{
    std::set<std::string> l_targetDirs;
    for (const pyke::Generator::SourceSet& l_set : p_generator.sourceSets()) l_targetDirs.insert(l_set.targetDir);

    for (const pyke::Generator::SourceSet& l_set : p_generator.sourceSets())
    {
        std::set<std::string> l_covered;
        for (const std::string& l_entry : l_set.entries)
        {
            if (l_entry.find_first_of("*?") == std::string::npos)
            {
                l_covered.insert(l_entry);
                continue;
            }
            std::vector<std::string> l_parts;
            std::stringstream l_stream(l_entry);
            for (std::string l_p; std::getline(l_stream, l_p, '/');) if (!l_p.empty()) l_parts.push_back(l_p);
            std::vector<fs::path> l_found;
            globExpand(p_outRoot, l_parts, 0, l_found);
            if (l_found.empty())
            {
                std::cerr << "warning: target '" << l_set.target << "': pattern \"" << l_entry << "\" matches no files; CMake will fail to configure if the target has no other sources" << std::endl;
            }
            for (const fs::path& l_file : l_found) l_covered.insert(fs::relative(l_file, p_outRoot).generic_string());
        }

        // Files in the target's own folder that no entry mentions (skipped for the project root, where everything lives)
        if (l_set.targetDir == "." || l_set.targetDir.empty()) continue;
        std::error_code l_ec;
        std::vector<std::string> l_missed;
        for (fs::recursive_directory_iterator l_it(p_outRoot / l_set.targetDir, l_ec), l_end; !l_ec && l_it != l_end; l_it.increment(l_ec))
        {
            if (l_it->is_directory(l_ec))
            {
                std::string l_rel = fs::relative(l_it->path(), p_outRoot).generic_string();
                if (l_targetDirs.count(l_rel) || fs::exists(l_it->path() / "CMakeLists.txt", l_ec)) l_it.disable_recursion_pending();
                continue;
            }
            if (!isCompiledSource(l_it->path())) continue;
            std::string l_rel = fs::relative(l_it->path(), p_outRoot).generic_string();
            if (!l_covered.count(l_rel)) l_missed.push_back(l_rel);
        }
        if (!l_missed.empty())
        {
            std::cerr << "warning: target '" << l_set.target << "': " << l_missed.size() << " source file(s) in its folder are not in 'sources': ";
            for (size_t l_i = 0; l_i < l_missed.size() && l_i < 3; ++l_i) std::cerr << (l_i ? ", " : "") << l_missed[l_i];
            if (l_missed.size() > 3) std::cerr << ", ...";
            std::cerr << std::endl;
        }
    }
}

// ---- pyke build / pyke test ------------------------------------------------------------------------------------

struct BuildOptions
{
    bool runTests = false;
    std::string config = "Release";
    std::string target;
    std::string jobs;
    std::string input;
    bool usesModules = false;
    bool importStd = false;
};

std::string shellQuote(const std::string& p_arg)
{
    return "\"" + p_arg + "\"";
}

int runCommand(const std::vector<std::string>& p_args)
{
    std::string l_command;
    for (const std::string& l_arg : p_args) l_command += (l_command.empty() ? "" : " ") + shellQuote(l_arg);
    std::cout << "$ " << l_command << std::endl;
#ifdef _WIN32
    l_command = "\"" + l_command + "\""; // cmd.exe strips the outer quotes
#endif
    return std::system(l_command.c_str());
}

bool toolAvailable(const std::string& p_tool)
{
#ifdef _WIN32
    std::string l_command = p_tool + " --version > nul 2>&1";
#else
    std::string l_command = p_tool + " --version > /dev/null 2>&1";
#endif
    return std::system(l_command.c_str()) == 0;
}

// The single .pyke file in the current directory, or an empty string
std::string findProjectFile()
{
    std::vector<std::string> l_found;
    std::error_code l_ec;
    for (fs::directory_iterator l_it(".", l_ec), l_end; !l_ec && l_it != l_end; l_it.increment(l_ec))
    {
        if (l_it->is_regular_file(l_ec) && l_it->path().extension() == ".pyke") l_found.push_back(l_it->path().filename().string());
    }
    if (l_found.size() == 1) return l_found[0];
    if (l_found.empty()) std::cerr << "Error: no .pyke file in the current directory; pass one: pyke build <input.pyke>" << std::endl;
    else std::cerr << "Error: several .pyke files here; pass one: pyke build <input.pyke>" << std::endl;
    return "";
}

// Configure + build (+ ctest) in <root>/build, after the CMake files have been generated into <root>
int runBuild(const BuildOptions& p_opts, const fs::path& p_root)
{
    if (!toolAvailable("cmake"))
    {
        std::cerr << "Error: cmake was not found on PATH; install CMake to build" << std::endl;
        return 1;
    }
    fs::path l_buildDir = p_root / "build";
    bool l_ninja = toolAvailable("ninja");
    if (p_opts.usesModules)
    {
        // C++ modules need a generator that can order compilation by module dependencies: Ninja or Visual Studio
        std::ifstream l_cache(l_buildDir / "CMakeCache.txt");
        std::string l_line, l_generator;
        while (std::getline(l_cache, l_line))
        {
            if (l_line.rfind("CMAKE_GENERATOR:INTERNAL=", 0) == 0) l_generator = l_line.substr(25);
        }
        bool l_ninjaBuild = l_generator.find("Ninja") != std::string::npos;
        bool l_supported = l_generator.empty() ? true : (l_ninjaBuild || (!p_opts.importStd && l_generator.find("Visual Studio") != std::string::npos));
        if (!l_supported)
        {
            std::cerr << "Error: this project uses C++ modules" << (p_opts.importStd ? " with import std" : "") << ", but build/ was configured with '" << l_generator << "', which cannot build them; delete build/ to reconfigure with Ninja" << std::endl;
            return 1;
        }
        if (l_generator.empty() && !l_ninja && p_opts.importStd)
        {
            std::cerr << "Error: import_std needs the Ninja generator (Visual Studio generators cannot build the std module); install ninja" << std::endl;
            return 1;
        }
#ifndef _WIN32
        if (l_generator.empty() && !l_ninja)
        {
            std::cerr << "Error: this project uses C++ modules, which need the Ninja generator; install ninja (not found on PATH)" << std::endl;
            return 1;
        }
#endif
    }
    std::vector<std::string> l_configure = {"cmake", "-S", p_root.string(), "-B", l_buildDir.string(), "-DCMAKE_BUILD_TYPE=" + p_opts.config};
    if (!fs::exists(l_buildDir / "CMakeCache.txt") && l_ninja) l_configure.insert(l_configure.end(), {"-G", "Ninja"});
    if (runCommand(l_configure) != 0) return 1;

    std::vector<std::string> l_build = {"cmake", "--build", l_buildDir.string(), "--config", p_opts.config};
    if (!p_opts.target.empty()) l_build.insert(l_build.end(), {"--target", p_opts.target});
    if (!p_opts.jobs.empty()) l_build.insert(l_build.end(), {"--parallel", p_opts.jobs});
    else l_build.push_back("--parallel");
    if (runCommand(l_build) != 0) return 1;

    if (p_opts.runTests)
    {
        if (runCommand({"ctest", "--test-dir", l_buildDir.string(), "-C", p_opts.config, "--output-on-failure"}) != 0) return 1;
    }
    return 0;
}

// Formats each file in place; with p_check only reports files that are not formatted (exit 1 if any)
int runFmt(const std::vector<std::string>& p_paths, bool p_check)
{
    int l_status = 0;
    for (const std::string& l_path : p_paths)
    {
        std::ifstream l_in(l_path, std::ios::binary);
        if (!l_in.is_open())
        {
            std::cerr << "Error: Could not open file: " << l_path << std::endl;
            l_status = 1;
            continue;
        }
        std::stringstream l_buffer;
        l_buffer << l_in.rdbuf();
        l_in.close();
        std::string l_source = l_buffer.str();

        std::string l_error;
        std::string l_formatted = pyke::formatSource(l_source, &l_error);
        if (!l_error.empty())
        {
            std::cerr << l_path << ": " << l_error << std::endl;
            l_status = 1;
            continue;
        }

        if (l_formatted == l_source)
        {
            if (!p_check) std::cout << "Already formatted: " << l_path << std::endl;
            continue;
        }
        if (p_check)
        {
            std::cout << "Would reformat: " << l_path << std::endl;
            l_status = 1;
            continue;
        }

        std::ofstream l_out(l_path, std::ios::binary);
        if (!l_out.is_open())
        {
            std::cerr << "Error: Could not write: " << l_path << std::endl;
            l_status = 1;
            continue;
        }
        l_out << l_formatted;
        std::cout << "Formatted: " << l_path << std::endl;
    }
    return l_status;
}

int main(int argc, char* argv[])
{
    // --clean is a modifier: remove files an earlier run generated that are no longer produced
    bool l_clean = false;
    bool l_force = false;
    std::vector<char*> l_argvFiltered;
    for (int l_i = 0; l_i < argc; l_i++)
    {
        if (l_i > 0 && std::string(argv[l_i]) == "--clean") l_clean = true;
        else if (l_i > 0 && std::string(argv[l_i]) == "--force") l_force = true;
        else l_argvFiltered.push_back(argv[l_i]);
    }
    argc = static_cast<int>(l_argvFiltered.size());
    argv = l_argvFiltered.data();

    if (argc < 2)
    {
        std::cerr << "Usage: pyke [--clean] [--force] <input.pyke> [output_dir]" << std::endl;
        std::cerr << "       pyke build|test [input.pyke] [--release|--debug] [--target <name>] [-j <jobs>]" << std::endl;
        std::cerr << "       pyke --init [directory]" << std::endl;
        std::cerr << "       pyke --validate <input.pyke>" << std::endl;
        std::cerr << "       pyke --fmt [--check] <input.pyke>..." << std::endl;
        std::cerr << "       pyke --upgrade <input.pyke>" << std::endl;
        return 1;
    }

    std::string l_firstArg = argv[1];

    if (l_firstArg == "--init")
    {
        std::string l_dir = (argc >= 3) ? argv[2] : ".";
        return runInit(l_dir);
    }

    if (l_firstArg == "--upgrade")
    {
        if (argc < 3)
        {
            std::cerr << "Usage: pyke --upgrade <input.pyke>" << std::endl;
            return 1;
        }
        std::ifstream l_uf(argv[2]);
        if (!l_uf.is_open())
        {
            std::cerr << "Error: Could not open: " << argv[2] << std::endl;
            return 1;
        }
        std::stringstream l_ubuf;
        l_ubuf << l_uf.rdbuf();
        pyke::Lexer l_ulexer(l_ubuf.str());
        std::vector<pyke::Token> l_utokens = l_ulexer.tokenize();
        for (const pyke::Token& l_tok : l_utokens)
        {
            if (l_tok.type == pyke::TokenType::ERROR_TOKEN)
            {
                std::cerr << argv[2] << ":" << l_tok.line << ":" << l_tok.column << ": error: " << l_tok.value << std::endl;
                return 1;
            }
        }
        pyke::Parser l_uparser(l_utokens);
        pyke::Program l_uprogram = l_uparser.parse();
        if (l_uparser.hasErrors())
        {
            for (const std::string& l_err : l_uparser.errors()) std::cerr << argv[2] << ": " << l_err << std::endl;
            return 1;
        }

        if (l_uprogram.fetches.empty())
        {
            std::cout << "No GitHub dependencies found." << std::endl;
            return 0;
        }

        std::cout << "GitHub dependencies:" << std::endl;
        for (const pyke::FetchDecl& l_f : l_uprogram.fetches)
        {
            std::cout << "  " << l_f.name << " -> " << l_f.repo;
            if (!l_f.tag.empty())
            {
                std::cout << " @ " << l_f.tag;
            }
            else
            {
                std::cout << " @ (latest)";
            }
            std::cout << "\n    Check: https://github.com/" << l_f.repo << "/releases" << std::endl;
        }
        return 0;
    }

    if (l_firstArg == "--fmt")
    {
        bool l_check = false;
        std::vector<std::string> l_paths;
        for (int l_i = 2; l_i < argc; ++l_i)
        {
            if (std::string(argv[l_i]) == "--check") l_check = true;
            else l_paths.push_back(argv[l_i]);
        }
        if (l_paths.empty())
        {
            std::cerr << "Usage: pyke --fmt [--check] <input.pyke>..." << std::endl;
            return 1;
        }
        return runFmt(l_paths, l_check);
    }

    bool l_buildMode = false;
    BuildOptions l_build;
    if (l_firstArg == "build" || l_firstArg == "test")
    {
        l_buildMode = true;
        l_build.runTests = l_firstArg == "test";
        for (int l_i = 2; l_i < argc; ++l_i)
        {
            std::string l_arg = argv[l_i];
            if (l_arg == "--release") l_build.config = "Release";
            else if (l_arg == "--debug") l_build.config = "Debug";
            else if ((l_arg == "--target" || l_arg == "-j") && l_i + 1 < argc) (l_arg == "-j" ? l_build.jobs : l_build.target) = argv[++l_i];
            else if (!l_arg.empty() && l_arg[0] != '-' && l_build.input.empty()) l_build.input = l_arg;
            else
            {
                std::cerr << "Usage: pyke " << l_firstArg << " [input.pyke] [--release|--debug] [--target <name>] [-j <jobs>]" << std::endl;
                return 1;
            }
        }
        if (l_build.input.empty()) l_build.input = findProjectFile();
        if (l_build.input.empty()) return 1;
    }

    bool l_validateOnly = false;
    if (l_buildMode)
    {
        l_firstArg = l_build.input;
    }
    else if (l_firstArg == "--validate")
    {
        l_validateOnly = true;
        if (argc < 3)
        {
            std::cerr << "Usage: pyke --validate <input.pyke>" << std::endl;
            return 1;
        }
        l_firstArg = argv[2];
    }

    std::string l_inputPath = l_firstArg;
    std::string l_outputDir = l_validateOnly ? "" : ((argc >= 3) ? argv[2] : ".");
    if (l_buildMode)
    {
        l_outputDir = fs::path(l_inputPath).parent_path().string(); // generate next to the .pyke file
        if (l_outputDir.empty()) l_outputDir = ".";
    }

    std::ifstream l_file(l_inputPath);
    if (!l_file.is_open())
    {
        std::cerr << "Error: Could not open file: " << l_inputPath << std::endl;
        return 1;
    }

    std::stringstream l_buffer;
    l_buffer << l_file.rdbuf();
    std::string l_source = l_buffer.str();

    pyke::SourceFile l_srcFile(l_source, l_inputPath);

    pyke::Lexer l_lexer(l_source);
    std::vector<pyke::Token> l_tokens = l_lexer.tokenize();

    for (const pyke::Token& l_token : l_tokens)
    {
        if (l_token.type == pyke::TokenType::ERROR_TOKEN)
        {
            std::cerr << l_srcFile.formatError(l_token.line, l_token.column, l_token.value);
            return 1;
        }
    }

    pyke::Parser l_parser(l_tokens);
    pyke::Program l_program = l_parser.parse();

    auto l_renderParserError = [&](const std::string& p_err)
    {
        // parser messages look like "Line 5:14: text"
        int l_line = 0, l_col = 0;
        std::string l_text = p_err;
        if (std::sscanf(p_err.c_str(), "Line %d:%d: ", &l_line, &l_col) == 2)
        {
            size_t l_pos = p_err.find(": ", p_err.find(':') + 1);
            l_text = l_pos == std::string::npos ? p_err : p_err.substr(l_pos + 2);
        }
        std::cerr << l_srcFile.formatDiagnostic("error", l_line, l_col, l_text);
    };

    if (l_parser.hasSyntaxErrors())
    {
        for (const std::string& l_err : l_parser.errors()) l_renderParserError(l_err);
        std::cerr << l_parser.errors().size() << " error(s); nothing was generated." << std::endl;
        return 1;
    }

    pyke::Analyzer l_analyzer(l_program);
    bool l_analysisOk = l_analyzer.analyze();

    std::vector<pyke::Diagnostic> l_diags = l_analyzer.diagnostics();

    // Non-syntax parser errors (reused names, bad f-string names) don't stop analysis, so one run reports everything
    for (const std::string& l_err : l_parser.errors())
    {
        int l_line = 0, l_col = 0;
        std::string l_text = l_err;
        if (std::sscanf(l_err.c_str(), "Line %d:%d: ", &l_line, &l_col) == 2)
        {
            size_t l_pos = l_err.find(": ", l_err.find(':') + 1);
            l_text = l_pos == std::string::npos ? l_err : l_err.substr(l_pos + 2);
        }
        l_diags.push_back({l_line, l_col, false, l_text});
    }
    std::stable_sort(l_diags.begin(), l_diags.end(), [](const pyke::Diagnostic& p_a, const pyke::Diagnostic& p_b) { return p_a.line < p_b.line; });
    for (const pyke::Diagnostic& l_d : l_diags)
    {
        std::cerr << l_srcFile.formatDiagnostic(l_d.warning ? "warning" : "error", l_d.line, l_d.column, l_d.message);
    }
    if (!l_analysisOk || l_parser.hasErrors())
    {
        std::cerr << (l_analyzer.errors().size() + l_parser.errors().size()) << " error(s); nothing was generated." << std::endl;
        return 1;
    }

    if (l_validateOnly)
    {
        std::cout << l_inputPath << ": OK" << std::endl;
        return 0;
    }

    pyke::Generator l_generator(l_program);
    std::vector<pyke::GeneratedFile> l_files = l_generator.generate();

    std::error_code l_ec;
    // Purely lexical (absolute + normalised) so it behaves the same on every platform; weakly_canonical mishandles ".\\file" on Windows
    auto l_normalize = [](const fs::path& p_path)
    {
        fs::path l_abs = fs::absolute(p_path).lexically_normal();
        if (!l_abs.has_filename()) l_abs = l_abs.parent_path(); // drop the trailing separator left by "dir/."
        return l_abs;
    };
    fs::path l_outRoot = l_normalize(fs::path(l_outputDir));
    auto l_isInsideRoot = [&](const fs::path& p_path)
    {
        std::string l_rel = l_normalize(p_path).lexically_relative(l_outRoot).generic_string();
        return !l_rel.empty() && l_rel != "." && l_rel != ".." && l_rel.rfind("../", 0) != 0;
    };

    const std::string l_headerPrefix = "# Generated by pyke";
    const std::string l_header = l_headerPrefix + " from " + fs::path(l_inputPath).filename().string() + " - do not edit, changes will be overwritten.\n";

    // The manifest remembers what we generated so files from removed targets can be reported
    fs::path l_manifestPath = fs::path(l_outputDir) / ".pyke" / "generated";
    std::set<std::string> l_previous;
    {
        std::ifstream l_in(l_manifestPath);
        std::string l_line;
        while (std::getline(l_in, l_line))
        {
            if (!l_line.empty()) l_previous.insert(l_line);
        }
    }

    // Never overwrite a file pyke didn't write (e.g. a hand-written CMakeLists.txt): it carries our header or is in the manifest
    if (!l_force)
    {
        std::vector<std::string> l_foreign;
        for (const pyke::GeneratedFile& l_genFile : l_files)
        {
            fs::path l_outPath = fs::path(l_outputDir) / l_genFile.path;
            std::ifstream l_in(l_outPath, std::ios::binary);
            if (!l_in.is_open()) continue;
            std::string l_firstLine;
            std::getline(l_in, l_firstLine);
            bool l_empty = l_in.peek() == std::ifstream::traits_type::eof() && l_firstLine.empty();
            bool l_ours = l_firstLine.rfind(l_headerPrefix, 0) == 0 || l_previous.count(fs::path(l_genFile.path).generic_string());
            if (!l_empty && !l_ours) l_foreign.push_back(l_outPath.string());
        }
        if (!l_foreign.empty())
        {
            for (const std::string& l_path : l_foreign) std::cerr << "error: " << l_path << " already exists and was not generated by pyke" << std::endl;
            std::cerr << "Nothing was written. Move those files away, choose another output directory (pyke " << l_inputPath << " <output_dir>), or pass --force to overwrite them." << std::endl;
            return 1;
        }
    }

    int l_written = 0, l_unchanged = 0;
    std::set<std::string> l_generatedPaths;

    for (const pyke::GeneratedFile& l_genFile : l_files)
    {
        fs::path l_outPath = fs::path(l_outputDir) / l_genFile.path;
        if (!l_isInsideRoot(l_outPath))
        {
            std::cerr << "Error: refusing to write outside the output directory: " << l_outPath.string() << std::endl;
            return 1;
        }
        l_generatedPaths.insert(fs::path(l_genFile.path).generic_string());

        // JSON has no comments, so only CMake files carry the header
        std::string l_content = (l_outPath.extension() == ".json" ? "" : l_header + "\n") + l_genFile.content;

        std::string l_existing;
        {
            std::ifstream l_in(l_outPath, std::ios::binary);
            if (l_in.is_open())
            {
                std::stringstream l_buf;
                l_buf << l_in.rdbuf();
                l_existing = l_buf.str();
            }
        }
        if (!l_existing.empty() && l_existing == l_content)
        {
            l_unchanged++;
            std::cout << "  Unchanged: " << l_outPath.string() << std::endl;
            continue;
        }

        fs::path l_parent = l_outPath.parent_path();
        if (!l_parent.empty()) fs::create_directories(l_parent);

        // write beside the target and rename, so an interrupted run never leaves a half-written file
        fs::path l_tmp = l_outPath;
        l_tmp += ".pyke-tmp";
        {
            std::ofstream l_out(l_tmp, std::ios::binary);
            if (!l_out.is_open())
            {
                std::cerr << "Error: Could not write file: " << l_outPath.string() << std::endl;
                return 1;
            }
            l_out << l_content;
        }
        fs::rename(l_tmp, l_outPath, l_ec);
        if (l_ec)
        {
            std::cerr << "Error: Could not replace file: " << l_outPath.string() << ": " << l_ec.message() << std::endl;
            return 1;
        }
        l_written++;
        std::cout << (l_existing.empty() ? "  Generated: " : "  Updated:   ") << l_outPath.string() << std::endl;
    }

    std::cout << "Done. " << l_written << " written, " << l_unchanged << " unchanged." << std::endl;

    std::set<std::string> l_stillOwned = l_generatedPaths;
    for (const std::string& l_old : l_previous)
    {
        if (l_generatedPaths.count(l_old)) continue;
        fs::path l_oldPath = fs::path(l_outputDir) / l_old;
        if (!fs::exists(l_oldPath) || !l_isInsideRoot(l_oldPath)) continue;

        bool l_ours = false;
        {
            std::ifstream l_in(l_oldPath);
            std::string l_firstLine;
            std::getline(l_in, l_firstLine);
            l_ours = l_firstLine.rfind(l_headerPrefix, 0) == 0 || l_oldPath.extension() == ".json";
        }

        if (l_clean && l_ours)
        {
            fs::remove(l_oldPath, l_ec);
            std::cout << "  Removed stale: " << l_oldPath.string() << std::endl;
            for (fs::path l_dir = l_oldPath.parent_path(); l_dir != fs::path(l_outputDir) && fs::is_empty(l_dir, l_ec); l_dir = l_dir.parent_path())
            {
                fs::remove(l_dir, l_ec);
            }
        }
        else
        {
            std::cerr << "warning: " << l_oldPath.string() << " is no longer generated by this project" << (l_ours ? " (run with --clean to remove it)" : " (not touched: it was edited)") << std::endl;
            l_stillOwned.insert(l_old); // keep tracking it until it is cleaned up
        }
    }
    if (l_stillOwned != l_previous)
    {
        fs::create_directories(l_manifestPath.parent_path(), l_ec);
        std::ofstream l_manifest(l_manifestPath);
        for (const std::string& l_path : l_stillOwned) l_manifest << l_path << "\n";
    }

    int l_stubsCreated = 0;
    for (const pyke::Generator::SourceRef& l_ref : l_generator.sourceRefs())
    {
        fs::path l_filePath = fs::path(l_outputDir) / l_ref.targetDir / l_ref.file;

        if (fs::exists(l_filePath)) continue;
        if (!l_isInsideRoot(l_filePath))
        {
            std::cerr << "warning: skipping stub outside the output directory: " << l_filePath.string() << std::endl;
            continue;
        }

        fs::path l_parent = l_filePath.parent_path();
        if (!l_parent.empty())
        {
            fs::create_directories(l_parent);
        }

        std::ofstream l_stub(l_filePath);
        if (!l_stub.is_open()) continue;

        std::string l_ext = l_filePath.extension().string();
        if (l_ext == ".h" || l_ext == ".hpp" || l_ext == ".hxx")
        {
            l_stub << "#pragma once\n";
        }
        else if (l_ext == ".cppm" || l_ext == ".ixx" || l_ext == ".mpp" || l_ext == ".cxxm" || l_ext == ".c++m" || l_ext == ".ccm")
        {
            std::string l_module = l_filePath.stem().string();
            for (char& l_c : l_module) if (!std::isalnum(static_cast<unsigned char>(l_c))) l_c = '_';
            if (l_module.empty() || std::isdigit(static_cast<unsigned char>(l_module[0]))) l_module.insert(l_module.begin(), '_');
            l_stub << "export module " << l_module << ";\n";
        }
        else if (l_ext == ".cpp" || l_ext == ".cc" || l_ext == ".cxx")
        {
            l_stub << "// " << l_ref.file << "\n";
            if (l_filePath.stem() == "main")
            {
                l_stub << "\nint main()\n{\n    return 0;\n}\n";
            }
        }
        else if (l_ext == ".c")
        {
            l_stub << "// " << l_ref.file << "\n";
        }

        std::cout << "  Created stub: " << l_filePath.string() << std::endl;
        l_stubsCreated++;
    }

    if (l_stubsCreated > 0)
    {
        std::cout << "Created " << l_stubsCreated << " stub file(s)." << std::endl;
    }

    checkSources(l_generator, fs::path(l_outputDir));

    for (const pyke::FetchDecl& l_fetch : l_program.fetches)
    {
        if (l_fetch.local && !fs::exists(fs::path(l_outputDir) / l_fetch.repo / "CMakeLists.txt"))
        {
            std::cerr << "warning: vendor import '" << l_fetch.name << "': " << (fs::path(l_outputDir) / l_fetch.repo / "CMakeLists.txt").string() << " does not exist (the folder is relative to the output directory)" << std::endl;
        }
    }

    if (l_buildMode)
    {
        for (const pyke::TargetDecl& l_t : l_program.targets) l_build.usesModules = l_build.usesModules || pyke::targetUsesModules(l_t);
        l_build.importStd = l_program.project && l_program.project->importStd;
        return runBuild(l_build, fs::path(l_outputDir));
    }
    return 0;
}
