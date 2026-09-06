#include "commands/catalog/MatchedSymbolFormat.h"

#include <format>
#include <llvm/Support/JSON.h>
#include <llvm/Support/raw_ostream.h>

namespace facts::commands {

std::string
renderMatchedSymbolsText(const std::vector<MatchedSymbolCandidate> &values) {
  std::string output =
      "USR\tQUALIFIED NAME\tFILE ID\tKIND\tPATH\tCOMPONENT\tREPOSITORY\n";
  for (const auto &value : values)
    output += std::format("{}\t{}\t{}\t{}\t{}\t{}\t{}\n", value.symbol.usr,
                          value.symbol.qualifiedName, value.symbol.fileId,
                          value.symbol.kind, value.path.string(),
                          value.component, value.repository);
  output += "INDEX SCOPE: matched-only\nSOURCE COMPLETE: unknown\n";
  return output;
}

std::string
renderMatchedSymbolsJson(const std::vector<MatchedSymbolCandidate> &values) {
  llvm::json::Array matches;
  for (const auto &value : values)
    matches.push_back(
        llvm::json::Object{{"usr", value.symbol.usr},
                           {"qualified_name", value.symbol.qualifiedName},
                           {"file_id", value.symbol.fileId},
                           {"kind", value.symbol.kind},
                           {"path", value.path.string()},
                           {"component", value.component},
                           {"repository", value.repository}});
  llvm::json::Object output{
      {"schema_version", 1},
      {"matches", std::move(matches)},
      {"coverage", llvm::json::Object{{"index_scope", "matched-only"},
                                      {"source_complete", nullptr}}}};
  std::string text;
  llvm::raw_string_ostream stream(text);
  stream << llvm::json::Value(std::move(output)) << '\n';
  return text;
}

} // namespace facts::commands
