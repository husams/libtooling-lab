#pragma once
#include <expected>
#include <string>
#include <string_view>

namespace facts::config::detail {
inline bool invalidCharacters(std::string_view text) {
  return text.find('\0') != text.npos || text.find('\n') != text.npos ||
         text.find('\\') != text.npos;
}
inline std::expected<std::string, std::string>
substitute(std::string_view text, std::string_view relative,
           std::string_view filename) {
  if (text.empty() || text.back() == '/' || invalidCharacters(text))
    return std::unexpected("conf_template is invalid");
  if (invalidCharacters(relative) || invalidCharacters(filename))
    return std::unexpected("conf_template has an invalid project path");
  std::string result;
  for (std::size_t at = 0; at < text.size();) {
    if (text[at] == '}') return std::unexpected("conf_template has unmatched braces");
    if (text[at] != '{') { result += text[at++]; continue; }
    const auto end = text.find('}', at);
    if (end == text.npos) return std::unexpected("conf_template has unmatched braces");
    const auto key = text.substr(at + 1, end - at - 1);
    if (key == "relative_path") result += relative;
    else if (key == "filename") result += filename;
    else return std::unexpected("conf_template has unknown placeholder: " + std::string(key));
    at = end + 1;
  }
  // The default root-level expansion has an empty parent component.
  if (relative.empty() && text.starts_with("{relative_path}/") &&
      result.starts_with('/')) result.erase(0, 1);
  return result;
}
} // namespace facts::config::detail
