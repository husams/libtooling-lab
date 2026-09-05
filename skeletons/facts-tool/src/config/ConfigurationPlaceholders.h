#pragma once
#include <expected>
#include <pwd.h>
#include <string>
#include <string_view>
#include <unistd.h>

namespace facts::config::detail {

// {project_root}/{project_name}/{relative_path}/{filename}/{user} plus
// ${ENV_NAME}. Callers fill in the fields that apply to their template kind
// (conf_template vs facts_template assign different relative_path/filename
// meanings) and leave the rest empty.
struct PlaceholderContext {
  std::string projectRoot;
  std::string projectName;
  std::string relativePath;
  std::string filename;
};

inline bool invalidCharacters(std::string_view text) {
  return text.find('\0') != text.npos || text.find('\n') != text.npos ||
         text.find('\\') != text.npos;
}

inline std::string loginUser() {
  if (const auto *user = std::getenv("USER"); user && *user) return user;
  if (const auto *user = std::getenv("LOGNAME"); user && *user) return user;
  if (const auto *entry = getpwuid(geteuid())) return entry->pw_name;
  return {};
}

inline std::expected<std::string, std::string>
environmentValue(std::string_view name) {
  if (name.empty()) return std::unexpected("template has an empty ${} placeholder");
  const auto value = std::getenv(std::string(name).c_str());
  if (!value) return std::unexpected("template references unset environment variable: " + std::string(name));
  if (invalidCharacters(value))
    return std::unexpected("template environment variable has invalid characters: " + std::string(name));
  return std::string(value);
}

inline std::expected<std::string, std::string>
placeholderValue(std::string_view key, const PlaceholderContext &context) {
  if (key == "project_root") return context.projectRoot;
  if (key == "project_name") return context.projectName;
  if (key == "relative_path") return context.relativePath;
  if (key == "filename") return context.filename;
  if (key == "user") {
    const auto user = loginUser();
    if (user.empty()) return std::unexpected("template placeholder {user} could not be resolved");
    return user;
  }
  return std::unexpected("template has unknown placeholder: " + std::string(key));
}

// Single left-to-right pass: "${NAME}" expands an environment variable,
// "{name}" expands a known placeholder, a bare "$" without "{" is literal,
// and everything else is copied through unchanged. Never re-interpreted.
inline std::expected<std::string, std::string>
expandPlaceholders(std::string_view text, const PlaceholderContext &context) {
  if (invalidCharacters(text) || invalidCharacters(context.relativePath) ||
      invalidCharacters(context.filename))
    return std::unexpected("template must not contain NUL, newline, or backslash");
  std::string result;
  for (std::size_t at = 0; at < text.size();) {
    if (text[at] == '}') return std::unexpected("template has unmatched braces");
    const bool env = text[at] == '$' && at + 1 < text.size() && text[at + 1] == '{';
    if (text[at] != '{' && !env) { result += text[at++]; continue; }
    const auto start = env ? at + 2 : at + 1;
    const auto end = text.find('}', start);
    if (end == text.npos) return std::unexpected("template has unmatched braces");
    const auto key = text.substr(start, end - start);
    auto value = env ? environmentValue(key) : placeholderValue(key, context);
    if (!value) return std::unexpected(value.error());
    result += *value;
    at = end + 1;
  }
  return result;
}

} // namespace facts::config::detail
