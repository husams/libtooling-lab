#pragma once
#include <algorithm>
#include <filesystem>
#include <string>

namespace facts::config::detail {

inline bool hasDotDot(const std::filesystem::path &value) {
  return std::ranges::any_of(value, [](const auto &part) { return part == ".."; });
}

// Shape rules shared by the per-tier parse check (raw template text) and the
// renderer (substituted text): a template may not be empty, end with a
// separator, normalize to "." or contain any ".." component.
inline bool invalidPathShape(const std::string &text) {
  const auto value = std::filesystem::path(text);
  return text.empty() || text.back() == '/' || value.lexically_normal() == "." ||
         hasDotDot(value);
}

} // namespace facts::config::detail
