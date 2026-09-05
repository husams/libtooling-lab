#include "config/Configuration.h"
#include "config/ConfigurationDiscovery.h"
#include "config/ConfigurationPlaceholders.h"
#include "config/ConfigurationShape.h"

#include <algorithm>

namespace facts::config {
namespace {
bool contained(const std::filesystem::path &root, const std::filesystem::path &target) {
  auto left = root.begin(), right = target.begin();
  for (; left != root.end() && right != target.end(); ++left, ++right)
    if (*left != *right) return false;
  return left == root.end();
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

// The literal text before the first placeholder of an absolute template: the
// part the user wrote verbatim and therefore trusts.
std::filesystem::path literalPrefix(std::string_view text) {
  const auto stop = std::min(text.find('{'), text.find("${"));
  return std::filesystem::path(std::string(text.substr(0, stop))).parent_path();
}

// Shared by conf_template and facts_template. Every rendered path is checked
// canonically against one anchor and may not escape it, even through a
// symlink: "~/" anchors to HOME, a leading placeholder ({project_root},
// ${ENV}) to its own value, an absolute literal to its literal prefix before
// the first placeholder, and a relative template to `relativeBase`.
std::expected<std::filesystem::path, std::string>
renderTemplate(const std::string &templateText, const detail::PlaceholderContext &context,
              const char *key, const std::string &source, const std::filesystem::path &relativeBase,
              const char *rootLabel, const std::vector<std::string> &discovery) {
  const auto fail = [&](const std::string &reason) {
    return std::unexpected(detail::settingError(key, source, reason, discovery));
  };
  std::string text = templateText;
  std::filesystem::path anchor = relativeBase;
  std::string anchorLabel = rootLabel;
  if (text.starts_with("~/")) {
    if (detail::env("HOME").empty()) return fail("requires HOME for ~/ expansion");
    anchor = detail::env("HOME");
    anchorLabel = "HOME";
    text = anchor.string() + text.substr(1);
  } else if (text.starts_with('/')) {
    anchor = literalPrefix(text);
    anchorLabel = "its literal prefix " + anchor.string();
  } else if (const auto leading = leadingPlaceholder(text); !leading.empty()) {
    auto rootText = detail::expandPlaceholders(leading, context);
    if (!rootText) return fail(rootText.error());
    if (std::filesystem::path(*rootText).is_absolute()) {
      anchor = *rootText;
      anchorLabel = std::string(leading) + " (" + *rootText + ")";
    }
  }
  auto substituted = detail::expandPlaceholders(text, context);
  if (!substituted) return fail(substituted.error());
  text = std::move(*substituted);
  // An empty {relative_path} immediately followed by a literal "/" (the
  // default conf_template's root-level case) would otherwise render a
  // leading "/" that looks like an absolute path.
  if (context.relativePath.empty() && leadingPlaceholder(templateText) == "{relative_path}" &&
      templateText.substr(std::string_view("{relative_path}").size()).starts_with('/') &&
      text.starts_with('/'))
    text.erase(0, 1);
  if (detail::invalidPathShape(text)) return fail("is invalid");
  const auto expanded = std::filesystem::path(text);
  const auto target = (expanded.is_absolute() ? expanded : relativeBase / expanded).lexically_normal();
  std::error_code error;
  const auto canonicalRoot = std::filesystem::weakly_canonical(anchor, error);
  if (error) return fail("cannot resolve: " + error.message());
  const auto canonicalTarget = std::filesystem::weakly_canonical(target, error);
  if (error) return fail("cannot resolve: " + error.message());
  if (canonicalTarget == canonicalRoot || !contained(canonicalRoot, canonicalTarget))
    return std::unexpected(std::string(key) + " escapes " + anchorLabel + ": " + target.string());
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
