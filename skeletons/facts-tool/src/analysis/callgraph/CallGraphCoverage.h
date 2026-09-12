#pragma once

#include "analysis/callgraph/CallGraphQuery.h"

#include <expected>
#include <filesystem>
#include <span>

namespace facts::callgraph {

struct CoverageComponent {
  std::int64_t id = 0;
  std::string name;
  std::string path;
  std::string kind;
};

struct CoverageFile {
  FileId id;
  std::string path;
  std::int64_t componentId = 0;
  std::string componentName;
  std::string componentPath;
  std::string componentKind;
  bool projectLocal = false;
  bool indexed = false;
  std::optional<double> mtime;
  std::string indexedAt;
  std::string factsDb; // '' when NULL
  bool translationUnit = false;
};

struct CoverageReport {
  std::vector<CoverageComponent> components;
  std::vector<CoverageFile> files;
  std::vector<std::string> recoveryCandidates;
};

// Normalizes a facts database path the same way extract() records facts_db,
// except an empty path (no facts database paired at all) stays empty
// instead of resolving to the current working directory.
inline std::string normalizedFactsPathOrEmpty(const std::string &factsPath) {
  return factsPath.empty() ? std::string{}
                           : std::filesystem::absolute(factsPath)
                                 .lexically_normal()
                                 .string();
}

std::expected<CoverageReport, std::string>
loadCoverage(const std::string &path, const std::string &factsPath,
             const QueryGraph &graph);
const CoverageFile *findCoverageFile(const CoverageReport &report, FileId id);
const CoverageFile *findCoverageEvidenceFile(const CoverageReport &report,
                                             const QueryNode &node);
bool isProjectLocal(const CoverageReport &report, const QueryNode &node);
bool hasDefinitionEvidence(const QueryNode &node);
std::string definitionAvailability(const CoverageReport &report,
                                   const QueryNode &node);
std::string extractionCoverage(const CoverageReport &report,
                               const QueryNode &node);
std::string coverageFreshness(const CoverageReport &report,
                              const QueryNode &node);
// Whether an indexed file's recorded mtime no longer exactly matches what
// is on disk now. False for a file that has never been indexed, or whose
// current mtime cannot be read.
bool coverageFileMtimeDrifted(const CoverageFile &file);
std::string coverageAction(const CoverageReport &report, const QueryNode &node);
std::string summarizeCoverage(const CoverageReport &report,
                              const QueryGraph &graph,
                              std::span<const SymbolId> nodes);

} // namespace facts::callgraph
