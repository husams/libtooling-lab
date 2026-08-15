#ifndef FACTS_TOOL_MODEL_VARIABLE_H
#define FACTS_TOOL_MODEL_VARIABLE_H

#include "model/Symbol.h"

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

struct VariableInitializer {
  std::string expression;
  std::optional<EvaluatedValue> evaluated;
};

struct Variable : Symbol {
  std::optional<VariableInitializer> initializer;
};
} // namespace facts

#endif
