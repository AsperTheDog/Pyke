#pragma once

#include "ast.hpp"
#include <set>
#include <string>
#include <vector>

namespace pyke
{

class Analyzer
{
public:
    explicit Analyzer(const Program& p_program);

    bool analyze();

    bool hasErrors() const { return !m_errors.empty(); }
    const std::vector<std::string>& errors() const { return m_errors; }

private:
    void collectNames();
    void validateDependencies();
    void validateMethods();
    void detectCycles();

    bool hasCycleFrom(const std::string& p_target, std::set<std::string>& p_visiting, std::set<std::string>& p_visited) const;

    static std::string basePackage(const std::string& p_name);

    void error(const std::string& p_message);

    const Program& m_program;
    std::set<std::string> m_targetNames;
    std::set<std::string> m_importedPackages;
    std::vector<std::string> m_errors;
};

} // namespace pyke
