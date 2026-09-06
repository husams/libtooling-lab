#pragma once

#include "model/Location.h"
#include "model/SymbolId.h"

namespace facts {

struct UnresolvedCallSite {
  SymbolId source;
  FileId file = builtinFileId;
  Location location;
};

} // namespace facts
