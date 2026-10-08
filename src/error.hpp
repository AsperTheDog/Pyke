#pragma once

#include <string>
#include <vector>

namespace pyke
{

class SourceFile
{
public:
    explicit SourceFile(const std::string& p_source, const std::string& p_filename = "<input>") : m_filename(p_filename)
    {
        size_t l_start = 0;
        while (l_start < p_source.size())
        {
            size_t l_end = p_source.find('\n', l_start);
            if (l_end == std::string::npos)
            {
                m_lines.push_back(p_source.substr(l_start));
                break;
            }
            size_t l_lineEnd = (l_end > l_start && p_source[l_end - 1] == '\r') ? l_end - 1 : l_end;
            m_lines.push_back(p_source.substr(l_start, l_lineEnd - l_start));
            l_start = l_end + 1;
        }
    }

    std::string formatError(int p_line, int p_column, const std::string& p_message) const
    {
        return formatDiagnostic("error", p_line, p_column, p_message);
    }

    // file:line:col: severity: message, then the source line with a caret under the column.
    // A column of 0 means "unknown": the caret goes under the first non-blank character.
    std::string formatDiagnostic(const std::string& p_severity, int p_line, int p_column, const std::string& p_message) const
    {
        std::string l_result;
        if (p_line < 1)
        {
            return m_filename + ": " + p_severity + ": " + p_message + "\n";
        }

        bool l_known = p_line <= static_cast<int>(m_lines.size());
        const std::string l_srcLine = l_known ? m_lines[p_line - 1] : "";
        int l_column = p_column;
        if (l_column < 1)
        {
            size_t l_first = l_srcLine.find_first_not_of(" \t");
            l_column = l_first == std::string::npos ? 1 : static_cast<int>(l_first) + 1;
        }

        l_result += m_filename + ":" + std::to_string(p_line) + ":" + std::to_string(l_column) + ": " + p_severity + ": " + p_message + "\n";

        if (l_known)
        {
            std::string l_num = std::to_string(p_line);
            l_result += "  " + l_num + " | " + l_srcLine + "\n";

            // keep tabs in the padding so the caret lines up whatever the viewer's tab width
            std::string l_pad;
            for (int l_i = 0; l_i < l_column - 1; l_i++)
            {
                l_pad += (l_i < static_cast<int>(l_srcLine.size()) && l_srcLine[l_i] == '\t') ? '\t' : ' ';
            }
            l_result += "  " + std::string(l_num.size(), ' ') + " | " + l_pad + "^\n";
        }

        return l_result;
    }

    const std::string& filename() const { return m_filename; }
    const std::vector<std::string>& lines() const { return m_lines; }

private:
    std::string m_filename;
    std::vector<std::string> m_lines;
};

} // namespace pyke
