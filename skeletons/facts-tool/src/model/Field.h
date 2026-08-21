#ifndef FACTS_TOOL_MODEL_FIELD_H
#define FACTS_TOOL_MODEL_FIELD_H

#include "model/Variable.h"

namespace facts {

// Fields share the existing variable storage node while retaining a distinct
// extraction model name at the AST boundary.
using Field = Variable;

} // namespace facts

#endif
