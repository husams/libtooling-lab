#include "config/Configuration.h"
#include "config/ConfigurationDiscovery.h"
#include "config/ConfigurationTemplate.h"
#include <algorithm>

namespace facts::config {
namespace {
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
  auto root = value.storageRoot;
  if (root.string().starts_with("~/")) {
    if (detail::env("HOME").empty())
      return std::unexpected("conf_root requires HOME for ~/ expansion");
    root = std::filesystem::path(detail::env("HOME")) / root.string().substr(2);
  }
  if (root.empty()) return std::unexpected("conf_root is empty");
  if (root.is_relative()) root = std::filesystem::path(value.source).parent_path() / root;
  auto relative = value.projectRoot.parent_path().lexically_relative("/").generic_string();
  if (relative == ".") relative.clear();
  const auto filename = value.projectRoot.filename().empty()
                            ? "_root" : value.projectRoot.filename().string();
  return detail::substitute(value.templateText, relative, filename)
      .and_then([&](const std::string &text)
                    -> std::expected<std::filesystem::path, std::string> {
        const auto expanded = std::filesystem::path(text);
        const auto target = (root / expanded).lexically_normal();
        std::error_code error;
        const auto canonicalRoot = std::filesystem::weakly_canonical(root, error);
        if (error) return std::unexpected("conf_root cannot resolve: " + error.message());
        const auto canonicalTarget = std::filesystem::weakly_canonical(target, error);
        if (error) return std::unexpected("conf_template cannot resolve: " + error.message());
        if (expanded.lexically_normal() == "." || expanded.is_absolute() || std::ranges::any_of(expanded,
            [](const auto &part) { return part == ".."; }) ||
            canonicalTarget == canonicalRoot || !contained(canonicalRoot, canonicalTarget))
          return std::unexpected("conf_template escapes conf_root: " + target.string());
        return canonicalTarget;
      });
}
} // namespace facts::config
