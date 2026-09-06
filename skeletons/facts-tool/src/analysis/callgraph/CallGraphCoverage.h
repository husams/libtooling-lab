#pragma once

#include "analysis/callgraph/CallGraphQuery.h"

#include <expected>
#include <span>

namespace facts::callgraph {

struct CoverageFile {
  FileId id;
  std::string path;
  bool projectLocal = false;
  bool indexed = false;
  std::optional<double> mtime;
  std::string indexedAt;
  bool translationUnit = false;
};

struct CoverageReport {
  std::vector<CoverageFile> files;
  std::vector<std::string> recoveryCandidates;
};

std::expected<CoverageReport, std::string>
loadCoverage(const std::string &path, const QueryGraph &graph);
const CoverageFile *findCoverageFile(const CoverageReport &report, FileId id);
const CoverageFile *findCoverageEvidenceFile(const CoverageReport &report,
                                             const QueryNode &node);
bool isProjectLocal(const CoverageReport &report, const QueryNode &node);
std::string definitionAvailability(const CoverageReport &report,
                                   const QueryNode &node);
std::string extractionCoverage(const CoverageReport &report,
                               const QueryNode &node);
std::string coverageFreshness(const CoverageReport &report,
                              const QueryNode &node);
std::string coverageAction(const CoverageReport &report, const QueryNode &node);
std::string summarizeCoverage(const CoverageReport &report,
                              const QueryGraph &graph,
                              std::span<const SymbolId> nodes);

} // namespace facts::callgraph
