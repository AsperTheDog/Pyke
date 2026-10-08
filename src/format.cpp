#include "format.hpp"
#include "lexer.hpp"
#include <sstream>
#include <vector>

namespace pyke
{

namespace
{

std::vector<std::string> splitLines(const std::string& p_source)
{
    std::vector<std::string> l_lines;
    std::string l_cur;
    for (size_t l_i = 0; l_i < p_source.size(); ++l_i)
    {
        char l_c = p_source[l_i];
        if (l_c == '\r')
        {
            if (l_i + 1 < p_source.size() && p_source[l_i + 1] == '\n') ++l_i;
            l_lines.push_back(l_cur);
            l_cur.clear();
        }
        else if (l_c == '\n')
        {
            l_lines.push_back(l_cur);
            l_cur.clear();
        }
        else l_cur += l_c;
    }
    if (!l_cur.empty()) l_lines.push_back(l_cur);
    return l_lines;
}

int indentWidth(const std::string& p_line, size_t& p_end)
{
    int l_width = 0;
    p_end = 0;
    while (p_end < p_line.size() && (p_line[p_end] == ' ' || p_line[p_end] == '\t'))
    {
        l_width += p_line[p_end] == '\t' ? 4 : 1;
        ++p_end;
    }
    return l_width;
}

bool isOpen(char p_c) { return p_c == '(' || p_c == '[' || p_c == '{'; }
bool isClose(char p_c) { return p_c == ')' || p_c == ']' || p_c == '}'; }

// Re-spaces the code of one line; p_stack carries open brackets across lines
std::string formatCode(const std::string& p_text, std::string& p_stack)
{
    // `from github import "x/y" as z, tag="1.0"`: the = signs are keyword arguments even outside parentheses
    bool l_importLine = p_text.rfind("from ", 0) == 0;
    std::string l_out;
    auto l_space = [&]() { if (!l_out.empty() && l_out.back() != ' ' && !isOpen(l_out.back())) l_out += ' '; };
    size_t l_i = 0;
    while (l_i < p_text.size())
    {
        char l_c = p_text[l_i];

        if (l_c == '"')
        {
            size_t l_j = l_i + 1;
            while (l_j < p_text.size() && p_text[l_j] != '"')
            {
                if (p_text[l_j] == '\\' && l_j + 1 < p_text.size()) ++l_j;
                ++l_j;
            }
            l_out += p_text.substr(l_i, l_j + 1 - l_i);
            l_i = l_j + 1;
        }
        else if (l_c == '#')
        {
            if (!l_out.empty() && l_out.back() != ' ') l_out += "  ";
            else if (!l_out.empty()) { while (l_out.size() > 1 && l_out[l_out.size() - 2] == ' ') l_out.pop_back(); l_out += ' '; }
            l_out += p_text.substr(l_i);
            return l_out;
        }
        else if (l_c == ' ' || l_c == '\t')
        {
            while (l_i < p_text.size() && (p_text[l_i] == ' ' || p_text[l_i] == '\t')) ++l_i;
            // keep one space unless a bracket, comma or colon follows
            if (l_i < p_text.size() && !isClose(p_text[l_i]) && p_text[l_i] != ',' && p_text[l_i] != ':' && p_text[l_i] != '#') l_space();
            else if (l_i < p_text.size() && p_text[l_i] == '#') l_space();
        }
        else if (isOpen(l_c))
        {
            p_stack += l_c;
            l_out += l_c;
            ++l_i;
            while (l_i < p_text.size() && (p_text[l_i] == ' ' || p_text[l_i] == '\t')) ++l_i;
        }
        else if (isClose(l_c))
        {
            while (!l_out.empty() && l_out.back() == ' ') l_out.pop_back();
            if (!p_stack.empty()) p_stack.pop_back();
            l_out += l_c;
            ++l_i;
        }
        else if (l_c == ',')
        {
            while (!l_out.empty() && l_out.back() == ' ') l_out.pop_back();
            l_out += ',';
            ++l_i;
            while (l_i < p_text.size() && (p_text[l_i] == ' ' || p_text[l_i] == '\t')) ++l_i;
            if (l_i < p_text.size() && !isClose(p_text[l_i]) && p_text[l_i] != '#') l_out += ' ';
        }
        else if (l_c == ':' && !p_stack.empty() && p_stack.back() == '{')
        {
            while (!l_out.empty() && l_out.back() == ' ') l_out.pop_back();
            l_out += ':';
            ++l_i;
            while (l_i < p_text.size() && (p_text[l_i] == ' ' || p_text[l_i] == '\t')) ++l_i;
            if (l_i < p_text.size() && !isClose(p_text[l_i])) l_out += ' ';
        }
        else if (l_c == ':')
        {
            while (!l_out.empty() && l_out.back() == ' ') l_out.pop_back();
            l_out += ':';
            ++l_i;
        }
        else if (l_c == '=' || l_c == '!' || l_c == '+' || l_c == '<' || l_c == '>')
        {
            std::string l_op(1, l_c);
            if (l_i + 1 < p_text.size() && p_text[l_i + 1] == '=') l_op += '=';
            l_i += l_op.size();
            while (!l_out.empty() && l_out.back() == ' ') l_out.pop_back();
            bool l_kwarg = l_op == "=" && (l_importLine || (!p_stack.empty() && p_stack.back() == '('));
            if (l_kwarg) { l_out += '='; }
            else
            {
                l_space();
                l_out += l_op;
                l_out += ' ';
            }
            while (l_i < p_text.size() && (p_text[l_i] == ' ' || p_text[l_i] == '\t')) ++l_i;
        }
        else
        {
            l_out += l_c;
            ++l_i;
        }
    }
    while (!l_out.empty() && l_out.back() == ' ') l_out.pop_back();
    return l_out;
}

std::string tokenDump(const std::string& p_source)
{
    Lexer l_lexer(p_source);
    std::string l_dump;
    for (const Token& l_tok : l_lexer.tokenize())
    {
        l_dump += std::to_string(static_cast<int>(l_tok.type)) + ":" + l_tok.value + "\n";
    }
    return l_dump;
}

} // namespace

std::string formatSource(const std::string& p_source, std::string* p_error)
{
    std::vector<std::string> l_lines = splitLines(p_source);
    std::vector<std::string> l_out;
    std::vector<int> l_widths{0};
    std::string l_brackets;
    int l_blank = 0;

    for (const std::string& l_raw : l_lines)
    {
        size_t l_start = 0;
        int l_width = indentWidth(l_raw, l_start);
        std::string l_text = l_raw.substr(l_start);
        while (!l_text.empty() && (l_text.back() == ' ' || l_text.back() == '\t')) l_text.pop_back();

        if (l_text.empty())
        {
            if (!l_out.empty() && ++l_blank == 1) l_out.push_back("");
            continue;
        }
        l_blank = 0;

        bool l_continuation = !l_brackets.empty();
        bool l_commentOnly = l_text[0] == '#';
        std::string l_prefix;
        if (l_continuation)
        {
            // Inside brackets: keep the author's alignment, only normalize tabs
            l_prefix = std::string(static_cast<size_t>(l_width), ' ');
        }
        else
        {
            if (!l_commentOnly)
            {
                while (l_widths.size() > 1 && l_width < l_widths.back()) l_widths.pop_back();
                if (l_width > l_widths.back()) l_widths.push_back(l_width);
            }
            size_t l_level = l_widths.size() - 1;
            if (l_commentOnly && l_width > l_widths.back()) l_level = l_widths.size();
            else if (l_commentOnly) { l_level = 0; for (size_t l_k = 0; l_k < l_widths.size(); ++l_k) if (l_widths[l_k] <= l_width) l_level = l_k; }
            l_prefix = std::string(l_level * 4, ' ');
        }
        l_out.push_back(l_prefix + formatCode(l_text, l_brackets));
    }

    while (!l_out.empty() && l_out.back().empty()) l_out.pop_back();
    std::string l_result;
    for (const std::string& l_l : l_out) l_result += l_l + "\n";

    if (tokenDump(l_result) != tokenDump(p_source))
    {
        if (p_error) *p_error = "formatting would change the meaning of the file; left untouched";
        return p_source;
    }
    return l_result;
}

} // namespace pyke
