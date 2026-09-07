#ifndef FACTS_TOOL_MODEL_EXPRESSION_EVIDENCE_H
#define FACTS_TOOL_MODEL_EXPRESSION_EVIDENCE_H

#include "model/SymbolId.h"

#include <optional>
#include <string>

namespace facts {

struct ExpressionOccurrence {
  std::string identity;
  std::optional<SymbolId> owner;
  std::optional<SymbolId> target;
  FileId file = 0;
  unsigned line = 0;
  unsigned column = 0;
  unsigned offset = 0;
  unsigned size = 0;
  std::string sourceSha256;
  std::string expressionKind;
  std::string access;
  std::string freshness = "current";
  std::optional<std::string> unavailableReason;
};

struct SourceRegion {
  std::string identity;
  SymbolId symbol;
  FileId file = 0;
  unsigned line = 0;
  unsigned column = 0;
  unsigned offset = 0;
  unsigned size = 0;
  std::string sourceSha256;
  std::string symbolKind;
  std::string freshness = "current";
  std::optional<std::string> unavailableReason;
};

} // namespace facts

#endif // FACTS_TOOL_MODEL_EXPRESSION_EVIDENCE_H
