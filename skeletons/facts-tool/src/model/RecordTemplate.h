// model/RecordTemplate.h — a class template pattern: `template <class T> struct Box`.
//
// Derives from Record rather than repeating it: everything true of the pattern
// class (its body, its bases, its fields, final/abstract) is true here too, and
// a query that only cares about records can read the Record part and ignore the
// list. What it adds is the `template <...>` line — the slots as declared, with
// their names and defaults. What gets substituted into them is a
// RecordInstance.

#ifndef FACTS_TOOL_MODEL_RECORDTEMPLATE_H
#define FACTS_TOOL_MODEL_RECORDTEMPLATE_H

#include "model/Record.h"
#include "model/TemplateArgument.h"

#include <vector>

namespace facts {

struct RecordTemplate : Record {
  // The slots as written: names, kinds, and defaults.
  std::vector<TemplateArgument> templateArguments;
};

} // namespace facts

#endif // FACTS_TOOL_MODEL_RECORDTEMPLATE_H
