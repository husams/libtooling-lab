#ifndef FACTS_TOOL_MODEL_VARIABLE_H
#define FACTS_TOOL_MODEL_VARIABLE_H

#include "model/Initializer.h"
#include "model/Symbol.h"

#include <optional>

namespace facts {

struct Variable : Symbol {
  std::optional<Initializer> initializer;
};
} // namespace facts

#endif
