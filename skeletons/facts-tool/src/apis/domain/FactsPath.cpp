#include "apis/domain/Candidates.h"

namespace facts::apis::domain::detail {
Result<std::filesystem::path> factsPath(const Context &context,
                                       const Candidate &candidate,
                                       const std::filesystem::path &source) {
  const auto &file = candidate.file;
  const auto root = file.clone ? std::filesystem::path(file.clone->path)
      : effectiveComponentRoot(file.component, file.clone);
  if (!file.factsDb.empty()) {
    const std::filesystem::path saved(file.factsDb);
    return saved.is_absolute() ? saved
                              : root / saved;
  }
  return workspaceContext(context, root).and_then([&](const Context &workspace) -> Result<std::filesystem::path> {
  auto configuration = workspace.configuration;
  if (configuration.factsTemplate.empty())
    return configuration.database.parent_path() / "facts" /
           (std::to_string(file.id) + ".db");
  configuration.projectRoot = file.clone
      ? std::filesystem::path(file.clone->path)
      : effectiveComponentRoot(file.component, file.clone);
  return config::renderFactsPath(configuration, {source.string()})
      .transform_error([](const std::string &message) {
        return Error{500, "facts_configuration", message};
      });
  });
}
} // namespace facts::apis::domain::detail
