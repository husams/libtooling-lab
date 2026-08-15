#ifndef FACTS_TOOL_MODEL_INITIALIZER_H
#define FACTS_TOOL_MODEL_INITIALIZER_H

#include <cstdint>
#include <optional>
#include <string>

namespace facts {

enum class EvaluatedValueKind : std::uint8_t {
  Integer = 1,
  Floating,
  Boolean,
  String,
};

struct EvaluatedValue {
  EvaluatedValueKind kind;
  std::string value;
};

struct Initializer {
  std::string expression;
  std::optional<EvaluatedValue> evaluated;
};

} // namespace facts

#endif // FACTS_TOOL_MODEL_INITIALIZER_H
