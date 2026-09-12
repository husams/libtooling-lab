#include "analysis/callgraph/CallGraphCoverage.h"

#include "storage/catalog/Component.h"
#include "storage/catalog/File.h"

#include <ranges>
#include <set>

namespace facts::callgraph {
namespace {
bool knownFile(const CoverageReport &report, FileId id) {
  return id == builtinFileId || findCoverageFile(report, id);
}

std::set<FileId> referencedFiles(const QueryGraph &graph) {
  std::set<FileId> result;
  for (const auto &node : graph.nodes) {
    result.insert(node.id.file);
    if (node.definitionLocation)
      result.insert(node.definitionLocation->file);
  }
  for (const auto &edge : graph.edges)
    result.insert(edge.file);
  return result;
}
} // namespace

const CoverageFile *findCoverageFile(const CoverageReport &report, FileId id) {
  const auto found = std::ranges::find(report.files, id, &CoverageFile::id);
  return found == report.files.end() ? nullptr : &*found;
}

std::expected<CoverageReport, std::string>
loadCoverage(const std::string &path, const std::string &factsPath,
             const QueryGraph &graph) {
  const auto normalizedFactsPath = normalizedFactsPathOrEmpty(factsPath);
  return catalog::open(path, false).and_then([&](auto database) {
    return catalog::components(database).and_then([&](const auto &components) {
      return catalog::files(database).and_then(
          [&](const auto &files) -> std::expected<CoverageReport, std::string> {
            CoverageReport report;
            for (const auto &component : components)
              report.components.push_back(
                  {component.value.id, component.value.name,
                   component.value.path, component.value.kind});
            const auto factsFiles = referencedFiles(graph);
            for (const auto &file : files) {
              auto resolved = catalog::filePath(file);
              if (!resolved)
                return std::unexpected(resolved.error());
              report.files.push_back(
                  {static_cast<FileId>(file.id), resolved->string(),
                   file.component.id, file.componentName, file.component.path,
                   file.component.kind, file.component.repositoryId.has_value(),
                   file.indexed, file.mtime, file.indexedAt, file.factsDb,
                   !file.driver.empty()});
            }
            const auto missingNode =
                std::ranges::find_if(graph.nodes, [&](const auto &node) {
                  return !knownFile(report, node.id.file);
                });
            const auto missingEdge =
                std::ranges::find_if(graph.edges, [&](const auto &edge) {
                  return !knownFile(report, edge.file);
                });
            const auto missingDefinition =
                std::ranges::find_if(graph.nodes, [&](const auto &node) {
                  return node.definitionLocation &&
                         !knownFile(report, node.definitionLocation->file);
                });
            if (missingNode != graph.nodes.end() ||
                missingEdge != graph.edges.end() ||
                missingDefinition != graph.nodes.end())
              return std::unexpected(
                  "project/facts pair has unmatched file identities");
            // A file recovery has never touched is always a candidate; one
            // extract already indexed is a candidate again once its source
            // has drifted from what was last recorded (so recovery can
            // still pick up a body that changed after a real extract), or
            // once its recorded facts database is not this one (so a file
            // extracted only into some other facts database is still a
            // candidate here, even though it is genuinely indexed there).
            for (const auto &file : report.files)
              if (file.projectLocal && file.translationUnit &&
                  !factsFiles.contains(file.id) &&
                  (!file.indexed || coverageFileMtimeDrifted(file) ||
                   file.factsDb != normalizedFactsPath))
                report.recoveryCandidates.push_back(file.path);
            return report;
          });
    });
  });
}
} // namespace facts::callgraph
