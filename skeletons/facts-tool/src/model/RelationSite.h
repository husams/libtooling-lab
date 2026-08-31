#ifndef FACTS_TOOL_MODEL_RELATION_SITE_H
#define FACTS_TOOL_MODEL_RELATION_SITE_H

#include "model/Location.h"
#include "model/ReceiverCertainty.h"
#include "model/Relation.h"

#include <cstdint>
#include <optional>

namespace facts {

struct RelationSite {
  SymbolId source;
  SymbolId destination;
  RelationKind kind = RelationKind::Uses;
  std::uint16_t position = 0;
  FileId file = builtinFileId;
  Location location;
  std::optional<SymbolId> receiverType;
  std::optional<ReceiverCertainty> certainty;
};

} // namespace facts

#endif // FACTS_TOOL_MODEL_RELATION_SITE_H
