#pragma once

#include "model/SymbolId.h"

#include <cstdint>
#include <string>

namespace facts {

struct MatchedSymbol {
  std::string usr;
  std::string qualifiedName;
  FileId fileId = 0;
  std::int64_t kind = 0;
};

} // namespace facts
