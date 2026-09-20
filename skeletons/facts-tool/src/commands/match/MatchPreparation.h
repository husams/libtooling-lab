#pragma once
#include "cli/Options.h"
#include "commands/CompilationDatabase.h"
#include "commands/FactPairValidation.h"
#include <optional>

namespace facts { class FileManager; }
namespace facts::commands::match {
struct Preparation {
  std::vector<FileId> selected;
  bool rejectLegacyWrites = false;
  std::optional<FactPairProvenanceSnapshot> pairing;
};
std::expected<Preparation, std::string> prepare(
    const cli::MatchOptions &options,
    const clang::tooling::CompilationDatabase &database, FileManager &files,
    const std::vector<std::string> &sources, const std::string &fingerprint,
    bool discoverIncludes);
}
