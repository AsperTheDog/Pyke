#include "generator.hpp"
#include <algorithm>
#include <filesystem>
#include <sstream>

namespace pyke
{

static std::string escapeCmake(const std::string& p_value);


Generator::Generator(const Program& p_program)
    : m_program(p_program)
{
    for (const TargetDecl& l_target : m_program.targets)
    {
        m_targetNames.insert(l_target.name);
    }
    for (const ImportDecl& l_import : m_program.imports)
    {
        for (size_t l_i = 0; l_i < l_import.packages.size(); ++l_i)
        {
            m_importedPackages.insert(l_import.packages[l_i]);
            if (l_i < l_import.specs.size() && l_import.specs[l_i].optional) m_optionalPackages.insert(l_import.packages[l_i]);
        }
    }
    for (const FetchDecl& l_fetch : m_program.fetches)
    {
        m_targetNames.insert(l_fetch.name);
        m_fetchedNames.insert(l_fetch.name);
    }
    if (m_program.project)
    {
        bool l_cxx = false;
        for (const std::string& l_lang : m_program.project->langs)
        {
            if (l_lang.substr(0, 3) == "c++") l_cxx = true;
            else m_hasC = true;
        }
        m_compilerIdVar = (m_hasC && !l_cxx) ? "CMAKE_C_COMPILER_ID" : "CMAKE_CXX_COMPILER_ID";
    }
    collectPackageComponents();
}

std::vector<GeneratedFile> Generator::generate()
{
    std::vector<GeneratedFile> l_files;

    l_files.push_back({"CMakeLists.txt", generateRoot()});

    for (const TargetDecl& l_target : m_program.targets)
    {
        std::string l_dir = targetDir(l_target);
        if (l_dir == ".") continue; // lives in the root CMakeLists.txt
        l_files.push_back({l_dir + "/CMakeLists.txt", generateTarget(l_target)});
    }

    if (m_program.project.has_value() && m_program.project->presets)
    {
        l_files.push_back({"CMakePresets.json", generatePresets()});
    }

    return l_files;
}

std::string Generator::generateRoot()
{
    std::ostringstream l_out;

    l_out << "cmake_minimum_required(VERSION 3.21)\n";

    if (m_program.project.has_value())
    {
        l_out << "project(" << m_program.project->name;
        if (!m_program.project->version.empty())
        {
            l_out << " VERSION " << m_program.project->version;
        }
        bool l_hasCxx = !m_hasC; // C++ is the default language when none is given
        for (const std::string& l_lang : m_program.project->langs)
        {
            if (l_lang.substr(0, 3) == "c++") l_hasCxx = true;
        }
        l_out << " LANGUAGES" << (m_hasC ? " C" : "") << (l_hasCxx ? " CXX" : "") << ")\n";

        for (const std::string& l_lang : m_program.project->langs)
        {
            bool l_isCxx = l_lang.substr(0, 3) == "c++";
            std::string l_stdVer = l_isCxx ? l_lang.substr(3) : l_lang.substr(1);
            std::string l_var = l_isCxx ? "CMAKE_CXX_STANDARD" : "CMAKE_C_STANDARD";
            l_out << "\nset(" << l_var << " " << l_stdVer << ")\n";
            l_out << "set(" << l_var << "_REQUIRED ON)\n";
        }

        // Single-config generators otherwise build with no optimisation flags at all
        l_out << "\nif(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)\n";
        l_out << "    set(CMAKE_BUILD_TYPE Release CACHE STRING \"Build type\" FORCE)\n";
        l_out << "endif()\n";
        l_out << "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n";

        if (!m_program.project->outputDir.empty())
        {
            l_out << "\nset(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/" << m_program.project->outputDir << ")\n";
            l_out << "set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/" << m_program.project->outputDir << ")\n";
        }
    }

    if (!m_program.options.empty())
    {
        l_out << "\n";
        for (const OptionDecl& l_opt : m_program.options)
        {
            if (l_opt.type == "bool")
            {
                std::string l_defaultVal = "OFF";
                if (l_opt.defaultValue)
                {
                    auto* l_b = std::get_if<BoolLiteral>(&l_opt.defaultValue->value);
                    if (l_b && l_b->value) l_defaultVal = "ON";
                }
                l_out << "option(" << l_opt.name << " \"" << l_opt.name << "\" " << l_defaultVal << ")\n";
            }
            else
            {
                std::string l_defaultVal;
                if (l_opt.defaultValue)
                {
                    auto* l_s = std::get_if<StringLiteral>(&l_opt.defaultValue->value);
                    if (l_s) l_defaultVal = l_s->value;
                }
                std::string l_cacheType = (l_opt.type == "path") ? "PATH" : "STRING";
                l_out << "set(" << l_opt.name << " \"" << l_defaultVal << "\" CACHE " << l_cacheType << " \"" << l_opt.name << "\")\n";
            }
        }
    }

    if (!m_program.imports.empty())
    {
        l_out << "\n";
        for (const ImportDecl& l_import : m_program.imports)
        {
            bool l_conditional = !l_import.condition.empty();
            if (l_conditional)
            {
                l_out << "if(" << l_import.condition << ")\n    ";
            }
            for (size_t l_pi = 0; l_pi < l_import.packages.size(); ++l_pi)
            {
                const std::string& l_pkg = l_import.packages[l_pi];
                PackageSpec l_spec = l_pi < l_import.specs.size() ? l_import.specs[l_pi] : PackageSpec{};
                if (l_conditional && l_pi > 0) l_out << "    ";
                l_out << "find_package(" << l_pkg;
                if (!l_spec.version.empty()) l_out << " " << l_spec.version;
                if (!l_spec.optional) l_out << " REQUIRED";
                if (l_spec.config) l_out << " CONFIG";
                std::map<std::string, std::set<std::string>>::const_iterator l_it = m_packageComponents.find(l_pkg);
                if (l_it != m_packageComponents.end() && !l_it->second.empty())
                {
                    l_out << " COMPONENTS";
                    for (const std::string& l_comp : l_it->second)
                    {
                        l_out << " " << l_comp;
                    }
                }
                l_out << ")\n";
            }
            if (l_conditional)
            {
                l_out << "endif()\n";
            }
        }
    }

    if (!m_program.fetches.empty())
    {
        l_out << "\ninclude(FetchContent)\n";
        for (const FetchDecl& l_fetch : m_program.fetches)
        {
            std::string l_ind = l_fetch.condition.empty() ? "" : "    ";
            if (!l_fetch.condition.empty()) l_out << "if(" << l_fetch.condition << ")\n";
            for (const auto& l_opt : l_fetch.options)
            {
                std::string l_value, l_type = "STRING";
                if (auto* l_b = std::get_if<BoolLiteral>(&l_opt.second->value)) { l_value = l_b->value ? "ON" : "OFF"; l_type = "BOOL"; }
                else if (auto* l_s = std::get_if<StringLiteral>(&l_opt.second->value)) l_value = escapeCmake(l_s->value);
                else if (auto* l_i = std::get_if<IntLiteral>(&l_opt.second->value)) l_value = std::to_string(l_i->value);
                l_out << l_ind << "set(" << l_opt.first << " \"" << l_value << "\" CACHE " << l_type << " \"\" FORCE)\n";
            }
            if (l_fetch.local)
            {
                if (!l_fetch.condition.empty()) l_out << "endif()\n"; // vendored folders are added in the second pass, with the other imports
                continue;
            }
            l_out << l_ind << "FetchContent_Declare(" << l_fetch.name << "\n";
            l_out << l_ind << "    GIT_REPOSITORY https://github.com/" << l_fetch.repo << ".git\n";
            if (!l_fetch.tag.empty())
            {
                l_out << l_ind << "    GIT_TAG " << l_fetch.tag << "\n";
                bool l_isCommit = l_fetch.tag.size() >= 7 && l_fetch.tag.find_first_not_of("0123456789abcdef") == std::string::npos;
                if (!l_isCommit) l_out << l_ind << "    GIT_SHALLOW TRUE\n"; // a tag or branch can be cloned shallowly; a commit hash can't
            }
            l_out << l_ind << ")\n";
            if (!l_fetch.condition.empty()) l_out << "endif()\n";
        }
        l_out << "\n";
        for (const FetchDecl& l_fetch : m_program.fetches)
        {
            std::string l_add = l_fetch.local ? "add_subdirectory(" + l_fetch.repo + ")" : "FetchContent_MakeAvailable(" + l_fetch.name + ")";
            if (l_fetch.condition.empty()) l_out << l_add << "\n";
            else l_out << "if(" << l_fetch.condition << ")\n    " << l_add << "\nendif()\n";
        }
    }

    bool l_hasTests = false;
    for (const TargetDecl& l_target : m_program.targets)
    {
        if (l_target.test) { l_hasTests = true; break; }
    }
    if (l_hasTests)
    {
        l_out << "\nenable_testing()\n";
    }

    if (!m_program.rawCmake.empty())
    {
        l_out << "\n";
        for (const RawCmake& l_raw : m_program.rawCmake) l_out << rawCmakeBlock(l_raw.text, "");
    }

    if (!m_program.targets.empty())
    {
        l_out << "\n";
        for (const TargetDecl& l_target : m_program.targets)
        {
            if (targetDir(l_target) != ".") l_out << "add_subdirectory(" << targetDir(l_target) << ")\n";
        }
    }

    emitPackageExports(l_out);

    // Targets declared with path "." sit beside the project() call, after every subdirectory is added
    for (const TargetDecl& l_target : m_program.targets)
    {
        if (targetDir(l_target) != ".") continue;
        l_out << "\n# " << l_target.name << "\n" << generateTarget(l_target);
    }

    return l_out.str();
}

// One find_package()-able package per distinct `self.export` name
void Generator::emitPackageExports(std::ostringstream& p_out)
{
    std::map<std::string, std::set<std::string>> l_packages; // export name -> imported packages its targets need
    for (const TargetDecl& l_target : m_program.targets)
    {
        std::string l_name = exportName(l_target);
        if (l_name.empty() || l_target.type == TargetType::EXECUTABLE) continue;
        std::set<std::string>& l_deps = l_packages[l_name];
        for (const Dependency& l_dep : l_target.dependencies)
        {
            if (l_target.type == TargetType::SHARED_LIBRARY && l_dep.visibility == "PRIVATE") continue;
            size_t l_dot = l_dep.name.find('.');
            std::string l_base = l_dot == std::string::npos ? l_dep.name : l_dep.name.substr(0, l_dot);
            if (m_importedPackages.count(l_base)) l_deps.insert(l_base);
        }
    }
    if (l_packages.empty()) return;

    p_out << "\ninclude(CMakePackageConfigHelpers)\n";
    for (const auto& l_pkg : l_packages)
    {
        const std::string& l_name = l_pkg.first;
        std::string l_dest = "lib/cmake/" + l_name;
        p_out << "\n# find_package(" << l_name << ") support\n";
        p_out << "install(EXPORT " << l_name << "Targets NAMESPACE " << l_name << ":: DESTINATION " << l_dest << ")\n";

        std::string l_config = "include(CMakeFindDependencyMacro)\\n";
        for (const std::string& l_dep : l_pkg.second)
        {
            l_config += "find_dependency(" + l_dep;
            auto l_comps = m_packageComponents.find(l_dep);
            if (l_comps != m_packageComponents.end() && !l_comps->second.empty())
            {
                l_config += " COMPONENTS";
                for (const std::string& l_c : l_comps->second) l_config += " " + l_c;
            }
            l_config += ")\\n";
        }
        l_config += "include(\\\"\\${CMAKE_CURRENT_LIST_DIR}/" + l_name + "Targets.cmake\\\")\\n";
        p_out << "file(WRITE \"${CMAKE_CURRENT_BINARY_DIR}/" << l_name << "Config.cmake\" \"" << l_config << "\")\n";
        p_out << "install(FILES \"${CMAKE_CURRENT_BINARY_DIR}/" << l_name << "Config.cmake\"";
        if (m_program.project && !m_program.project->version.empty())
        {
            p_out << "\n    \"${CMAKE_CURRENT_BINARY_DIR}/" << l_name << "ConfigVersion.cmake\"";
        }
        p_out << " DESTINATION " << l_dest << ")\n";
        if (m_program.project && !m_program.project->version.empty())
        {
            p_out << "write_basic_package_version_file(\"${CMAKE_CURRENT_BINARY_DIR}/" << l_name << "ConfigVersion.cmake\"\n"
                  << "    VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)\n";
        }
    }
}

std::string Generator::generatePresets()
{
    std::string l_name = "default";
    if (m_program.project.has_value())
    {
        l_name = m_program.project->name;
    }

    std::ostringstream l_out;
    l_out << "{\n";
    l_out << "    \"version\": 6,\n";
    l_out << "    \"cmakeMinimumRequired\": { \"major\": 3, \"minor\": 21, \"patch\": 0 },\n";
    l_out << "    \"configurePresets\": [\n";
    l_out << "        {\n";
    l_out << "            \"name\": \"default\",\n";
    l_out << "            \"displayName\": \"Default\",\n";
    l_out << "            \"binaryDir\": \"${sourceDir}/build\",\n";
    l_out << "            \"cacheVariables\": {\n";
    l_out << "                \"CMAKE_BUILD_TYPE\": \"Release\"\n";
    l_out << "            }\n";
    l_out << "        },\n";
    l_out << "        {\n";
    l_out << "            \"name\": \"debug\",\n";
    l_out << "            \"displayName\": \"Debug\",\n";
    l_out << "            \"binaryDir\": \"${sourceDir}/build-debug\",\n";
    l_out << "            \"cacheVariables\": {\n";
    l_out << "                \"CMAKE_BUILD_TYPE\": \"Debug\"\n";
    l_out << "            }\n";
    l_out << "        },\n";
    l_out << "        {\n";
    l_out << "            \"name\": \"release\",\n";
    l_out << "            \"displayName\": \"Release\",\n";
    l_out << "            \"binaryDir\": \"${sourceDir}/build-release\",\n";
    l_out << "            \"cacheVariables\": {\n";
    l_out << "                \"CMAKE_BUILD_TYPE\": \"Release\"\n";
    l_out << "            }\n";
    l_out << "        }\n";
    l_out << "    ],\n";
    l_out << "    \"buildPresets\": [\n";
    l_out << "        { \"name\": \"default\", \"configurePreset\": \"default\" },\n";
    l_out << "        { \"name\": \"debug\", \"configurePreset\": \"debug\" },\n";
    l_out << "        { \"name\": \"release\", \"configurePreset\": \"release\" }\n";
    l_out << "    ]\n";
    l_out << "}\n";
    return l_out.str();
}

std::string Generator::generateTarget(const TargetDecl& p_target)
{
    m_globCounter = 0;
    m_currentTargetDir = targetDir(p_target);
    m_currentExport = p_target.type == TargetType::EXECUTABLE ? "" : exportName(p_target);
    std::string l_result;

    std::string l_typeCmd = cmakeTargetType(p_target);
    l_result += l_typeCmd + "(" + p_target.name;
    if (p_target.type == TargetType::SHARED_LIBRARY) l_result += " SHARED";
    else if (p_target.type == TargetType::STATIC_LIBRARY) l_result += " STATIC";
    else if (p_target.type == TargetType::HEADER_ONLY) l_result += " INTERFACE";
    l_result += ")\n";

    if (p_target.unityBuild && p_target.type != TargetType::HEADER_ONLY)
    {
        l_result += "set_target_properties(" + p_target.name + " PROPERTIES UNITY_BUILD ON)\n";
    }

    for (const Dependency& l_dep : p_target.dependencies)
    {
        std::string l_cmakeDep = depToCmake(l_dep.name);
        l_result += "target_link_libraries(" + p_target.name + " " + l_dep.visibility + " " + l_cmakeDep + ")\n";
    }

    for (const Method& l_method : p_target.methods)
    {
        if (l_method.name == "configure" || l_method.name == "install")
        {
            l_result += "\n";
            generateMethodBody(l_method, p_target, l_result, 0);
        }
    }

    if (p_target.sourceGroups && p_target.type != TargetType::HEADER_ONLY)
    {
        // source_group(TREE) is a hard error for files outside the root, so group those separately
        const std::string& l_n = p_target.name;
        l_result += "\nget_target_property(" + l_n + "_ALL_SOURCES " + l_n + " SOURCES)\n";
        l_result += "if(" + l_n + "_ALL_SOURCES)\n";
        l_result += "    foreach(_src IN LISTS " + l_n + "_ALL_SOURCES)\n";
        l_result += "        cmake_path(ABSOLUTE_PATH _src BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} NORMALIZE OUTPUT_VARIABLE _abs)\n";
        l_result += "        cmake_path(IS_PREFIX CMAKE_CURRENT_SOURCE_DIR \"${_abs}\" NORMALIZE _inside)\n";
        l_result += "        if(_inside)\n";
        l_result += "            source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES \"${_abs}\")\n";
        l_result += "        else()\n";
        l_result += "            source_group(\"External Sources\" FILES \"${_abs}\")\n";
        l_result += "        endif()\n";
        l_result += "    endforeach()\n";
        l_result += "endif()\n";
    }

    if (!m_currentExport.empty())
    {
        auto l_dest = [&](const char* p_attr, const char* p_default)
        {
            const Expression* l_value = installValue(p_target, p_attr);
            return l_value ? exprToCmake(*l_value) : std::string(p_default);
        };
        l_result += "\ninstall(TARGETS " + p_target.name + " EXPORT " + m_currentExport + "Targets\n";
        if (p_target.type != TargetType::HEADER_ONLY)
        {
            l_result += "    RUNTIME DESTINATION " + l_dest("runtime", "bin") + "\n";
            l_result += "    LIBRARY DESTINATION " + l_dest("library", "lib") + "\n";
            l_result += "    ARCHIVE DESTINATION " + l_dest("library", "lib") + "\n";
        }
        l_result += "    INCLUDES DESTINATION include\n)\n";
    }

    if (p_target.test && p_target.type == TargetType::EXECUTABLE)
    {
        l_result += "\nadd_test(NAME " + p_target.name + " COMMAND " + p_target.name + ")\n";
    }

    if (p_target.copyDlls && p_target.type == TargetType::EXECUTABLE)
    {
        // TARGET_RUNTIME_DLLS is empty off Windows, and also for an executable with no DLL dependencies; copy_if_different
        // with no files is a usage error, so the command degrades to `cmake -E true` in that case
        const std::string l_dlls = "$<TARGET_RUNTIME_DLLS:" + p_target.name + ">";
        l_result += "\nif(WIN32)\n";
        l_result += "    add_custom_command(TARGET " + p_target.name + " POST_BUILD\n";
        l_result += "        COMMAND ${CMAKE_COMMAND} -E $<IF:$<BOOL:" + l_dlls + ">,copy_if_different,true>\n";
        l_result += "            $<TARGET_RUNTIME_DLLS:" + p_target.name + ">\n";
        l_result += "            $<TARGET_FILE_DIR:" + p_target.name + ">\n";
        l_result += "        COMMAND_EXPAND_LISTS\n";
        l_result += "    )\n";
        l_result += "endif()\n";
    }

    return l_result;
}

void Generator::generateMethodBody(const Method& p_method, const TargetDecl& p_target, std::string& p_out, int p_indentLevel)
{
    for (const StmtPtr& l_stmt : p_method.body)
    {
        generateStatement(*l_stmt, p_target, p_out, p_indentLevel);
    }
}

void Generator::generateStatement(const Statement& p_stmt, const TargetDecl& p_target, std::string& p_out, int p_indentLevel)
{
    if (auto* l_assign = std::get_if<AssignStatement>(&p_stmt.value))
    {
        generateAssignment(*l_assign, p_target, p_out, p_indentLevel);
    }
    else if (auto* l_aug = std::get_if<AugAssignStatement>(&p_stmt.value))
    {
        generateAugAssignment(*l_aug, p_target, p_out, p_indentLevel);
    }
    else if (auto* l_ifStmt = std::get_if<IfStatement>(&p_stmt.value))
    {
        generateIfStatement(*l_ifStmt, p_target, p_out, p_indentLevel);
    }
}

void Generator::generateIfStatement(const IfStatement& p_ifStmt, const TargetDecl& p_target, std::string& p_out, int p_indentLevel)
{
    // Use generator expressions for build_type conditions on definitions/flags to support multi-config generators
    if (p_ifStmt.branches.size() == 1 && p_ifStmt.branches[0].condition)
    {
        auto* l_cmp = std::get_if<Comparison>(&p_ifStmt.branches[0].condition->value);
        if (l_cmp)
        {
            auto* l_leftId = std::get_if<Identifier>(&l_cmp->left->value);
            auto* l_rightStr = std::get_if<StringLiteral>(&l_cmp->right->value);
            if (l_leftId && l_leftId->name == "build_type" && l_rightStr && l_cmp->op == "==")
            {
                std::string l_bt = cmakeConfigName(l_rightStr->value);
                std::string l_genexConfig = "$<CONFIG:" + l_bt + ">";

                std::string l_ind = indent(p_indentLevel);
                for (const StmtPtr& l_stmt : p_ifStmt.branches[0].body)
                {
                    if (auto* l_assign = std::get_if<AssignStatement>(&l_stmt->value))
                    {
                        std::string l_vis;
                        std::string l_attr = resolveAttribute(*l_assign->target, p_target, l_vis);

                        if (l_attr == "definitions")
                        {
                            if (auto* l_dict = std::get_if<DictLiteral>(&l_assign->value->value))
                            {
                                for (const std::pair<ExprPtr, ExprPtr>& l_entry : l_dict->entries)
                                {
                                    std::string l_key = exprToCmake(*l_entry.first);
                                    if (l_key.front() == '"') l_key = l_key.substr(1, l_key.size() - 2);
                                    std::string l_val = exprToCmake(*l_entry.second);
                                    p_out += l_ind + "target_compile_definitions(" + p_target.name + " " + l_vis + " $<" + l_genexConfig + ":" + l_key + "=" + l_val + ">)\n";
                                }
                                continue;
                            }
                        }

                        if (auto* l_idx = std::get_if<IndexAccess>(&l_assign->target->value))
                        {
                            std::string l_baseVis;
                            std::string l_baseAttr = resolveAttribute(*l_idx->object, p_target, l_baseVis);
                            if (l_baseAttr == "definitions")
                            {
                                std::string l_key = exprToCmake(*l_idx->index);
                                if (l_key.front() == '"') l_key = l_key.substr(1, l_key.size() - 2);
                                std::string l_val = exprToCmake(*l_assign->value);
                                p_out += l_ind + "target_compile_definitions(" + p_target.name + " " + l_baseVis + " $<" + l_genexConfig + ":" + l_key + "=" + l_val + ">)\n";
                                continue;
                            }
                        }

                        if (l_attr == "flags")
                        {
                            if (auto* l_list = std::get_if<ListLiteral>(&l_assign->value->value))
                            {
                                p_out += l_ind + "target_compile_options(" + p_target.name + " " + l_vis;
                                for (const ExprPtr& l_elem : l_list->elements)
                                {
                                    p_out += " $<" + l_genexConfig + ":" + exprToCmake(*l_elem) + ">";
                                }
                                p_out += ")\n";
                                continue;
                            }
                        }
                    }
                    if (auto* l_aug = std::get_if<AugAssignStatement>(&l_stmt->value))
                    {
                        std::string l_vis;
                        std::string l_attr = resolveAttribute(*l_aug->target, p_target, l_vis);

                        if (l_attr == "definitions")
                        {
                            if (auto* l_idx = std::get_if<IndexAccess>(&l_aug->target->value))
                            {
                                std::string l_baseVis;
                                std::string l_baseAttr = resolveAttribute(*l_idx->object, p_target, l_baseVis);
                                if (l_baseAttr == "definitions")
                                {
                                    std::string l_key = exprToCmake(*l_idx->index);
                                    if (l_key.front() == '"') l_key = l_key.substr(1, l_key.size() - 2);
                                    std::string l_val = exprToCmake(*l_aug->value);
                                    p_out += l_ind + "target_compile_definitions(" + p_target.name + " " + l_baseVis + " $<" + l_genexConfig + ":" + l_key + "=" + l_val + ">)\n";
                                    continue;
                                }
                            }
                        }

                        if (l_attr == "flags")
                        {
                            if (auto* l_list = std::get_if<ListLiteral>(&l_aug->value->value))
                            {
                                p_out += l_ind + "target_compile_options(" + p_target.name + " " + l_vis;
                                for (const ExprPtr& l_elem : l_list->elements)
                                {
                                    p_out += " $<" + l_genexConfig + ":" + exprToCmake(*l_elem) + ">";
                                }
                                p_out += ")\n";
                                continue;
                            }
                        }
                    }

                    p_out += l_ind + "if(CMAKE_BUILD_TYPE STREQUAL \"" + l_bt + "\")\n";
                    generateStatement(*l_stmt, p_target, p_out, p_indentLevel + 1);
                    p_out += l_ind + "endif()\n";
                }
                return;
            }
        }
    }

    for (size_t l_i = 0; l_i < p_ifStmt.branches.size(); l_i++)
    {
        const IfBranch& l_branch = p_ifStmt.branches[l_i];

        if (l_i == 0)
        {
            p_out += indent(p_indentLevel) + "if(" + conditionToCmake(*l_branch.condition) + ")\n";
        }
        else if (l_branch.condition)
        {
            p_out += indent(p_indentLevel) + "elseif(" + conditionToCmake(*l_branch.condition) + ")\n";
        }
        else
        {
            p_out += indent(p_indentLevel) + "else()\n";
        }

        for (const StmtPtr& l_stmt : l_branch.body)
        {
            generateStatement(*l_stmt, p_target, p_out, p_indentLevel + 1);
        }
    }
    p_out += indent(p_indentLevel) + "endif()\n";
}

void Generator::emitListCommand(const std::string& p_cmakeCmd, const TargetDecl& p_target, const std::string& p_visibility, const Expression& p_value, std::string& p_out, int p_indentLevel)
{
    std::string l_ind = indent(p_indentLevel);
    p_out += l_ind + p_cmakeCmd + "(" + p_target.name + " " + p_visibility + "\n";
    if (auto* l_list = std::get_if<ListLiteral>(&p_value.value))
    {
        // An exported target's include paths must differ between the build tree and the installed tree
        bool l_wrap = p_cmakeCmd == "target_include_directories" && p_visibility != "PRIVATE" && !m_currentExport.empty();
        for (const ExprPtr& l_elem : l_list->elements)
        {
            auto* l_str = std::get_if<StringLiteral>(&l_elem->value);
            if (l_wrap && l_str && l_str->value.size() > 2 && l_str->value.compare(0, 2, "//") == 0 && l_str->value[2] != '/')
            {
                p_out += l_ind + "    \"$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/" + l_str->value.substr(2) + ">\"\n";
            }
            else if (l_wrap && l_str && !l_str->value.empty() && l_str->value[0] != '/' && l_str->value[0] != '$')
            {
                p_out += l_ind + "    \"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/" + l_str->value + ">\"\n";
            }
            else
            {
                p_out += l_ind + "    " + exprToCmake(*l_elem) + "\n";
            }
        }
    }
    p_out += l_ind + ")\n";
}

void Generator::generateAssignment(const AssignStatement& p_assign, const TargetDecl& p_target, std::string& p_out, int p_indentLevel)
{
    emitAssignment(*p_assign.target, *p_assign.value, p_target, p_out, p_indentLevel);
}

// warnings / warnings_as_errors / sanitize / lto: one setting, translated per compiler family
void Generator::emitQualityAttribute(const std::string& p_attr, const Expression& p_rhs, const TargetDecl& p_target, std::string& p_out, int p_indentLevel)
{
    std::string l_ind = indent(p_indentLevel);
    const std::string& l_name = p_target.name;
    std::string l_idKind = m_compilerIdVar == "CMAKE_C_COMPILER_ID" ? "C_COMPILER_ID" : "CXX_COMPILER_ID";
    std::string l_msvc = "$<" + l_idKind + ":MSVC>";
    std::string l_gnuLike = "$<" + l_idKind + ":GNU,Clang,AppleClang>";

    auto l_options = [&](const std::string& p_command, const std::string& p_msvcFlags, const std::string& p_gnuFlags)
    {
        if (!p_msvcFlags.empty()) p_out += l_ind + p_command + "(" + l_name + " PRIVATE \"$<$<BOOL:" + l_msvc + ">:" + p_msvcFlags + ">\")\n";
        if (!p_gnuFlags.empty()) p_out += l_ind + p_command + "(" + l_name + " PRIVATE \"$<$<BOOL:" + l_gnuLike + ">:" + p_gnuFlags + ">\")\n";
    };

    if (p_attr == "warnings")
    {
        std::string l_level = std::get<StringLiteral>(p_rhs.value).value;
        if (l_level == "none") l_options("target_compile_options", "/W0", "-w");
        else if (l_level == "all") l_options("target_compile_options", "/W3", "-Wall");
        else if (l_level == "strict") l_options("target_compile_options", "/W4", "-Wall;-Wextra;-Wpedantic");
        return;
    }

    if (p_attr == "warnings_as_errors")
    {
        if (std::get<BoolLiteral>(p_rhs.value).value) l_options("target_compile_options", "/WX", "-Werror");
        return;
    }

    if (p_attr == "sanitize")
    {
        std::string l_gnuList, l_msvcFlag;
        if (auto* l_list = std::get_if<ListLiteral>(&p_rhs.value))
        {
            for (const ExprPtr& l_elem : l_list->elements)
            {
                auto* l_str = std::get_if<StringLiteral>(&l_elem->value);
                if (!l_str) continue;
                l_gnuList += (l_gnuList.empty() ? "" : ",") + l_str->value;
                if (l_str->value == "address") l_msvcFlag = "/fsanitize=address";
            }
        }
        if (l_gnuList.empty()) return;
        l_options("target_compile_options", l_msvcFlag, "-fsanitize=" + l_gnuList + ";-fno-omit-frame-pointer");
        l_options("target_link_options", "", "-fsanitize=" + l_gnuList);
        return;
    }

    if (p_attr == "lto" && std::get<BoolLiteral>(p_rhs.value).value)
    {
        p_out += l_ind + "include(CheckIPOSupported)\n";
        p_out += l_ind + "check_ipo_supported(RESULT " + l_name + "_ipo_supported OUTPUT " + l_name + "_ipo_message)\n";
        p_out += l_ind + "if(" + l_name + "_ipo_supported)\n";
        p_out += l_ind + "    set_target_properties(" + l_name + " PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)\n";
        p_out += l_ind + "else()\n";
        p_out += l_ind + "    message(WARNING \"lto is not supported for " + l_name + ": ${" + l_name + "_ipo_message}\")\n";
        p_out += l_ind + "endif()\n";
    }
}

void Generator::emitAssignment(const Expression& p_lhs, const Expression& p_rhs, const TargetDecl& p_target, std::string& p_out, int p_indentLevel)
{
    std::string l_visibility;
    std::string l_attr = resolveAttribute(p_lhs, p_target, l_visibility);
    static const std::set<std::string> s_pathAttrs = {"sources", "includes", "link_dirs", "copy_files", "pch", "assets", "headers"};
    struct AnchorScope
    {
        bool& flag;
        bool saved;
        AnchorScope(bool& p_flag, bool p_value) : flag(p_flag), saved(p_flag) { flag = p_value; }
        ~AnchorScope() { flag = saved; }
    } l_anchorScope(m_anchorPaths, s_pathAttrs.count(l_attr) > 0);
    std::string l_valueStr = exprToCmake(p_rhs);
    std::string l_ind = indent(p_indentLevel);

    if (l_attr == "export") return; // handled once per target in emitExportInstall

    if (l_attr == "runtime" || l_attr == "library" || l_attr == "headers")
    {
        if (!m_currentExport.empty() && l_attr != "headers") return; // merged into the single install(TARGETS ... EXPORT) call
        if (l_attr == "headers")
        {
            if (auto* l_tuple = std::get_if<TupleLiteral>(&p_rhs.value))
            {
                if (l_tuple->elements.size() == 2)
                {
                    std::string l_src = exprToCmake(*l_tuple->elements[0]);
                    std::string l_dst = exprToCmake(*l_tuple->elements[1]);
                    p_out += l_ind + "install(DIRECTORY " + l_src + " DESTINATION " + l_dst + ")\n";
                }
            }
        }
        else
        {
            std::string l_kinds;
            if (l_attr == "runtime") l_kinds = "RUNTIME DESTINATION " + l_valueStr;
            else if (p_target.type == TargetType::STATIC_LIBRARY) l_kinds = "ARCHIVE DESTINATION " + l_valueStr; // .a/.lib are archives
            else l_kinds = "LIBRARY DESTINATION " + l_valueStr + " ARCHIVE DESTINATION " + l_valueStr; // .so plus the Windows import lib
            p_out += l_ind + "install(TARGETS " + p_target.name + " " + l_kinds + ")\n";
        }
        return;
    }

    if (l_attr == "cmake")
    {
        // Escape hatch: lines go into the target's CMakeLists.txt exactly as written
        std::vector<const Expression*> l_lines;
        if (auto* l_list = std::get_if<ListLiteral>(&p_rhs.value))
        {
            for (const ExprPtr& l_elem : l_list->elements) l_lines.push_back(l_elem.get());
        }
        else
        {
            l_lines.push_back(&p_rhs);
        }
        for (const Expression* l_line : l_lines)
        {
            if (auto* l_str = std::get_if<StringLiteral>(&l_line->value)) p_out += rawCmakeBlock(l_str->value, l_ind);
        }
        return;
    }

    if (l_attr == "sources")
    {
        if (auto* l_list = std::get_if<ListLiteral>(&p_rhs.value))
        {
            bool l_hasGlob = false;
            SourceSet* l_set = nullptr;
            for (SourceSet& l_existing : m_sourceSets)
            {
                if (l_existing.target == p_target.name) l_set = &l_existing;
            }
            if (!l_set)
            {
                m_sourceSets.push_back({p_target.name, m_currentTargetDir, {}});
                l_set = &m_sourceSets.back();
            }
            for (const ExprPtr& l_elem : l_list->elements)
            {
                if (auto* l_s = std::get_if<StringLiteral>(&l_elem->value))
                {
                    const std::string& l_v = l_s->value;
                    bool l_anchored = l_v.size() > 2 && l_v.compare(0, 2, "//") == 0;
                    std::filesystem::path l_full = l_anchored ? std::filesystem::path(l_v.substr(2)) : std::filesystem::path(m_currentTargetDir) / l_v;
                    l_set->entries.push_back(l_full.lexically_normal().generic_string());
                    if (l_s->value.find('*') != std::string::npos)
                    {
                        l_hasGlob = true;
                        break;
                    }
                }
            }

            if (l_hasGlob)
            {
                // "dir/**/*.cpp" searches subfolders too (GLOB_RECURSE); plain patterns stay in one folder
                std::vector<std::string> l_flat, l_recursive;
                for (const ExprPtr& l_elem : l_list->elements)
                {
                    std::string l_text = exprToCmake(*l_elem);
                    size_t l_pos = l_text.find("/**/");
                    if (l_pos != std::string::npos)
                    {
                        l_text.replace(l_pos, 4, "/");
                        l_recursive.push_back(l_text);
                    }
                    else l_flat.push_back(l_text);
                }
                std::string l_sources;
                for (int l_pass = 0; l_pass < 2; ++l_pass)
                {
                    const std::vector<std::string>& l_patterns = l_pass == 0 ? l_flat : l_recursive;
                    if (l_patterns.empty()) continue;
                    std::string l_varName = p_target.name + "_SOURCES_" + std::to_string(m_globCounter++);
                    p_out += l_ind + (l_pass == 0 ? "file(GLOB " : "file(GLOB_RECURSE ") + l_varName + " CONFIGURE_DEPENDS\n";
                    for (const std::string& l_pattern : l_patterns) p_out += l_ind + "    " + l_pattern + "\n";
                    p_out += l_ind + ")\n";
                    l_sources += " ${" + l_varName + "}";
                }
                p_out += l_ind + "target_sources(" + p_target.name + " PRIVATE" + l_sources + ")\n";
            }
            else
            {
                p_out += l_ind + "target_sources(" + p_target.name + " PRIVATE\n";
                for (const ExprPtr& l_elem : l_list->elements)
                {
                    p_out += l_ind + "    " + exprToCmake(*l_elem) + "\n";
                    if (auto* l_s = std::get_if<StringLiteral>(&l_elem->value))
                    {
                        if (l_s->value.size() > 2 && l_s->value.compare(0, 2, "//") == 0) m_sourceRefs.push_back({".", l_s->value.substr(2)});
                        else m_sourceRefs.push_back({m_currentTargetDir, l_s->value});
                    }
                }
                p_out += l_ind + ")\n";
            }
        }
        return;
    }

    if (l_attr == "output_name" || l_attr == "version" || l_attr == "soversion")
    {
        std::string l_prop = l_attr == "output_name" ? "OUTPUT_NAME" : (l_attr == "version" ? "VERSION" : "SOVERSION");
        p_out += l_ind + "set_target_properties(" + p_target.name + " PROPERTIES " + l_prop + " " + l_valueStr + ")\n";
        return;
    }
    if (l_attr == "warnings" || l_attr == "warnings_as_errors" || l_attr == "sanitize" || l_attr == "lto")
    {
        emitQualityAttribute(l_attr, p_rhs, p_target, p_out, p_indentLevel);
        return;
    }
    if (l_attr == "features") { emitListCommand("target_compile_features", p_target, l_visibility, p_rhs, p_out, p_indentLevel); return; }

    if (l_attr == "includes") { emitListCommand("target_include_directories", p_target, l_visibility, p_rhs, p_out, p_indentLevel); return; }
    if (l_attr == "flags") { emitListCommand("target_compile_options", p_target, l_visibility, p_rhs, p_out, p_indentLevel); return; }
    if (l_attr == "link_dirs") { emitListCommand("target_link_directories", p_target, l_visibility, p_rhs, p_out, p_indentLevel); return; }

    if (l_attr == "link")
    {
        p_out += l_ind + "target_link_libraries(" + p_target.name + " " + l_visibility + "\n";
        if (auto* l_list = std::get_if<ListLiteral>(&p_rhs.value))
        {
            for (const ExprPtr& l_elem : l_list->elements)
            {
                p_out += l_ind + "    " + depToCmake(exprToCmake(*l_elem)) + "\n";
            }
        }
        p_out += l_ind + ")\n";
        return;
    }

    if (l_attr == "definitions")
    {
        if (auto* l_dict = std::get_if<DictLiteral>(&p_rhs.value))
        {
            for (const std::pair<ExprPtr, ExprPtr>& l_entry : l_dict->entries)
            {
                std::string l_key = exprToCmake(*l_entry.first);
                if (l_key.front() == '"') l_key = l_key.substr(1, l_key.size() - 2);
                p_out += l_ind + "target_compile_definitions(" + p_target.name + " " + l_visibility + " " + l_key + "=" + exprToCmake(*l_entry.second) + ")\n";
            }
        }
        return;
    }

    if (l_attr == "pch")
    {
        p_out += l_ind + "target_precompile_headers(" + p_target.name + " " + l_visibility;
        if (auto* l_list = std::get_if<ListLiteral>(&p_rhs.value))
        {
            for (const ExprPtr& l_elem : l_list->elements) p_out += " " + exprToCmake(*l_elem);
        }
        else
        {
            p_out += " " + l_valueStr;
        }
        p_out += ")\n";
        return;
    }

    if (l_attr == "assets")
    {
        std::vector<std::string> l_dirs;
        if (auto* l_list = std::get_if<ListLiteral>(&p_rhs.value))
        {
            for (const ExprPtr& l_elem : l_list->elements) l_dirs.push_back(exprToCmake(*l_elem));
        }
        else if (auto* l_str = std::get_if<StringLiteral>(&p_rhs.value))
        {
            l_dirs.push_back("\"" + l_str->value + "\"");
        }
        for (std::string l_dir : l_dirs)
        {
            if (l_dir.size() >= 2 && l_dir.front() == '"') l_dir = l_dir.substr(1, l_dir.size() - 2); // quote whole paths, not halves
            p_out += l_ind + "add_custom_command(TARGET " + p_target.name + " POST_BUILD\n";
            p_out += l_ind + "    COMMAND ${CMAKE_COMMAND} -E copy_directory\n";
            p_out += l_ind + "        \"${CMAKE_CURRENT_SOURCE_DIR}/" + l_dir + "\"\n";
            p_out += l_ind + "        \"$<TARGET_FILE_DIR:" + p_target.name + ">/" + l_dir + "\"\n";
            p_out += l_ind + ")\n";
        }
        return;
    }

    if (l_attr == "copy_files")
    {
        if (auto* l_list = std::get_if<ListLiteral>(&p_rhs.value))
        {
            for (const ExprPtr& l_elem : l_list->elements)
            {
                p_out += l_ind + "add_custom_command(TARGET " + p_target.name + " POST_BUILD\n";
                p_out += l_ind + "    COMMAND ${CMAKE_COMMAND} -E copy_if_different\n";
                p_out += l_ind + "        " + exprToCmake(*l_elem) + "\n";
                p_out += l_ind + "        $<TARGET_FILE_DIR:" + p_target.name + ">\n";
                p_out += l_ind + ")\n";
            }
        }
        return;
    }

    if (l_attr == "commands")
    {
        if (auto* l_list = std::get_if<ListLiteral>(&p_rhs.value))
        {
            for (const ExprPtr& l_elem : l_list->elements)
            {
                auto* l_tuple = std::get_if<TupleLiteral>(&l_elem->value);
                if (!l_tuple || l_tuple->elements.size() < 2) continue;

                std::string l_cmd = exprToCmake(*l_tuple->elements[0]);
                if (!l_cmd.empty() && l_cmd.front() == '"') l_cmd = l_cmd.substr(1, l_cmd.size() - 2);

                std::vector<std::string> l_outputs;
                if (auto* l_outList = std::get_if<ListLiteral>(&l_tuple->elements[1]->value))
                {
                    for (const ExprPtr& l_o : l_outList->elements)
                    {
                        l_outputs.push_back(exprToCmake(*l_o));
                    }
                }

                std::vector<std::string> l_depends;
                if (l_tuple->elements.size() >= 3)
                {
                    if (auto* l_depList = std::get_if<ListLiteral>(&l_tuple->elements[2]->value))
                    {
                        for (const ExprPtr& l_d : l_depList->elements)
                        {
                            l_depends.push_back(exprToCmake(*l_d));
                        }
                    }
                }

                p_out += l_ind + "add_custom_command(\n";
                for (const std::string& l_o : l_outputs)
                {
                    p_out += l_ind + "    OUTPUT " + l_o + "\n";
                }
                p_out += l_ind + "    COMMAND " + l_cmd + "\n";
                for (const std::string& l_d : l_depends)
                {
                    p_out += l_ind + "    DEPENDS " + l_d + "\n";
                }
                p_out += l_ind + "    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}\n";
                p_out += l_ind + ")\n";

                for (const std::string& l_o : l_outputs)
                {
                    p_out += l_ind + "target_sources(" + p_target.name + " PRIVATE " + l_o + ")\n";
                }
            }
        }
        return;
    }

    if (auto* l_idx = std::get_if<IndexAccess>(&p_lhs.value))
    {
        std::string l_baseVis;
        std::string l_baseAttr = resolveAttribute(*l_idx->object, p_target, l_baseVis);
        if (l_baseAttr == "definitions")
        {
            std::string l_key = exprToCmake(*l_idx->index);
            if (l_key.front() == '"') l_key = l_key.substr(1, l_key.size() - 2);
            std::string l_val = exprToCmake(p_rhs);
            p_out += l_ind + "target_compile_definitions(" + p_target.name + " " + l_baseVis + " " + l_key + "=" + l_val + ")\n";
        }
        return;
    }
}

void Generator::generateAugAssignment(const AugAssignStatement& p_aug, const TargetDecl& p_target, std::string& p_out, int p_indentLevel)
{
    // += is equivalent to = for CMake purposes since target_sources etc. accumulate
    emitAssignment(*p_aug.target, *p_aug.value, p_target, p_out, p_indentLevel);
}

std::string Generator::rawCmakeBlock(const std::string& p_text, const std::string& p_indent)
{
    std::string l_out;
    std::istringstream l_in(p_text);
    std::string l_line;
    while (std::getline(l_in, l_line)) l_out += (l_line.empty() ? "" : p_indent) + l_line + "\n";
    return l_out;
}

// Escapes a literal for use inside a CMake quoted argument. Variable references
// (${...}, $ENV{...}) are intentionally left intact so env imports keep working.
static std::string escapeCmake(const std::string& p_value)
{
    std::string l_out;
    for (char l_c : p_value)
    {
        if (l_c == '"' || l_c == '\\') l_out += '\\';
        if (l_c == '\n') { l_out += "\\n"; continue; }
        l_out += l_c;
    }
    return l_out;
}

std::string Generator::exprToCmake(const Expression& p_expr)
{
    if (auto* l_str = std::get_if<StringLiteral>(&p_expr.value))
    {
        // "//src/x.cpp" is anchored at the project root instead of the target's folder
        if (m_anchorPaths && l_str->value.size() > 2 && l_str->value.compare(0, 2, "//") == 0 && l_str->value[2] != '/')
        {
            return "\"${PROJECT_SOURCE_DIR}/" + escapeCmake(l_str->value.substr(2)) + "\"";
        }
        return "\"" + escapeCmake(l_str->value) + "\"";
    }
    if (auto* l_num = std::get_if<IntLiteral>(&p_expr.value))
    {
        return std::to_string(l_num->value);
    }
    if (auto* l_b = std::get_if<BoolLiteral>(&p_expr.value))
    {
        return l_b->value ? "1" : "0";
    }
    if (auto* l_id = std::get_if<Identifier>(&p_expr.value))
    {
        return l_id->name;
    }
    if (auto* l_dot = std::get_if<DotAccess>(&p_expr.value))
    {
        return l_dot->member;
    }
    if (auto* l_env = std::get_if<EnvVariable>(&p_expr.value))
    {
        return "$ENV{" + l_env->name + "}";
    }
    if (auto* l_concat = std::get_if<StringConcat>(&p_expr.value))
    {
        std::string l_left = exprToCmake(*l_concat->left);
        std::string l_right = exprToCmake(*l_concat->right);
        if (!l_left.empty() && l_left.front() == '"') l_left = l_left.substr(1, l_left.size() - 2);
        if (!l_right.empty() && l_right.front() == '"') l_right = l_right.substr(1, l_right.size() - 2);
        return "\"" + l_left + l_right + "\"";
    }
    return "";
}

// True when the text has an AND/OR outside any parentheses, i.e. embedding it in a larger condition could regroup it
static bool needsGrouping(const std::string& p_text)
{
    int l_depth = 0;
    for (size_t l_i = 0; l_i < p_text.size(); l_i++)
    {
        if (p_text[l_i] == '(') l_depth++;
        else if (p_text[l_i] == ')') l_depth--;
        else if (l_depth == 0 && p_text[l_i] == ' ' && (p_text.compare(l_i, 5, " AND ") == 0 || p_text.compare(l_i, 4, " OR ") == 0)) return true;
    }
    return false;
}

std::string Generator::conditionToCmake(const Expression& p_expr)
{
    if (auto* l_cmp = std::get_if<Comparison>(&p_expr.value))
    {
        std::string l_positive = comparisonToCmake(*l_cmp);
        return l_cmp->op == "!=" ? "NOT (" + l_positive + ")" : l_positive;
    }

    if (auto* l_bool = std::get_if<BoolOp>(&p_expr.value))
    {
        // Parenthesize anything compound so AND/OR precedence can never regroup the user's intent
        auto l_operand = [&](const Expression& p_e)
        {
            std::string l_text = conditionToCmake(p_e);
            return needsGrouping(l_text) ? "(" + l_text + ")" : l_text;
        };
        return l_operand(*l_bool->left) + (l_bool->op == "and" ? " AND " : " OR ") + l_operand(*l_bool->right);
    }

    if (auto* l_not = std::get_if<NotExpr>(&p_expr.value))
    {
        return "NOT (" + conditionToCmake(*l_not->operand) + ")";
    }

    if (auto* l_id = std::get_if<Identifier>(&p_expr.value))
    {
        // if(NAME) dereferences the variable itself; "${NAME}" would break on empty values
        if (m_optionalPackages.count(l_id->name)) return l_id->name + "_FOUND";
        return l_id->name;
    }

    if (auto* l_b = std::get_if<BoolLiteral>(&p_expr.value))
    {
        return l_b->value ? "TRUE" : "FALSE";
    }

    return exprToCmake(p_expr);
}

std::string Generator::comparisonToCmake(const Comparison& p_cmp)
{
    auto* l_leftId = std::get_if<Identifier>(&p_cmp.left->value);
    auto* l_rightStr = std::get_if<StringLiteral>(&p_cmp.right->value);

    if (l_leftId && l_rightStr)
    {
        if (l_leftId->name == "platform")
        {
            if (l_rightStr->value == "windows") return "WIN32";
            if (l_rightStr->value == "linux") return "UNIX AND NOT APPLE";
            if (l_rightStr->value == "macos") return "APPLE";
        }
        if (l_leftId->name == "compiler")
        {
            if (l_rightStr->value == "msvc") return "MSVC";
            if (l_rightStr->value == "gcc") return m_compilerIdVar + " STREQUAL \"GNU\"";
            if (l_rightStr->value == "clang") return m_compilerIdVar + " MATCHES \"Clang\"";
        }
        if (l_leftId->name == "build_type")
        {
            return "CMAKE_BUILD_TYPE STREQUAL \"" + cmakeConfigName(l_rightStr->value) + "\"";
        }
    }

    return conditionToCmake(*p_cmp.left) + " STREQUAL " + conditionToCmake(*p_cmp.right);
}

std::string Generator::cmakeConfigName(const std::string& p_buildType)
{
    if (p_buildType == "debug") return "Debug";
    if (p_buildType == "release") return "Release";
    if (p_buildType == "relwithdebinfo") return "RelWithDebInfo";
    if (p_buildType == "minsizerel") return "MinSizeRel";
    return p_buildType;
}

std::string Generator::resolveAttribute(const Expression& p_targetExpr, const TargetDecl& p_target, std::string& p_visibility)
{
    p_visibility = "PRIVATE";

    if (auto* l_dot = std::get_if<DotAccess>(&p_targetExpr.value))
    {
        if (l_dot->member == "exports")
        {
            p_visibility = "PUBLIC";
            return "exports";
        }

        if (auto* l_innerDot = std::get_if<DotAccess>(&l_dot->object->value))
        {
            if (l_innerDot->member == "exports")
            {
                p_visibility = "PUBLIC";
                if (p_target.type == TargetType::HEADER_ONLY)
                {
                    p_visibility = "INTERFACE";
                }
                return l_dot->member;
            }
        }

        if (auto* l_selfId = std::get_if<Identifier>(&l_dot->object->value))
        {
            if (l_selfId->name == "self")
            {
                if (p_target.type == TargetType::HEADER_ONLY)
                {
                    p_visibility = "INTERFACE";
                }
                return l_dot->member;
            }
        }
    }

    return "";
}

std::string Generator::targetDir(const TargetDecl& p_target) const
{
    if (p_target.path == "." || p_target.path == "./") return ".";
    if (!p_target.path.empty())
    {
        return p_target.path;
    }
    return p_target.name;
}

std::string Generator::cmakeTargetType(const TargetDecl& p_target) const
{
    switch (p_target.type)
    {
        case TargetType::EXECUTABLE:     return "add_executable";
        case TargetType::SHARED_LIBRARY: return "add_library";
        case TargetType::STATIC_LIBRARY: return "add_library";
        case TargetType::HEADER_ONLY:    return "add_library";
    }
    return "add_executable";
}

std::string Generator::indent(int p_level) const
{
    return std::string(p_level * 4, ' ');
}

std::string Generator::depToCmake(const std::string& p_depName) const
{
    size_t l_dot = p_depName.find('.');
    if (l_dot != std::string::npos)
    {
        std::string l_pkg = p_depName.substr(0, l_dot);
        std::string l_component = p_depName.substr(l_dot + 1);
        // fetched projects define their own target names (Catch2::Catch2WithMain), so their case is kept
        if (!m_fetchedNames.count(l_pkg)) std::transform(l_component.begin(), l_component.end(), l_component.begin(), ::tolower);
        return l_pkg + "::" + l_component;
    }

    if (isImportedPackage(p_depName))
    {
        return p_depName + "::" + p_depName;
    }

    return p_depName;
}

bool Generator::isImportedPackage(const std::string& p_name) const
{
    return m_importedPackages.count(p_name) > 0;
}

void Generator::collectPackageComponents()
{
    for (const TargetDecl& l_target : m_program.targets)
    {
        for (const Dependency& l_dep : l_target.dependencies)
        {
            size_t l_dot = l_dep.name.find('.');
            if (l_dot != std::string::npos)
            {
                std::string l_pkg = l_dep.name.substr(0, l_dot);
                std::string l_component = l_dep.name.substr(l_dot + 1);
                std::transform(l_component.begin(), l_component.end(), l_component.begin(), ::tolower);
                if (m_importedPackages.count(l_pkg))
                {
                    m_packageComponents[l_pkg].insert(l_component);
                }
            }
        }

        for (const Method& l_method : l_target.methods)
        {
            if (l_method.name != "configure") continue;
            for (const StmtPtr& l_stmt : l_method.body)
            {
                const auto l_scanLinkList = [&](const Expression& p_valueExpr)
                {
                    if (auto* l_list = std::get_if<ListLiteral>(&p_valueExpr.value))
                    {
                        for (const ExprPtr& l_elem : l_list->elements)
                        {
                            if (auto* l_id = std::get_if<Identifier>(&l_elem->value))
                            {
                            }
                            else if (auto* l_dotAccess = std::get_if<DotAccess>(&l_elem->value))
                            {
                            }
                        }
                    }
                };

                const auto l_checkLink = [&](const Expression& p_targetExpr, const Expression& p_valueExpr)
                {
                    std::string l_vis;
                    std::string l_attr = resolveAttribute(p_targetExpr, l_target, l_vis);
                    if (l_attr == "link")
                    {
                        l_scanLinkList(p_valueExpr);
                    }
                };

                if (auto* l_assign = std::get_if<AssignStatement>(&l_stmt->value))
                {
                    l_checkLink(*l_assign->target, *l_assign->value);
                }
                else if (auto* l_aug = std::get_if<AugAssignStatement>(&l_stmt->value))
                {
                    l_checkLink(*l_aug->target, *l_aug->value);
                }
            }
        }
    }
}

} // namespace pyke
