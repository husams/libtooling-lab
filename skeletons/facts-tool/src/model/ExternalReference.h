#pragma once

#include "model/Relation.h"
#include "model/SymbolId.h"

#include <cstdint>

namespace facts {

struct ExternalReference {
  SymbolId source;
  SymbolId destination;
  RelationKind kind = RelationKind::Calls;
  std::uint16_t position = 0;
  FileId file = builtinFileId;
  std::uint32_t offset = 0;
  SymbolId externalSymbol;
};

using CallGraphExternalReference = ExternalReference;

} // namespace facts
