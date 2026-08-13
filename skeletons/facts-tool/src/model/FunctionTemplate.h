// model/FunctionTemplate.h — a function template pattern. What gets substituted into
// its slots is a FunctionInstance.

#ifndef FACTS_TOOL_MODEL_FUNCTIONTEMPLATE_H
#define FACTS_TOOL_MODEL_FUNCTIONTEMPLATE_H

#include "model/Function.h"
#include "model/TemplateArgument.h"

#include <vector>

namespace facts {

struct FunctionTemplate : Function {
  // The slots as written: names, kinds, and defaults.
  std::vector<TemplateArgument> templateArguments;
};

} // namespace facts

#endif // FACTS_TOOL_MODEL_FUNCTIONTEMPLATE_H
