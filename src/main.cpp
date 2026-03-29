#include "lexer.hpp"
#include "parser.hpp"
#include "analyzer.hpp"
#include "generator.hpp"
#include "error.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

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

    for (const fs::directory_entry& l_entry : fs::recursive_directory_iterator(l_root))
    {
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

    std::string l_projectName = l_root.filename().string();
    if (l_projectName.empty() || l_projectName == ".")
    {
        l_projectName = fs::current_path().filename().string();
    }

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
    l_out << "target " << l_projectName << "():\n";
    l_out << "    def configure(self):\n";

    if (l_useGlob)
    {
        std::set<std::string> l_globDirs;
        for (const std::string& l_s : l_sources)
        {
            std::string l_parent = fs::path(l_s).parent_path().generic_string();
            if (l_parent.empty()) l_parent = ".";
            l_globDirs.insert(l_parent);
        }
        l_out << "        self.sources = [";
        bool l_first = true;
        for (const std::string& l_d : l_globDirs)
        {
            if (!l_first) l_out << ", ";
            if (l_d == ".") l_out << "\"*.cpp\"";
            else l_out << "\"" << l_d << "/*.cpp\"";
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

    std::string l_outputFile = (l_root / (l_projectName + ".pyke")).string();
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

int runFmt(const std::string& p_path)
{
    std::ifstream l_in(p_path);
    if (!l_in.is_open())
    {
        std::cerr << "Error: Could not open file: " << p_path << std::endl;
        return 1;
    }

    std::vector<std::string> l_lines;
    std::string l_line;
    while (std::getline(l_in, l_line))
    {
        while (!l_line.empty() && (l_line.back() == ' ' || l_line.back() == '\t' || l_line.back() == '\r'))
        {
            l_line.pop_back();
        }
        l_lines.push_back(l_line);
    }
    l_in.close();

    while (!l_lines.empty() && l_lines.back().empty())
    {
        l_lines.pop_back();
    }

    std::vector<std::string> l_formatted;
    int l_consecutiveBlank = 0;
    for (const std::string& l_l : l_lines)
    {
        if (l_l.empty())
        {
            l_consecutiveBlank++;
            if (l_consecutiveBlank <= 1)
            {
                l_formatted.push_back(l_l);
            }
        }
        else
        {
            l_consecutiveBlank = 0;
            l_formatted.push_back(l_l);
        }
    }

    std::ofstream l_out(p_path);
    if (!l_out.is_open())
    {
        std::cerr << "Error: Could not write: " << p_path << std::endl;
        return 1;
    }
    for (const std::string& l_l : l_formatted)
    {
        l_out << l_l << "\n";
    }
    std::cout << "Formatted: " << p_path << std::endl;
    return 0;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: pyke <input.pyke> [output_dir]" << std::endl;
        std::cerr << "       pyke --init [directory]" << std::endl;
        std::cerr << "       pyke --validate <input.pyke>" << std::endl;
        std::cerr << "       pyke --fmt <input.pyke>" << std::endl;
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
        pyke::Parser l_uparser(l_ulexer.tokenize());
        pyke::Program l_uprogram = l_uparser.parse();

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
        if (argc < 3)
        {
            std::cerr << "Usage: pyke --fmt <input.pyke>" << std::endl;
            return 1;
        }
        return runFmt(argv[2]);
    }

    bool l_validateOnly = false;
    if (l_firstArg == "--validate")
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

    if (l_parser.hasErrors())
    {
        for (const std::string& l_err : l_parser.errors())
        {
            std::cerr << l_inputPath << ": " << l_err << std::endl;
        }
        return 1;
    }

    pyke::Analyzer l_analyzer(l_program);
    if (!l_analyzer.analyze())
    {
        for (const std::string& l_err : l_analyzer.errors())
        {
            std::cerr << l_inputPath << ": " << l_err << std::endl;
        }
        return 1;
    }

    if (l_validateOnly)
    {
        std::cout << l_inputPath << ": OK" << std::endl;
        return 0;
    }

    pyke::Generator l_generator(l_program);
    std::vector<pyke::GeneratedFile> l_files = l_generator.generate();

    for (const pyke::GeneratedFile& l_genFile : l_files)
    {
        fs::path l_outPath = fs::path(l_outputDir) / l_genFile.path;

        fs::path l_parent = l_outPath.parent_path();
        if (!l_parent.empty())
        {
            fs::create_directories(l_parent);
        }

        std::ofstream l_out(l_outPath);
        if (!l_out.is_open())
        {
            std::cerr << "Error: Could not write file: " << l_outPath.string() << std::endl;
            return 1;
        }

        l_out << l_genFile.content;
        std::cout << "  Generated: " << l_outPath.string() << std::endl;
    }

    std::cout << "Done. Generated " << l_files.size() << " file(s)." << std::endl;

    int l_stubsCreated = 0;
    for (const pyke::Generator::SourceRef& l_ref : l_generator.sourceRefs())
    {
        fs::path l_filePath = fs::path(l_outputDir) / l_ref.targetDir / l_ref.file;

        if (fs::exists(l_filePath)) continue;

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
        else if (l_ext == ".cpp" || l_ext == ".cc" || l_ext == ".cxx")
        {
            fs::path l_header = l_filePath;
            l_header.replace_extension(".h");
            if (fs::exists(l_header) || l_ref.file.find("main") != std::string::npos)
            {
            }
            l_stub << "// " << l_ref.file << "\n";
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

    return 0;
}
