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

int unresolved(int (*target)()) { return target(); }

} // namespace s022_fixture
