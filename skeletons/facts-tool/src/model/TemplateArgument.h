// model/TemplateArgument.h — one slot declared on a template pattern.
//
// The `T`, `U`, `Z` of `template <typename T, typename U, typename Z>`. A type
// slot is a name and nothing else: the whole point of it is that its type is
// not known until something fills it, and what fills it is a TemplateParameter
// on the instance.
//
// A non-type slot is the exception. `template <int N>` is a slot that takes a
// value — 5, 6, 7 — and `int` is written right there in the declaration, so the
// slot does have a type of its own. `type` is that, and it is zero for the
// builtins nothing declares, which is most of them: int, bool, std::size_t.
// `template <Mode M>` fills it in.
//
// The flags say which of the three kinds of slot this is, and whether it takes
// one value or any number. A plain type slot sets none of them.

#ifndef FACTS_TOOL_MODEL_TEMPLATEARGUMENT_H
#define FACTS_TOOL_MODEL_TEMPLATEARGUMENT_H

#include "model/SymbolId.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace facts {

enum TemplateArgumentBit : std::size_t {
  ParameterPackBit = 0, // `class... Ts`, `int... Ns`
  NonTypeBit = 1,       // `int N`, `Mode M` — takes a value, not a type
  TemplateTemplateBit = 2 // `template <class> class C` — takes a template
};

struct TemplateArgument {
  std::string name; // "T", "U", "N"

  // A non-type slot's own type, and zero otherwise — including for the
  // builtins that declare nothing.
  SymbolId type;

  std::uint32_t flags = 0;
};

} // namespace facts

#endif // FACTS_TOOL_MODEL_TEMPLATEARGUMENT_H
