#include "call_graph.hpp"

namespace call_graph_fixture {

int Base::toString() const { return 0; }

int Base::log() const { return toString(); }

int X::toString() const { return 1; }

int Y::toString() const { return 2; }

int helper() { return 4; }

struct Owner {
  Owner() { helper(); }

  int method() const { return helper(); }
};

int directMethodLambdaAndConstructor() {
  Owner owner;
  auto lambda = [] { return helper(); };
  return helper() + owner.method() + lambda();
}

int exactCalls() {
  X x;
  Y y;
  return x.log() + y.log() + declarationOnly(3);
}

int recurse(int value) { return value ? recurse(value - 1) : exactCalls(); }

int externalRoot() { return externalOnly(1); }

int depthMiddle() { return externalRoot(); }

int depthRoot() { return depthMiddle(); }

} // namespace call_graph_fixture
