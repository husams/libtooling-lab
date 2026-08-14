#include "shared.hpp"

namespace e2e {

int transform(const Widget &widget, Count factor) {
  return headerHelper(widget.value * factor, sharedCounter);
}

int MethodFixture::outOfLineMethod(int value) const { return value; }

int useTwo(const Widget &widget) {
  return transform(widget, 4) + sizeof(StructTemplate<Widget, 7>) +
         sizeof(UnionTemplate<Widget, Policy>) + functionTemplate<Widget, 9>() +
         MethodTemplateFixture{}.methodTemplate<Widget>();
}

} // namespace e2e
