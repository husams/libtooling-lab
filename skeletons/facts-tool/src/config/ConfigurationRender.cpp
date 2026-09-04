#include "config/Configuration.h"
#include <algorithm>
#include <cstdlib>
#include <system_error>

namespace facts::config {
namespace {
std::string home() { const auto *v = std::getenv("HOME"); return v ? v : ""; }
std::filesystem::path expand(std::string value) {
  return value.starts_with("~/") ? std::filesystem::path(home()) / value.substr(2) : std::filesystem::path(value);
}
bool contained(const std::filesystem::path &root,
               const std::filesystem::path &target) {
  auto left = root.begin(), right = target.begin();
  for (; left != root.end() && right != target.end(); ++left, ++right)
    if (*left != *right) return false;
  return left == root.end();
}
}
std::expected<std::filesystem::path, std::string>
renderDatabasePath(const Resolved &value) {
  auto root = expand(value.storageRoot.string());
  if (value.storageRoot.string().starts_with("~/") && home().empty())
    return std::unexpected("HOME is required to expand conf_root");
  if (root.empty()) return std::unexpected("conf_root is empty");
  if (root.is_relative()) root = std::filesystem::path(value.source).parent_path() / root;
  auto text = value.templateText;
  if (text.empty() || text.back() == '/' ||
      text.find_first_of("\\\0\n") != std::string::npos)
    return std::unexpected("invalid conf_template");
  const auto relative = value.projectRoot.parent_path().lexically_relative("/").generic_string();
  const auto filename = value.projectRoot.filename().empty() ? "_root" : value.projectRoot.filename().string();
  for (const auto key : {std::string_view{"relative_path"}, std::string_view{"filename"}}) {
    const auto marker = "{" + std::string(key) + "}";
    for (auto position = text.find(marker); position != std::string::npos;
         position = text.find(marker, position + 1))
      text.replace(position, marker.size(), key == "relative_path" ? relative : filename);
  }
  if (value.projectRoot.filename().string().find_first_of("\\\0\n") != std::string::npos)
    return std::unexpected("invalid project path");
  if (text.find('{') != std::string::npos || text.find('}') != std::string::npos)
    return std::unexpected("invalid conf_template");
  const auto target = (root / std::filesystem::path(text)).lexically_normal();
  std::error_code error;
  auto canonicalRoot = std::filesystem::weakly_canonical(root, error);
  if (error) return std::unexpected("cannot resolve conf_root: " + error.message());
  error.clear();
  auto canonicalTarget = std::filesystem::weakly_canonical(target, error);
  if (error) return std::unexpected("cannot resolve generated conf path: " + error.message());
  if (std::filesystem::path(text).is_absolute() ||
      std::ranges::any_of(std::filesystem::path(text), [](const auto &part) {
        return part == "..";
      }) || canonicalTarget == canonicalRoot || !contained(canonicalRoot, canonicalTarget))
    return std::unexpected("conf_template escapes conf_root: " + target.string());
  return target;
}
}
