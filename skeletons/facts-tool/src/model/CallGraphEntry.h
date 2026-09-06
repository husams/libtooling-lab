#pragma once

#include "model/SymbolId.h"

namespace facts {

struct CallGraphEntry {
  SymbolId symbolId;
  SymbolId graphNodeRef;
};

} // namespace facts
