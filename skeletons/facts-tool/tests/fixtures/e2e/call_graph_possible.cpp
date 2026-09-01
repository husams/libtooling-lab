#include "call_graph.hpp"

namespace call_graph_fixture {

int Base::toString() const { return 0; }

int Base::log() const { return toString(); }

int X::toString() const { return 1; }

int Y::toString() const { return 2; }

int possibleCall(Base *value) { return value->log(); }

struct PossibleRoot {
  virtual int value() const { return 0; }

  int call() const { return value(); }
};

struct PossibleMid : PossibleRoot {
  int value() const override { return 1; }
};

struct PossibleLeaf final : PossibleMid {
  int value() const override { return 2; }
};

int transitivePossible(PossibleRoot *value) { return value->call(); }

struct ExactRoot {
  virtual int value() const { return 0; }

  int call() const { return value(); }
};

struct ExactMid : ExactRoot {
  int value() const override { return 1; }
};

struct ExactLeaf final : ExactMid {
  int value() const override { return 2; }
};

int transitiveExact() {
  ExactLeaf value;
  return value.call();
}

struct FallbackRoot {
  virtual int value() const { return 0; }

  int call() const { return value(); }
};

struct Inherited final : FallbackRoot {};

int inheritedExact() {
  Inherited value;
  return value.call();
}

} // namespace call_graph_fixture
