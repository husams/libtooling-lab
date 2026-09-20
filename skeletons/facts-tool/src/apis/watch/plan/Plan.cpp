#include "apis/watch/plan/Details.h"

namespace facts::apis::watch {
namespace {
std::expected<void, std::string> clonePlan(Plan &result, const Settings &settings,
    const Catalog &catalog, const Clone &clone, const plan::Options &imports,
    const plan::Options &extracts, const std::vector<plan::Compilation> &compilations,
    const std::vector<std::string> &stored) {
  return Ignore::create(clone.path, settings).and_then([&](Ignore ignore)
      -> std::expected<void, std::string> {
    auto selected = plan::selectSources(stored, catalog, clone, ignore, settings);
    if (!selected) return std::unexpected(selected.error());
    std::set<std::string> sources(selected->begin(), selected->end());
    for (const auto &compilation : compilations) {
      auto files = plan::selectSources(compilation.sources, catalog, clone, ignore, settings);
      if (!files) return std::unexpected(files.error());
      auto arguments = plan::arguments(settings, catalog, imports, true);
      arguments.insert(arguments.end(), {"--existing-clone", std::to_string(clone.cloneId),
                                         "-p", compilation.directory.string()});
      auto added = plan::appendBatches(result.imports, std::move(arguments),
                                       std::set(files->begin(), files->end()));
      if (!added) return std::unexpected(added.error());
      sources.insert(files->begin(), files->end());
    }
    return plan::appendBatches(result.extracts,
        plan::arguments(settings, catalog, extracts, false), sources);
  });
}
}

std::expected<Plan, std::string> buildPlan(
    const Settings &settings, const Catalog &catalog,
    const std::vector<std::filesystem::path> &compilationDirectories) {
  auto imports = plan::options(settings, true);
  if (!imports) return std::unexpected(imports.error());
  auto extracts = plan::options(settings, false);
  if (!extracts) return std::unexpected(extracts.error());
  auto compilations = plan::compilations(imports->databases.empty()
      ? compilationDirectories : imports->databases);
  if (!compilations) return std::unexpected(compilations.error());
  return plan::storedSources(catalog).and_then([&](auto stored)
      -> std::expected<Plan, std::string> {
    Plan result;
    for (const auto &clone : catalog.clones) {
      if (!clone.active || !clone.excluded.empty()) continue;
      std::error_code error;
      if (!std::filesystem::is_directory(clone.path, error)) continue;
      auto added = clonePlan(result, settings, catalog, clone, *imports, *extracts,
                             *compilations, stored);
      if (!added) return std::unexpected(added.error());
    }
    return result;
  });
}
}
