#include "analyzer.hpp"
#include <algorithm>
#include <map>
#include <sstream>

namespace pyke
{

Analyzer::Analyzer(const Program& p_program)
    : m_program(p_program) {}

bool Analyzer::analyze()
{
    collectNames();
    validateDependencies();
    validateMethods();
    detectCycles();
    return !hasErrors();
}

void Analyzer::collectNames()
{
    for (const TargetDecl& l_target : m_program.targets)
    {
        if (m_targetNames.count(l_target.name))
        {
            std::ostringstream l_oss;
            l_oss << "Line " << l_target.line << ": duplicate target name: '" << l_target.name << "'";
            error(l_oss.str());
        }
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
}

void Analyzer::validateDependencies()
{
    for (const TargetDecl& l_target : m_program.targets)
    {
        for (const Dependency& l_dep : l_target.dependencies)
        {
            std::string l_base = basePackage(l_dep.name);

            if (!m_targetNames.count(l_base) && !m_importedPackages.count(l_base))
            {
                std::ostringstream l_oss;
                l_oss << "Line " << l_target.line << ": target '" << l_target.name << "': unknown dependency '" << l_dep.name << "' (not a target or imported package)";
                error(l_oss.str());
            }
        }
    }
}

void Analyzer::validateMethods()
{
    for (const TargetDecl& l_target : m_program.targets)
    {
        bool l_hasConfigure = false;
        for (const Method& l_method : l_target.methods)
        {
            if (l_method.name == "configure")
            {
                l_hasConfigure = true;
            }
            else if (l_method.name == "install")
            {
            }
            else
            {
                std::ostringstream l_oss;
                l_oss << "Line " << l_method.line << ": target '" << l_target.name << "': unknown method '" << l_method.name << "' (only 'configure' and 'install' are allowed)";
                error(l_oss.str());
            }
        }
        if (!l_hasConfigure)
        {
            std::ostringstream l_oss;
            l_oss << "Line " << l_target.line << ": target '" << l_target.name << "': missing required 'configure' method";
            error(l_oss.str());
        }
    }
}

void Analyzer::detectCycles()
{
    std::set<std::string> l_visiting;
    std::set<std::string> l_visited;

    for (const TargetDecl& l_target : m_program.targets)
    {
        if (!l_visited.count(l_target.name))
        {
            if (hasCycleFrom(l_target.name, l_visiting, l_visited))
            {
            }
        }
    }
}

bool Analyzer::hasCycleFrom(const std::string& p_target, std::set<std::string>& p_visiting, std::set<std::string>& p_visited) const
{
    p_visiting.insert(p_target);

    for (const TargetDecl& l_t : m_program.targets)
    {
        if (l_t.name == p_target)
        {
            for (const Dependency& l_dep : l_t.dependencies)
            {
                std::string l_base = basePackage(l_dep.name);

                if (!m_targetNames.count(l_base)) continue;

                if (p_visiting.count(l_base))
                {
                    std::ostringstream l_oss;
                    l_oss << "Circular dependency detected: '" << p_target << "' -> '" << l_base << "'";
                    const_cast<Analyzer*>(this)->error(l_oss.str());
                    return true;
                }

                if (!p_visited.count(l_base))
                {
                    if (hasCycleFrom(l_base, p_visiting, p_visited))
                    {
                        return true;
                    }
                }
            }
            break;
        }
    }

    p_visiting.erase(p_target);
    p_visited.insert(p_target);
    return false;
}

std::string Analyzer::basePackage(const std::string& p_name)
{
    size_t l_dot = p_name.find('.');
    if (l_dot != std::string::npos)
    {
        return p_name.substr(0, l_dot);
    }
    return p_name;
}

void Analyzer::error(const std::string& p_message)
{
    m_errors.push_back(p_message);
}

} // namespace pyke
