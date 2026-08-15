#ifndef FACTS_TOOL_MODEL_ENUMERATOR_H
#define FACTS_TOOL_MODEL_ENUMERATOR_H

#include "model/Symbol.h"

#include <optional>
#include <string>

namespace facts {

struct Enumerator : Symbol {
  std::string value;
  std::optional<std::string> initializerExpression;
};

} // namespace facts

#endif
