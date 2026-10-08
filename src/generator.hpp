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

    // Every entry of a target's `sources`, as a path relative to the output root (for post-generation checks)
    struct SourceSet
    {
        std::string target;
        std::string targetDir;
        std::vector<std::string> entries;
    };

    std::vector<GeneratedFile> generate();
    const std::vector<SourceSet>& sourceSets() const { return m_sourceSets; }
    const std::vector<SourceRef>& sourceRefs() const { return m_sourceRefs; }

private:
    std::string generateRoot();
    std::string generatePresets();
    void emitPackageExports(std::ostringstream& p_out);

    std::string generateTarget(const TargetDecl& p_target);

    void generateMethodBody(const Method& p_method, const TargetDecl& p_target, std::string& p_out, int p_indentLevel);
    void generateStatement(const Statement& p_stmt, const TargetDecl& p_target, std::string& p_out, int p_indentLevel);
    void generateIfStatement(const IfStatement& p_ifStmt, const TargetDecl& p_target, std::string& p_out, int p_indentLevel);
    void generateAssignment(const AssignStatement& p_assign, const TargetDecl& p_target, std::string& p_out, int p_indentLevel);
    void generateAugAssignment(const AugAssignStatement& p_aug, const TargetDecl& p_target, std::string& p_out, int p_indentLevel);

    std::string exprToCmake(const Expression& p_expr);
    std::string conditionToCmake(const Expression& p_expr);
    static std::string rawCmakeBlock(const std::string& p_text, const std::string& p_indent);
    std::string comparisonToCmake(const Comparison& p_cmp);
    static std::string cmakeConfigName(const std::string& p_buildType);
    SourceSet& sourceSetFor(const TargetDecl& p_target);
    void emitModules(const std::string& p_visibility, const Expression& p_rhs, const TargetDecl& p_target, std::string& p_out, int p_indentLevel);
    void emitQualityAttribute(const std::string& p_attr, const Expression& p_rhs, const TargetDecl& p_target, std::string& p_out, int p_indentLevel);
    void emitAssignment(const Expression& p_lhs, const Expression& p_rhs, const TargetDecl& p_target, std::string& p_out, int p_indentLevel);

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
    std::set<std::string> m_optionalPackages;
    bool m_usesModules = false;
    bool m_anchorPaths = false; // while emitting path attributes, "//x" means "<project root>/x"
    std::set<std::string> m_fetchedNames;
    std::string m_currentExport; // package name of the target being generated, if exported
    bool m_hasC = false;
    std::string m_compilerIdVar = "CMAKE_CXX_COMPILER_ID";
    std::map<std::string, std::set<std::string>> m_packageComponents;
    int m_globCounter = 0;
    std::vector<SourceRef> m_sourceRefs;
    std::vector<SourceSet> m_sourceSets;
    std::string m_currentTargetDir;

    const Program& m_program;
};

} // namespace pyke
