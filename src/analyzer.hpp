#pragma once

#include "ast.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace pyke
{

struct Diagnostic
{
    int line = 0;
    int column = 0; // 0 = unknown (the renderer points at the first non-blank character)
    bool warning = false;
    std::string message;
};

class Analyzer
{
public:
    explicit Analyzer(const Program& p_program);

    bool analyze();

    bool hasErrors() const { return !m_errors.empty(); }
    const std::vector<std::string>& errors() const { return m_errors; }
    const std::vector<std::string>& warnings() const { return m_warnings; }
    const std::vector<Diagnostic>& diagnostics() const { return m_diagnostics; }

private:
    void collectNames();
    void validateProject();
    void validateOptions();
    void validateTargets();
    void validateDependencies();
    void validateMethods();
    void validateBody(const TargetDecl& p_target, const Method& p_method, const std::vector<StmtPtr>& p_body, bool& p_sawSources);
    void validateStatement(const TargetDecl& p_target, const Method& p_method, const Statement& p_stmt, bool& p_sawSources);
    void validateAttributeValue(const TargetDecl& p_target, const std::string& p_attr, const Expression& p_value, int p_line);
    void validateCondition(const TargetDecl& p_target, const Expression& p_cond, int p_line);
    void detectCycles();

    bool findCycle(const std::string& p_target, std::vector<std::string>& p_stack, std::set<std::string>& p_done);

    static std::string basePackage(const std::string& p_name);
    static std::string normalizePath(const std::string& p_path);
    static bool isValidIdentifier(const std::string& p_name);

    std::string where(int p_line) const;
    // Messages may start with "Line N: " (legacy form); it is parsed into the diagnostic's line.
    void error(const std::string& p_message);
    void warning(const std::string& p_message);
    void errorAt(int p_line, int p_column, const std::string& p_message);
    void warningAt(int p_line, int p_column, const std::string& p_message);
    void record(bool p_warning, int p_line, int p_column, const std::string& p_message);

    const Program& m_program;
    std::set<std::string> m_targetNames;
    std::set<std::string> m_localTargetNames;
    std::set<std::string> m_importedPackages;
    std::set<std::string> m_envVars;
    std::map<std::string, const OptionDecl*> m_options;
    std::map<std::string, const TargetDecl*> m_targetsByName;
    std::vector<std::string> m_errors;
    std::vector<std::string> m_warnings;
    std::vector<Diagnostic> m_diagnostics;
};

} // namespace pyke
