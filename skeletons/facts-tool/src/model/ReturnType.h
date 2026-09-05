#pragma once

#include "model/SymbolId.h"

#include <string>

namespace facts {

struct ReturnType {
  SymbolId target;
  std::string canonicalType;
  // Predefined FileId-0 targets need a row for the relation foreign key.
  std::string builtinName;
};

} // namespace facts
