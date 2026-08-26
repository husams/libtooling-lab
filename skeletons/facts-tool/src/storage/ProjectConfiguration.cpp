#include "storage/ProjectConfiguration.h"

#include <algorithm>
#include <set>
#include <system_error>

namespace facts {
namespace {

std::filesystem::path absolutePath(std::filesystem::path path) {
  auto absolute =
      (path.is_absolute() ? std::move(path) : std::filesystem::absolute(path))
          .lexically_normal();
  std::error_code error;
  auto canonical = std::filesystem::weakly_canonical(absolute, error);
  return error ? absolute : canonical;
}

std::string ownershipKey(const std::filesystem::path &path) {
  auto key = path.string();
  while (!key.empty() && key.back() == '/') {
    key.pop_back();
  }
  return key;
}

bool ownsPath(const std::filesystem::path &root,
              const std::filesystem::path &source) {
  const auto key = ownershipKey(root);
  const auto identity = source.string();
  return identity == key || identity.starts_with(key + "/");
}

} // namespace

std::filesystem::path
effectiveComponentRoot(const ProjectComponent &component,
                       const std::optional<ProjectClone> &activeClone) {
  const auto componentPath = std::filesystem::path(component.path);
  const auto version = component.version.value_or("");
  const auto effective =
      version.empty() ? componentPath : componentPath / version;
  const auto cloneAnchored = component.repositoryId.has_value() &&
                             componentPath.is_relative() && activeClone &&
                             !activeClone->path.empty() &&
                             !componentPath.string().contains('<') &&
                             !componentPath.string().contains('$');
  return absolutePath(cloneAnchored
                          ? std::filesystem::path(activeClone->path) / effective
                          : effective);
}

std::filesystem::path
fullProjectFilePath(const ProjectComponent &component,
                    const std::optional<ProjectClone> &clone,
                    std::string_view directory, std::string_view name) {
  return (effectiveComponentRoot(component, clone) /
          std::filesystem::path(directory) / std::filesystem::path(name))
      .lexically_normal();
}

std::optional<std::size_t>
selectOwningComponent(std::span<const ProjectComponent> components,
                      const ProjectClone &activeClone,
                      const std::filesystem::path &source) {
  // An absolute source is already the identity the caller chose. Resolving it
  // again would undo the mapping that keeps a symlinked in-project source
  // inside its component.
  const auto identity =
      source.is_absolute() ? source.lexically_normal() : absolutePath(source);
  std::optional<std::size_t> owner;
  for (std::size_t index = 0; index < components.size(); ++index) {
    const auto root = effectiveComponentRoot(components[index], activeClone);
    if (ownsPath(root, identity) &&
        (!owner || ownershipKey(root).size() >
                       ownershipKey(effectiveComponentRoot(components[*owner],
                                                           activeClone))
                           .size())) {
      owner = index;
    }
  }
  return owner;
}

std::expected<void, std::string>
validateProjectConfiguration(const ProjectConfiguration &configuration) {
  const auto &clone = configuration.activeClone;
  if (clone.path.empty()) {
    return std::unexpected("active clone path is empty");
  }
  if (configuration.repositoryName.empty()) {
    return std::unexpected(
        "repository name is empty: the project root '" + clone.path +
        "' has no directory name to derive one from; pass an explicit "
        "repository name or import a compilation database whose commands "
        "share a project root");
  }
  if (configuration.components.empty()) {
    return std::unexpected("project has no components");
  }
  std::set<std::string> paths;
  for (const auto &component : configuration.components) {
    if (component.path.empty()) {
      return std::unexpected("component '" + component.name +
                             "' has an empty path");
    }
    if (!paths.insert(component.path).second) {
      return std::unexpected("component path '" + component.path +
                             "' is configured twice");
    }
  }
  for (const auto &file : configuration.files) {
    if (file.name.empty()) {
      return std::unexpected("file in component '" + file.componentPath +
                             "' has an empty name");
    }
    if (!paths.contains(file.componentPath)) {
      return std::unexpected("file '" + file.name +
                             "' names an unconfigured component '" +
                             file.componentPath + "'");
    }
    if (file.compileOptions.empty()) {
      return std::unexpected("file '" + file.componentPath + "/" + file.name +
                             "' has no compile options");
    }
  }
  return {};
}

} // namespace facts
