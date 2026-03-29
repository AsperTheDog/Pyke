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
        std::string l_result;
        l_result += m_filename + ":" + std::to_string(p_line) + ":" + std::to_string(p_column) + ": error: " + p_message + "\n";

        if (p_line >= 1 && p_line <= static_cast<int>(m_lines.size()))
        {
            const std::string& l_srcLine = m_lines[p_line - 1];
            l_result += "  " + std::to_string(p_line) + " | " + l_srcLine + "\n";

            std::string l_lineNumStr = std::to_string(p_line);
            std::string l_padding(2 + l_lineNumStr.size() + 3, ' ');
            if (p_column >= 1)
            {
                l_padding += std::string(p_column - 1, ' ');
            }
            l_result += l_padding + "^\n";
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
