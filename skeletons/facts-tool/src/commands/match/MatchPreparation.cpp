#include "commands/match/MatchPreparation.h"
#include "commands/ExtractionSetup.h"
#include "storage/FileManager.h"
#include <filesystem>

namespace facts::commands::match {
namespace {
std::expected<std::vector<FileId>, std::string> selectedFiles(
    const cli::MatchOptions &options,
    const clang::tooling::CompilationDatabase &database, FileManager &files,
    const std::vector<std::string> &sources, const std::string &fingerprint,
    bool discoverIncludes) {
  auto paths = sources;
  if (discoverIncludes) {
    auto discovered = requireRegisteredSources(files, database, sources,
                                               fingerprint, options.astCache);
    if (!discovered) return std::unexpected(discovered.error());
    paths = std::move(discovered->merged);
  }
  std::vector<FileId> selected;
  selected.reserve(paths.size());
  for (const auto &source : paths) {
    auto id = files.getId(source);
    if (!id)
      return std::unexpected("cannot resolve match source: " + id.error().message());
    selected.push_back(*id);
  }
  return selected;
}
}
std::expected<Preparation, std::string> prepare(
    const cli::MatchOptions &options,
    const clang::tooling::CompilationDatabase &database, FileManager &files,
    const std::vector<std::string> &sources, const std::string &fingerprint,
    bool discoverIncludes) {
  return selectedFiles(options, database, files, sources, fingerprint, discoverIncludes)
      .and_then([&](std::vector<FileId> selected)
                    -> std::expected<Preparation, std::string> {
        Preparation result{std::move(selected), false, std::nullopt};
        if (options.factsProvided && options.facts == options.configuration)
          return result;
        if (std::filesystem::exists(options.facts)) {
          auto legacy = legacyFactsNeedRegistration(options.facts, options.configuration);
          if (!legacy) return std::unexpected(legacy.error());
          result.rejectLegacyWrites = *legacy;
        }
        if (!result.rejectLegacyWrites) {
          auto pairing = prepareFactPairForWrite(options.facts, options.configuration);
          if (!pairing) return std::unexpected(pairing.error());
          result.pairing = std::move(*pairing);
        }
        return result;
      });
}
}
