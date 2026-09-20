#include "apis/domain/Candidates.h"
#include "config/ConfigurationShape.h"

namespace facts::apis::domain::detail {
Result<void> validateSelector(const FileSelector &selector) {
  const auto invalid = [](const std::string &value) {
    return value.empty() || value.find('\0') != std::string::npos;
  };
  if (invalid(selector.path) || (selector.repo && invalid(*selector.repo)) ||
      (selector.clone && invalid(*selector.clone)) ||
      (selector.component && invalid(*selector.component)))
    return std::unexpected(Error{422, "invalid_file_selector",
                                 "file selectors must be nonempty strings without NUL"});
  if (config::detail::hasDotDot(selector.path) ||
      std::filesystem::path(selector.path).filename().empty())
    return std::unexpected(Error{422, "invalid_file_selector",
                                 "file path must name a file without parent traversal"});
  return {};
}
Result<bool> matches(const Candidate &candidate, const FileSelector &selector) {
  const auto &file = candidate.file;
  const std::filesystem::path requested(selector.path);
  if (requested.is_relative() && !selector.clone && !candidate.active) return false;
  if (file.component.repositoryId &&
      std::filesystem::path(file.component.path).is_relative() && !file.clone)
    return false;
  try {
    const auto root = effectiveComponentRoot(file.component, file.clone);
    const auto base = !selector.component && file.clone
                          ? std::filesystem::path(file.clone->path) : root;
    const auto path = requested.is_absolute() ? requested : base / requested;
    const auto registered = fullProjectFilePath(file.component, file.clone,
                                                file.directory, file.name);
    return std::filesystem::weakly_canonical(path) ==
           std::filesystem::weakly_canonical(registered);
  } catch (const std::filesystem::filesystem_error &error) {
    return std::unexpected(Error{422, "invalid_file_path", error.what()});
  }
}
} // namespace facts::apis::domain::detail
