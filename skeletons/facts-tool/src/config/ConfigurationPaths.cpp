#include "config/Configuration.h"
#include "config/ConfigurationDiscovery.h"
#include "config/ConfigurationPlaceholders.h"

#include <algorithm>

namespace facts::config {
namespace {
bool contained(const std::filesystem::path &root, const std::filesystem::path &target) {
  auto left = root.begin(), right = target.begin();
  for (; left != root.end() && right != target.end(); ++left, ++right)
    if (*left != *right) return false;
  return left == root.end();
}

bool hasDotDot(const std::filesystem::path &value) {
  return std::ranges::any_of(value, [](const auto &part) { return part == ".."; });
}

std::string projectName(const std::filesystem::path &projectRoot) {
  return projectRoot.filename().empty() ? "_root" : projectRoot.filename().string();
}
} // namespace

std::expected<std::filesystem::path, std::string>
renderDatabasePath(const Resolved &value) {
  auto root = value.storageRoot;
  if (root.string().starts_with("~/")) {
    if (detail::env("HOME").empty())
      return std::unexpected(detail::settingError("conf_root", value.storageRootSource,
          "requires HOME for ~/ expansion", value.discovery));
    root = std::filesystem::path(detail::env("HOME")) / root.string().substr(2);
  }
  if (root.empty())
    return std::unexpected(detail::settingError("conf_root", value.storageRootSource,
        "is empty", value.discovery));
  // S-019 guarantee: a relative conf_root anchors to the canonical project
  // root, never to the directory holding the YAML file that declared it.
  if (root.is_relative()) root = value.projectRoot / root;
  auto relative = value.projectRoot.parent_path().lexically_relative("/").generic_string();
  if (relative == ".") relative.clear();
  detail::PlaceholderContext context{.projectRoot = value.projectRoot.generic_string(),
                                     .projectName = projectName(value.projectRoot),
                                     .relativePath = relative,
                                     .filename = projectName(value.projectRoot)};
  return detail::expandPlaceholders(value.templateText, context)
      .transform_error([&](const auto &reason) {
        return detail::settingError("conf_template", value.templateSource, reason, value.discovery);
      })
      .and_then([&](std::string text)
                    -> std::expected<std::filesystem::path, std::string> {
        // The root-level expansion has an empty parent component, so
        // "{relative_path}/{filename}.db" would otherwise render a leading
        // "/" that looks absolute (and would discard root on join).
        if (relative.empty() && value.templateText.starts_with("{relative_path}/") &&
            text.starts_with('/'))
          text.erase(0, 1);
        const auto expanded = std::filesystem::path(text);
        const auto target = (root / expanded).lexically_normal();
        std::error_code error;
        const auto canonicalRoot = std::filesystem::weakly_canonical(root, error);
        if (error) return std::unexpected(detail::settingError("conf_root", value.storageRootSource,
            "cannot resolve: " + error.message(), value.discovery));
        const auto canonicalTarget = std::filesystem::weakly_canonical(target, error);
        if (error) return std::unexpected(detail::settingError("conf_template", value.templateSource,
            "cannot resolve: " + error.message(), value.discovery));
        if (text.empty() || text.back() == '/' || expanded.lexically_normal() == "." ||
            expanded.is_absolute() || hasDotDot(expanded) ||
            canonicalTarget == canonicalRoot || !contained(canonicalRoot, canonicalTarget))
          return std::unexpected("conf_template escapes conf_root: " + target.string());
        return canonicalTarget;
      });
}

// facts_template has no containment invariant (there is no "facts_root"):
// its result may be absolute (typically via {project_root}) or relative, in
// which case it anchors to the project root like conf_root does.
std::expected<std::filesystem::path, std::string>
renderFactsPath(const Resolved &value, const std::vector<std::string> &sources) {
  if (value.factsTemplate.empty())
    return std::unexpected("usage:no facts_template is configured; pass -o/--facts explicitly");
  const bool needsSource = value.factsTemplate.find("{relative_path}") != std::string::npos ||
                           value.factsTemplate.find("{filename}") != std::string::npos;
  detail::PlaceholderContext context{.projectRoot = value.projectRoot.generic_string(),
                                     .projectName = projectName(value.projectRoot)};
  if (needsSource) {
    if (sources.size() != 1)
      return std::unexpected(
          "usage:facts_template needs exactly one source to derive a path; pass -o/--facts explicitly");
    std::error_code error;
    const auto canonicalSource = std::filesystem::weakly_canonical(sources.front(), error);
    if (error)
      return std::unexpected("facts_template cannot resolve source: " + error.message());
    const auto relative = canonicalSource.lexically_relative(value.projectRoot);
    const auto parent = relative.parent_path();
    context.relativePath = (parent.empty() || parent == ".") ? "" : parent.generic_string();
    context.filename = relative.stem().string();
  }
  return detail::expandPlaceholders(value.factsTemplate, context)
      .transform_error([&](const auto &reason) {
        return detail::settingError("facts_template", value.factsTemplateSource, reason,
                                    value.discovery);
      })
      .and_then([&](const std::string &text)
                    -> std::expected<std::filesystem::path, std::string> {
        const auto expanded = std::filesystem::path(text);
        if (text.empty() || text.back() == '/' || expanded.lexically_normal() == "." ||
            hasDotDot(expanded))
          return std::unexpected(detail::settingError("facts_template", value.factsTemplateSource,
              "is invalid", value.discovery));
        const auto target = expanded.is_absolute() ? expanded : value.projectRoot / expanded;
        return target.lexically_normal();
      });
}
} // namespace facts::config
