#include "relation_resolution.hpp"

// Field owners the registry has not persisted.
int regression::Box::value = 0;
int regression::BoxedPair::value = 0;

// A method owner the registry has not persisted.
unsigned long std::hash<regression::Hashable>::operator()(
    const regression::Hashable &value) const noexcept {
  return static_cast<unsigned long>(value.key);
}

namespace regression {

// A reference through an unnamed declaration: the anonymous union and the
// anonymous struct have no USR to key a use relation on, so the reference is
// skipped rather than escalated.
struct Reading {
  union {
    int asInteger;
    float asReal;
  };
  struct {
    int nested;
  };
  int named;
};

int readThrough(Reading &reading) {
  reading.asInteger = 1;
  reading.nested = 2;
  return reading.asInteger + reading.nested + reading.named;
}

} // namespace regression
