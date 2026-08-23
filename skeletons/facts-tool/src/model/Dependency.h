#pragma once

#include "model/SymbolId.h"

namespace facts {

struct DependencyEdge {
  FileId source;
  FileId destination;

  auto operator<=>(const DependencyEdge &) const = default;
};

} // namespace facts
