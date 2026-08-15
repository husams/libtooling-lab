#ifndef FACTS_TOOL_MODEL_PARAMETER_H
#define FACTS_TOOL_MODEL_PARAMETER_H

#include "model/Initializer.h"
#include "model/Location.h"
#include "model/SymbolId.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace facts {

enum class ParameterBit : std::size_t {
  PointerBit,             // semantic parameter type is a pointer
  LValueReferenceBit,     // T &
  RValueReferenceBit,     // T &&, including forwarding references
  ForwardingReferenceBit, // cv-unqualified function-template T &&
  ConstBit,               // value or immediate pointee/referred type is const
  PackBit,                // template function parameter pack
};

inline constexpr std::uint8_t bit(ParameterBit position) {
  return std::uint8_t{1} << static_cast<std::size_t>(position);
}

struct Parameter {
  std::string name;
  SymbolId type;
  Location loc;
  Region region;
  // ParameterBit values may overlap; a forwarding reference is also rvalue.
  std::uint8_t flags = 0;
  bool hasDefault = false;
  std::optional<Initializer> defaultValue;
};

} // namespace facts

#endif // FACTS_TOOL_MODEL_PARAMETER_H
