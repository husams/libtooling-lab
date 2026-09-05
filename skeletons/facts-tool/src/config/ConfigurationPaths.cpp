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

bool invalidBody(const std::string &text) {
  const auto expanded = std::filesystem::path(text);
  return text.empty() || text.back() == '/' || expanded.lexically_normal() == "." ||
        hasDotDot(expanded);
}

std::string projectName(const std::filesystem::path &projectRoot) {
  return projectRoot.filename().empty() ? "_root" : projectRoot.filename().string();
}

// The template's leading "{name}" or "${name}" token, verbatim, or empty if
// the template does not start with one.
std::string_view leadingPlaceholder(std::string_view text) {
  const bool env = text.starts_with("${");
  if (!env && !text.starts_with('{')) return {};
  const auto end = text.find('}', env ? 2 : 1);
  return end == text.npos ? std::string_view{} : text.substr(0, end + 1);
}

// Shared by conf_template and facts_template. A raw literal "/..." is
// always an escape attempt and rejected outright; a raw literal "~/..."
// expands like conf_root's own tilde support and is trusted as-is (no
// escape check, matching conf_root). Otherwise the template is substituted:
// if the result stays relative it joins onto `relativeBase` and must not
// escape it (the historical containment invariant); if a placeholder made
// the result absolute (typically {project_root}), that value is trusted,
// but any literal path segment after it still may not escape it through a
// symlink.
std::expected<std::filesystem::path, std::string>
renderTemplate(const std::string &templateText, const detail::PlaceholderContext &context,
              const char *key, const std::string &source, const std::filesystem::path &relativeBase,
              const char *rootLabel, const std::vector<std::string> &discovery) {
  const auto fail = [&](const std::string &reason) {
    return std::unexpected(detail::settingError(key, source, reason, discovery));
  };
  if (templateText.starts_with('/'))
    return fail("must not be an absolute literal; use ~/, a placeholder, or a relative path");
  if (templateText.starts_with("~/")) {
    if (detail::env("HOME").empty()) return fail("requires HOME for ~/ expansion");
    auto suffix = detail::expandPlaceholders(templateText.substr(2), context);
    if (!suffix) return fail(suffix.error());
    if (invalidBody(*suffix)) return fail("is invalid");
    return (std::filesystem::path(detail::env("HOME")) / *suffix).lexically_normal();
  }
  auto substituted = detail::expandPlaceholders(templateText, context);
  if (!substituted) return fail(substituted.error());
  auto text = std::move(*substituted);
  // An empty {relative_path} immediately followed by a literal "/" (the
  // default conf_template's root-level case) would otherwise render a
  // leading "/" that looks like an absolute-literal escape attempt.
  if (context.relativePath.empty() && leadingPlaceholder(templateText) == "{relative_path}" &&
      templateText.substr(std::string_view("{relative_path}").size()).starts_with('/') &&
      text.starts_with('/'))
    text.erase(0, 1);
  if (invalidBody(text)) return fail("is invalid");
  const auto expanded = std::filesystem::path(text);
  std::error_code error;
  if (!expanded.is_absolute()) {
    const auto target = (relativeBase / expanded).lexically_normal();
    const auto canonicalRoot = std::filesystem::weakly_canonical(relativeBase, error);
    if (error) return fail("cannot resolve: " + error.message());
    const auto canonicalTarget = std::filesystem::weakly_canonical(target, error);
    if (error) return fail("cannot resolve: " + error.message());
    if (canonicalTarget == canonicalRoot || !contained(canonicalRoot, canonicalTarget))
      return std::unexpected(std::string(key) + " escapes " + rootLabel + ": " + target.string());
    return canonicalTarget;
  }
  const auto leading = leadingPlaceholder(templateText);
  if (leading.empty()) return expanded.lexically_normal();
  auto rootText = detail::expandPlaceholders(leading, context);
  if (!rootText) return fail(rootText.error());
  const auto canonicalRoot = std::filesystem::weakly_canonical(*rootText, error);
  if (error) return fail("cannot resolve: " + error.message());
  const auto canonicalTarget = std::filesystem::weakly_canonical(expanded, error);
  if (error) return fail("cannot resolve: " + error.message());
  if (!contained(canonicalRoot, canonicalTarget))
    return std::unexpected(std::string(key) + " escapes " + *rootText + " through a symlink: " +
                           expanded.string());
  return canonicalTarget;
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
  return renderTemplate(value.templateText, context, "conf_template", value.templateSource, root,
                        "conf_root", value.discovery);
}

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
  return renderTemplate(value.factsTemplate, context, "facts_template", value.factsTemplateSource,
                        value.projectRoot, "the project root", value.discovery);
}
} // namespace facts::config
