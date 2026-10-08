#pragma once

#include <string>

namespace pyke
{

// Canonical layout for a .pyke file: 4-space indentation, one space after commas and around operators,
// no padding inside brackets, at most one blank line in a row, LF line endings, one trailing newline.
// Returns the source unchanged (and sets p_error) if formatting would alter its tokens.
std::string formatSource(const std::string& p_source, std::string* p_error = nullptr);

} // namespace pyke
