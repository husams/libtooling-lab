// model/RecordInstance.h — a class template with its arguments bound.
//
// `Value<int, std::string>` is a record in its own right: it has a body, its
// own members, its own size, and code names it directly. What it adds over a
// Record is the arguments it was built from, and an Instantiates edge — or
// Specializes, for a hand-written specialization — back to the pattern.
//
// Derives from RecordTemplate, and not from Record, because a partial
// specialization is both at once: `template <class T> class Buffer<T *, 1>`
// declares a slot T and fills two positions with `T *` and `1`. So the
// inherited templateArguments are the slots this declaration still leaves
// open, and templateParameters are the values it has already supplied. A full
// instantiation like `Buffer<Widget, 4>` leaves the inherited list empty.

#ifndef FACTS_TOOL_MODEL_RECORDINSTANCE_H
#define FACTS_TOOL_MODEL_RECORDINSTANCE_H

#include "model/RecordTemplate.h"
#include "model/TemplateParameter.h"

#include <vector>

namespace facts {

struct RecordInstance : RecordTemplate {
  // `int`, then `std::string` — in the order they were supplied, with a pack
  // expanded into consecutive entries carrying its packIndex.
  std::vector<TemplateParameter> templateParameters;
};

} // namespace facts

#endif // FACTS_TOOL_MODEL_RECORDINSTANCE_H
