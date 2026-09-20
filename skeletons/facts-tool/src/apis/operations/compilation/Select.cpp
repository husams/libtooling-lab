#include "apis/operations/compilation/Candidates.h"
#include "commands/CompilationDatabase.h"
#include "commands/IncludedFiles.h"
#include "tooling/CompilationCommandCodec.h"
#include "tooling/StoredCompilationDatabase.h"
#include <algorithm>

namespace facts::apis::operations::compilation {
namespace {
domain::Result<bool> includes(const domain::Context &context,
    const domain::ResolvedFile &file, const Group &group) {
  std::optional<std::string> failure;
  for (const auto &source : group.sources) {
    auto database = commands::appendExtraArguments(
        makeStoredCompilationDatabase({source.command}), context.configuration.extraArguments);
    auto cache = context.configuration.astCache;
    cache.verbosity = 0;
    const std::vector<std::string> selected{source.command.Filename};
    auto paths = commands::discoverIncludedFiles(*database, selected, cache);
    if (!paths) {
      if (!failure) failure = paths.error();
      continue;
    }
    if (std::ranges::any_of(*paths, [&](const auto &path) {
          return normalizeCompilationPath(path) == normalizeCompilationPath(file.path);
        })) return true;
  }
  if (failure)
    return std::unexpected(domain::Error{422, "compilation_context_unavailable",
        "cannot verify header compilation context: " + *failure});
  return false;
}
}
domain::Result<clang::tooling::CompileCommand> selectContext(
    const domain::Context &context, const domain::ResolvedFile &file,
    std::vector<Group> groups) {
  std::optional<clang::tooling::CompileCommand> selected;
  for (auto &group : groups) {
    auto matched = includes(context, file, group);
    if (!matched) return std::unexpected(matched.error());
    if (!*matched) continue;
    if (selected)
      return std::unexpected(domain::Error{409, "ambiguous_compilation_context",
          "registered translation units include this file with conflicting compilation settings"});
    selected = std::move(group.transferred);
  }
  if (!selected)
    return std::unexpected(domain::Error{422, "compilation_context_unavailable",
        "no registered translation unit includes the requested file"});
  return std::move(*selected);
}
}
