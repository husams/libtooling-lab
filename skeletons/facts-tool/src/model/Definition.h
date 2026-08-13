// model/Definition.h — where an entity's body is: which file, and which slice of it.
//
// The slice covers the whole thing, not its name and not its first line: for a
// record it runs from the `class` / `struct` / `union` keyword through the
// closing brace, and for a function from the return type through the closing
// brace of its body. That is what an editor folds, a viewer prints, and a
// rename has to rewrite.
//
// The file is here because it need not be the symbol's own: a record
// forward-declared in one header gets its body in another, and a method
// declared in a header is defined in a .cpp. Symbol::loc is where the entity
// was declared; this is where it was defined.
//
// A region of size 0 means no body was ever seen — `class Widget;` and nothing
// more.

#ifndef FACTS_TOOL_MODEL_DEFINITION_H
#define FACTS_TOOL_MODEL_DEFINITION_H

#include "model/Location.h"
#include "model/SymbolId.h"

namespace facts {

struct Definition {
  FileId file = 0;
  Region region;
};

} // namespace facts

#endif // FACTS_TOOL_MODEL_DEFINITION_H
