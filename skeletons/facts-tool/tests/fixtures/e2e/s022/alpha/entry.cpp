#include "../beta/service.hpp"

namespace s022_fixture {

int run() {
  Derived value;
  auto callback = [] { return helper(); };
  return invoke(&value) + callback();
}

int exactDispatch() {
  Derived value;
  return value.value();
}

int heapCleanup() {
  auto *value = new Derived;
  auto result = invoke(value);
  delete value;
  return result;
}

int temporaryCleanup() { return Derived{}.value(); }

void explicitCleanup(Derived *value) { value->~Derived(); }

void mixedCleanup(Derived *other) {
  Derived value;
  other->~Derived();
}

void deleteBase(Base *value) { delete value; }

int implicitConstruction() {
  ImplicitValue value;
  return consume(value);
}

int unresolved(int (*target)()) { return target(); }

} // namespace s022_fixture
