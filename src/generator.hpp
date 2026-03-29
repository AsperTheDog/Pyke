#pragma once

#include "ast.hpp"
#include <map>
#include <set>
#include <string>

namespace pyke
{

struct GeneratedFile
{
    std::string path;
    std::string content;
};

class Generator
{
public:
    explicit Generator(const Program& p_program);

    struct SourceRef
    {
        std::string targetDir;
        std::string file;
    };

    std::vector<GeneratedFile> generate();
    const std::vector<SourceRef>& sourceRefs() const { return m_sourceRefs; }

private:
    std::string generateRoot();
    std::string generatePresets();

    std::string generateTarget(const TargetDecl& p_target);

    void generateMethodBody(const Method& p_method, const TargetDecl& p_target, std::string& p_out, int p_indentLevel);
    void generateStatement(const Statement& p_stmt, const TargetDecl& p_target, std::string& p_out, int p_indentLevel);
    void generateIfStatement(const IfStatement& p_ifStmt, const TargetDecl& p_target, std::string& p_out, int p_indentLevel);
    void generateAssignment(const AssignStatement& p_assign, const TargetDecl& p_target, std::string& p_out, int p_indentLevel);
    void generateAugAssignment(const AugAssignStatement& p_aug, const TargetDecl& p_target, std::string& p_out, int p_indentLevel);

    std::string exprToCmake(const Expression& p_expr);
    std::string conditionToCmake(const Expression& p_expr);

    void emitListCommand(const std::string& p_cmakeCmd, const TargetDecl& p_target, const std::string& p_visibility, const Expression& p_value, std::string& p_out, int p_indentLevel);
    std::string resolveAttribute(const Expression& p_targetExpr, const TargetDecl& p_target, std::string& p_visibility);
    std::string targetDir(const TargetDecl& p_target) const;
    std::string cmakeTargetType(const TargetDecl& p_target) const;
    std::string indent(int p_level) const;
    std::string depToCmake(const std::string& p_depName) const;
    bool isImportedPackage(const std::string& p_name) const;
    void collectPackageComponents();

    std::set<std::string> m_targetNames;
    std::set<std::string> m_importedPackages;
    std::map<std::string, std::set<std::string>> m_packageComponents;
    int m_globCounter = 0;
    std::vector<SourceRef> m_sourceRefs;
    std::string m_currentTargetDir;

    const Program& m_program;
};

} // namespace pyke
