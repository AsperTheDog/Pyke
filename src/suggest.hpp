#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace pyke
{

inline size_t editDistance(const std::string& p_a, const std::string& p_b)
{
    std::vector<size_t> l_prev(p_b.size() + 1), l_cur(p_b.size() + 1);
    for (size_t l_j = 0; l_j <= p_b.size(); l_j++) l_prev[l_j] = l_j;
    for (size_t l_i = 1; l_i <= p_a.size(); l_i++)
    {
        l_cur[0] = l_i;
        for (size_t l_j = 1; l_j <= p_b.size(); l_j++)
        {
            size_t l_sub = l_prev[l_j - 1] + (p_a[l_i - 1] == p_b[l_j - 1] ? 0 : 1);
            l_cur[l_j] = std::min({l_prev[l_j] + 1, l_cur[l_j - 1] + 1, l_sub});
        }
        std::swap(l_prev, l_cur);
    }
    return l_prev[p_b.size()];
}

// Closest candidate to p_word, or "" when nothing is plausibly a typo of it.
template <typename Container>
std::string closestMatch(const std::string& p_word, const Container& p_candidates)
{
    std::string l_best;
    size_t l_bestDist = p_word.size() / 3 + 2; // allow roughly one edit per three characters
    for (const std::string& l_c : p_candidates)
    {
        size_t l_d = editDistance(p_word, l_c);
        if (l_d < l_bestDist || (l_d == l_bestDist && l_best.empty() && l_d < p_word.size()))
        {
            l_bestDist = l_d;
            l_best = l_c;
        }
    }
    return l_best;
}

// " (did you mean 'x'?)" or "" - ready to append to a message.
template <typename Container>
std::string didYouMean(const std::string& p_word, const Container& p_candidates)
{
    std::string l_match = closestMatch(p_word, p_candidates);
    return l_match.empty() ? "" : " (did you mean '" + l_match + "'?)";
}

} // namespace pyke
