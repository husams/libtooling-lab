#include "apis/domain/Candidates.h"

namespace facts::apis::domain {
namespace {
Result<detail::Candidate> select(std::vector<detail::Candidate> candidates,
                                 const FileSelector &selector) {
  std::optional<detail::Candidate> selected;
  for (auto &candidate : candidates) {
    const auto accepted = detail::matches(candidate, selector);
    if (!accepted) return std::unexpected(accepted.error());
    if (!*accepted) continue;
    if (selected && selected->file.id == candidate.file.id) {
      if (candidate.active) selected = std::move(candidate);
      continue;
    }
    if (selected)
      return std::unexpected(Error{409, "ambiguous_file",
          "file matches multiple registered identities; specify repo, clone or component"});
    selected = std::move(candidate);
  }
  if (!selected)
    return std::unexpected(Error{404, "file_not_found",
        "file is not registered for the supplied repo, clone and component selectors"});
  return std::move(*selected);
}
Result<ResolvedFile> resolve(const Context &context,
                             const detail::Candidate &candidate) {
  const auto &file = candidate.file;
  return catalog::filePath(file)
      .transform_error([](const std::string &message) {
        return Error{500, "file_configuration", message};
      })
      .and_then([&](const std::filesystem::path &path) -> Result<ResolvedFile> {
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error))
          return std::unexpected(Error{404, "file_unavailable",
                                       "registered source file is unavailable: " + path.string(),
              {{"path", path.string()}, {"project_root", file.clone ? file.clone->path : context.configuration.projectRoot.string()},
               {"stage", "resolve source"}, {"expected", "A readable source file in the selected clone"},
               {"action", "Restore the source file or select the correct active clone, then retry"}}});
        return detail::factsPath(context, candidate, path).transform(
            [&](std::filesystem::path facts) {
              return ResolvedFile{file.id, path, std::move(facts),
                  candidate.repository, file.component.name, file.clone,
                  candidate.active};
            });
      });
}
}
Result<ResolvedFile> resolveFile(const Context &context,
                                 const FileSelector &selector) {
  return detail::validateSelector(selector)
      .and_then([&] { return detail::candidates(context, selector); })
      .and_then([&](auto candidates) { return select(std::move(candidates), selector); })
      .and_then([&](const auto &candidate) { return resolve(context, candidate); });
}
} // namespace facts::apis::domain
