// model/TemplateParameter.h — one value supplied to a template.
//
// The `int` and `std::string` of `Value<int, std::string>`. It has no name —
// names belong to the slots it fills, which are TemplateArguments on the
// pattern — and it is either a symbol it refers to or a literal it spells out.
//
// A value is written the same way a function parameter is, so it carries the
// same two things: the symbol its type names, and flags for everything wrapped
// around that symbol. `Value<int *, int &>` is two entries, both with a zero
// type because nothing declares `int`, one Pointer and one Reference.

#ifndef FACTS_TOOL_MODEL_TEMPLATEPARAMETER_H
#define FACTS_TOOL_MODEL_TEMPLATEPARAMETER_H

#include "model/Parameter.h" // ParameterMode, ParameterBit
#include "model/SymbolId.h"

#include <cstdint>
#include <string>

namespace facts {

enum class TemplateParameterKind : std::uint8_t {
  Type = 1, // `int`, `std::string`, `Widget *`
  NonType,  // a value: `4`, `Mode::Write`
  Template, // a template supplied to a template-template slot: `Box`
  Pack      // an expansion, one entry per element
};

struct TemplateParameter {
  // A non-type value as written — "4", "Mode::Write". Empty for type values,
  // whose content is the type below.
  std::string value;

  // The symbol the type names: the record for `Widget`, the template for
  // `Box`, the instance for `std::vector<int>` — which itself carries an
  // Instantiates edge back to `std::vector<T>`. Zero for builtins.
  SymbolId type;

  // The same vocabulary a Parameter uses, and for the same reason: const,
  // volatile, restrict, and a two-bit mode saying whether the value is taken
  // by value, by pointer, or by reference.
  std::uint32_t flags = 0;

  TemplateParameterKind kind = TemplateParameterKind::Type;

  // Which element of a pack this is, and -1 when it is not one. One declared
  // `class... Ts` slot can take any number of values, and without this they
  // would look like that many separate slots.
  std::int16_t packIndex = -1;
};

} // namespace facts

#endif // FACTS_TOOL_MODEL_TEMPLATEPARAMETER_H
