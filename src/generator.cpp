#include "generator.hpp"
#include <algorithm>
#include <sstream>

namespace pyke
{

Generator::Generator(const Program& p_program)
    : m_program(p_program)
{
    for (const TargetDecl& l_target : m_program.targets)
    {
        m_targetNames.insert(l_target.name);
    }
    for (const ImportDecl& l_import : m_program.imports)
    {
        for (const std::string& l_pkg : l_import.packages)
        {
            m_importedPackages.insert(l_pkg);
        }
    }
    for (const FetchDecl& l_fetch : m_program.fetches)
    {
        m_targetNames.insert(l_fetch.name);
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
        l_out << " LANGUAGES CXX)\n";

        if (!m_program.project->lang.empty())
        {
            std::string l_stdVer = m_program.project->lang;
            if (l_stdVer.substr(0, 3) == "c++")
            {
                l_stdVer = l_stdVer.substr(3);
            }
            l_out << "\nset(CMAKE_CXX_STANDARD " << l_stdVer << ")\n";
            l_out << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n";
        }

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
                l_out << "if(${" << l_import.condition << "})\n    ";
            }
            for (const std::string& l_pkg : l_import.packages)
            {
                if (l_conditional && &l_pkg != &l_import.packages[0]) l_out << "    ";
                l_out << "find_package(" << l_pkg << " REQUIRED";
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
            l_out << "FetchContent_Declare(" << l_fetch.name << "\n";
            l_out << "    GIT_REPOSITORY https://github.com/" << l_fetch.repo << ".git\n";
            if (!l_fetch.tag.empty())
            {
                l_out << "    GIT_TAG " << l_fetch.tag << "\n";
            }
            l_out << ")\n";
        }
        l_out << "\n";
        for (const FetchDecl& l_fetch : m_program.fetches)
        {
            l_out << "FetchContent_MakeAvailable(" << l_fetch.name << ")\n";
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

    if (!m_program.targets.empty())
    {
        l_out << "\n";
        for (const TargetDecl& l_target : m_program.targets)
        {
            l_out << "add_subdirectory(" << targetDir(l_target) << ")\n";
        }
    }

    return l_out.str();
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
        l_result += "\nget_target_property(" + p_target.name + "_ALL_SOURCES " + p_target.name + " SOURCES)\n";
        l_result += "source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${" + p_target.name + "_ALL_SOURCES})\n";
    }

    if (p_target.test && p_target.type == TargetType::EXECUTABLE)
    {
        l_result += "\nadd_test(NAME " + p_target.name + " COMMAND " + p_target.name + ")\n";
    }

    if (p_target.copyDlls && p_target.type == TargetType::EXECUTABLE)
    {
        l_result += "\nadd_custom_command(TARGET " + p_target.name + " POST_BUILD\n";
        l_result += "    COMMAND ${CMAKE_COMMAND} -E copy_if_different\n";
        l_result += "        $<TARGET_RUNTIME_DLLS:" + p_target.name + ">\n";
        l_result += "        $<TARGET_FILE_DIR:" + p_target.name + ">\n";
        l_result += "    COMMAND_EXPAND_LISTS\n";
        l_result += ")\n";
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
            if (l_leftId && l_leftId->name == "build_type" && l_rightStr)
            {
                std::string l_bt = l_rightStr->value;
                if (!l_bt.empty()) l_bt[0] = static_cast<char>(toupper(l_bt[0]));
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
        for (const ExprPtr& l_elem : l_list->elements)
        {
            p_out += l_ind + "    " + exprToCmake(*l_elem) + "\n";
        }
    }
    p_out += l_ind + ")\n";
}

void Generator::generateAssignment(const AssignStatement& p_assign, const TargetDecl& p_target, std::string& p_out, int p_indentLevel)
{
    std::string l_visibility;
    std::string l_attr = resolveAttribute(*p_assign.target, p_target, l_visibility);
    std::string l_valueStr = exprToCmake(*p_assign.value);
    std::string l_ind = indent(p_indentLevel);

    if (l_attr == "runtime" || l_attr == "library" || l_attr == "headers")
    {
        if (l_attr == "headers")
        {
            if (auto* l_tuple = std::get_if<TupleLiteral>(&p_assign.value->value))
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
            p_out += l_ind + "install(TARGETS " + p_target.name + " " + (l_attr == "runtime" ? "RUNTIME" : "LIBRARY") + " DESTINATION " + l_valueStr + ")\n";
        }
        return;
    }

    if (l_attr == "sources")
    {
        if (auto* l_list = std::get_if<ListLiteral>(&p_assign.value->value))
        {
            bool l_hasGlob = false;
            for (const ExprPtr& l_elem : l_list->elements)
            {
                if (auto* l_s = std::get_if<StringLiteral>(&l_elem->value))
                {
                    if (l_s->value.find('*') != std::string::npos)
                    {
                        l_hasGlob = true;
                        break;
                    }
                }
            }

            if (l_hasGlob)
            {
                std::string l_varName = p_target.name + "_SOURCES_" + std::to_string(m_globCounter++);
                p_out += l_ind + "file(GLOB " + l_varName + " CONFIGURE_DEPENDS\n";
                for (const ExprPtr& l_elem : l_list->elements)
                {
                    p_out += l_ind + "    " + exprToCmake(*l_elem) + "\n";
                }
                p_out += l_ind + ")\n";
                p_out += l_ind + "target_sources(" + p_target.name + " PRIVATE ${" + l_varName + "})\n";
            }
            else
            {
                p_out += l_ind + "target_sources(" + p_target.name + " PRIVATE\n";
                for (const ExprPtr& l_elem : l_list->elements)
                {
                    p_out += l_ind + "    " + exprToCmake(*l_elem) + "\n";
                    if (auto* l_s = std::get_if<StringLiteral>(&l_elem->value))
                    {
                        m_sourceRefs.push_back({m_currentTargetDir, l_s->value});
                    }
                }
                p_out += l_ind + ")\n";
            }
        }
        return;
    }

    if (l_attr == "includes") { emitListCommand("target_include_directories", p_target, l_visibility, *p_assign.value, p_out, p_indentLevel); return; }
    if (l_attr == "flags") { emitListCommand("target_compile_options", p_target, l_visibility, *p_assign.value, p_out, p_indentLevel); return; }
    if (l_attr == "link_dirs") { emitListCommand("target_link_directories", p_target, l_visibility, *p_assign.value, p_out, p_indentLevel); return; }

    if (l_attr == "link")
    {
        p_out += l_ind + "target_link_libraries(" + p_target.name + " " + l_visibility + "\n";
        if (auto* l_list = std::get_if<ListLiteral>(&p_assign.value->value))
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
        if (auto* l_dict = std::get_if<DictLiteral>(&p_assign.value->value))
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
        p_out += l_ind + "target_precompile_headers(" + p_target.name + " " + l_visibility + " " + l_valueStr + ")\n";
        return;
    }

    if (l_attr == "assets")
    {
        std::vector<std::string> l_dirs;
        if (auto* l_list = std::get_if<ListLiteral>(&p_assign.value->value))
        {
            for (const ExprPtr& l_elem : l_list->elements) l_dirs.push_back(exprToCmake(*l_elem));
        }
        else if (auto* l_str = std::get_if<StringLiteral>(&p_assign.value->value))
        {
            l_dirs.push_back("\"" + l_str->value + "\"");
        }
        for (const std::string& l_dir : l_dirs)
        {
            p_out += l_ind + "add_custom_command(TARGET " + p_target.name + " POST_BUILD\n";
            p_out += l_ind + "    COMMAND ${CMAKE_COMMAND} -E copy_directory\n";
            p_out += l_ind + "        ${CMAKE_CURRENT_SOURCE_DIR}/" + l_dir + "\n";
            p_out += l_ind + "        $<TARGET_FILE_DIR:" + p_target.name + ">/" + l_dir + "\n";
            p_out += l_ind + ")\n";
        }
        return;
    }

    if (l_attr == "copy_files")
    {
        if (auto* l_list = std::get_if<ListLiteral>(&p_assign.value->value))
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
        if (auto* l_list = std::get_if<ListLiteral>(&p_assign.value->value))
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

    if (auto* l_idx = std::get_if<IndexAccess>(&p_assign.target->value))
    {
        std::string l_baseVis;
        std::string l_baseAttr = resolveAttribute(*l_idx->object, p_target, l_baseVis);
        if (l_baseAttr == "definitions")
        {
            std::string l_key = exprToCmake(*l_idx->index);
            if (l_key.front() == '"') l_key = l_key.substr(1, l_key.size() - 2);
            std::string l_val = exprToCmake(*p_assign.value);
            p_out += l_ind + "target_compile_definitions(" + p_target.name + " " + l_baseVis + " " + l_key + "=" + l_val + ")\n";
        }
        return;
    }
}

void Generator::generateAugAssignment(const AugAssignStatement& p_aug, const TargetDecl& p_target, std::string& p_out, int p_indentLevel)
{
    // += is equivalent to = for CMake purposes since target_sources etc. accumulate
    AssignStatement l_equiv;
    l_equiv.target = std::unique_ptr<Expression>(const_cast<Expression*>(p_aug.target.get()));
    l_equiv.value = std::unique_ptr<Expression>(const_cast<Expression*>(p_aug.value.get()));
    generateAssignment(l_equiv, p_target, p_out, p_indentLevel);
    l_equiv.target.release();
    l_equiv.value.release();
}

std::string Generator::exprToCmake(const Expression& p_expr)
{
    if (auto* l_str = std::get_if<StringLiteral>(&p_expr.value))
    {
        return "\"" + l_str->value + "\"";
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

std::string Generator::conditionToCmake(const Expression& p_expr)
{
    if (auto* l_cmp = std::get_if<Comparison>(&p_expr.value))
    {
        auto* l_leftId = std::get_if<Identifier>(&l_cmp->left->value);
        auto* l_rightStr = std::get_if<StringLiteral>(&l_cmp->right->value);

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
                if (l_rightStr->value == "gcc") return "CMAKE_CXX_COMPILER_ID STREQUAL \"GNU\"";
                if (l_rightStr->value == "clang") return "CMAKE_CXX_COMPILER_ID STREQUAL \"Clang\"";
            }
            if (l_leftId->name == "build_type")
            {
                std::string l_bt = l_rightStr->value;
                if (!l_bt.empty()) l_bt[0] = static_cast<char>(toupper(l_bt[0]));
                return "CMAKE_BUILD_TYPE STREQUAL \"" + l_bt + "\"";
            }
        }

        std::string l_left = conditionToCmake(*l_cmp->left);
        std::string l_right = conditionToCmake(*l_cmp->right);
        return l_left + " STREQUAL " + l_right;
    }

    if (auto* l_id = std::get_if<Identifier>(&p_expr.value))
    {
        return "${" + l_id->name + "}";
    }

    return exprToCmake(p_expr);
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
        std::transform(l_component.begin(), l_component.end(), l_component.begin(), ::tolower);
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
