#pragma once

#include "model/Location.h"
#include "model/SymbolId.h"

#include <optional>
#include <string>

namespace facts {

// The invocation is known even when its runtime function is not. target names
// the value declaration being invoked, never a guessed function declaration.
struct PointerCallSite {
  SymbolId source;
  std::optional<SymbolId> target;
  FileId file = builtinFileId;
  Location location;
  std::string signature;
  std::string expression;
};

} // namespace facts
