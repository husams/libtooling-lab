#ifndef FACTS_TOOL_MODEL_ENUMERATION_H
#define FACTS_TOOL_MODEL_ENUMERATION_H

#include "model/Symbol.h"

namespace facts {
struct Enumeration : Symbol {
  SymbolId underlyingType;
  bool isScoped = false;
  bool hasFixedUnderlyingType = false;
};
} // namespace facts

#endif
