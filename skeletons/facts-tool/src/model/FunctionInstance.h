// model/FunctionInstance.h — a function template with its arguments bound.
//
// `identity<double>` is a callable of its own, with concrete parameter types
// and its own address. Its Function part holds the substituted signature, this
// holds what was substituted, and an Instantiates or Specializes edge points
// back at the pattern.
//
// Derives from FunctionTemplate for the same reason RecordInstance derives from
// RecordTemplate: the inherited templateArguments are the slots still open,
// which is empty for an ordinary instantiation and not for a declaration that
// supplies some values and leaves others deduced.

#ifndef FACTS_TOOL_MODEL_FUNCTIONINSTANCE_H
#define FACTS_TOOL_MODEL_FUNCTIONINSTANCE_H

#include "model/FunctionTemplate.h"
#include "model/TemplateParameter.h"

#include <vector>

namespace facts {

struct FunctionInstance : FunctionTemplate {
  std::vector<TemplateParameter> templateParameters;
};

} // namespace facts

#endif // FACTS_TOOL_MODEL_FUNCTIONINSTANCE_H
